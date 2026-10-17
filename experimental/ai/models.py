# SPDX-License-Identifier: GPL-3.0-or-later
"""Lazy, offline-only model runners for the isolated TailKana trial."""
from __future__ import annotations
import functools
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import time

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'work/ai-experiment'
MODELS = DATA / 'models'
DATABASE = ROOT / 'assets/language/ja-zh.sqlite'


def katakana(text: str) -> str:
    return ''.join(chr(ord(c) + 0x60) if '\u3041' <= c <= '\u3096' else c for c in text)


def preprocess(text: str) -> str:
    # Same normalization as azooKey's ZenzContext.preprocessText.
    return text.replace(' ', '\u3000').replace('\n', '').replace('\r', '')


class Zenz:
    def __init__(self):
        import subprocess
        self.process = subprocess.Popen([str(DATA/'native-build/tailkana-zenz'), str(MODELS/'zenz.gguf')],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, text=True)

    def close(self):
        self.rank.cache_clear()
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except __import__('subprocess').TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdin.close()
        self.process.stdout.close()

    @functools.lru_cache(maxsize=64)
    def rank(self, context: str, reading: str, choices: tuple[str, ...]):
        import select
        prefix = ('\uee02' + preprocess(context[-40:]) if context else '')
        prefix += '\uee00' + preprocess(katakana(reading)) + '\uee01'
        self.process.stdin.write(json.dumps({'prompt': prefix, 'choices': [preprocess(c) for c in choices]})+'\n')
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], 30)[0]:
            self.close()
            raise RuntimeError('Zenz 响应超时')
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError('Zenz 后端退出')
        result = json.loads(line)
        if 'error' in result:
            raise RuntimeError(result['error'])
        # Display original candidate spelling even when spaces were normalized.
        mapping = {preprocess(c): c for c in choices}
        return [dict(r, text=mapping[r['text']]) for r in result['ranked']]


class E5:
    def __init__(self):
        os.environ.setdefault('TOKENIZERS_PARALLELISM', 'false')
        import onnxruntime as ort
        from tokenizers import Tokenizer
        options = ort.SessionOptions()
        options.intra_op_num_threads = 4
        options.inter_op_num_threads = 1
        self.session = ort.InferenceSession(str(MODELS/'e5.onnx'), options,
                                           providers=['CPUExecutionProvider'])
        self.tokenizer = Tokenizer.from_file(str(MODELS/'tokenizer.json'))
        self.tokenizer.enable_truncation(max_length=96)
        self.tokenizer.enable_padding(pad_id=1, pad_token='<pad>')

    def embed(self, texts):
        import numpy as np
        encoded = self.tokenizer.encode_batch(texts)
        ids = np.asarray([x.ids for x in encoded], dtype=np.int64)
        mask = np.asarray([x.attention_mask for x in encoded], dtype=np.int64)
        types = np.zeros_like(ids)
        values = {'input_ids': ids, 'attention_mask': mask, 'token_type_ids': types}
        output = self.session.run(None, {i.name: values[i.name] for i in self.session.get_inputs()})[0]
        pooled = (output * mask[:, :, None]).sum(axis=1) / mask.sum(axis=1)[:, None]
        return (pooled / np.maximum(np.linalg.norm(pooled, axis=1, keepdims=True), 1e-12)).astype(np.float32)


class SemanticIndex:
    def __init__(self):
        import numpy as np
        self.index_dir = DATA/'index'
        self.manifest = json.loads((self.index_dir/'manifest.json').read_text())
        if self.manifest['database_sha256'] != hashlib.sha256(DATABASE.read_bytes()).hexdigest():
            raise RuntimeError('词典已更新，请重新建立 AI 索引')
        self.entries = json.loads((self.index_dir/'entries.json').read_text())
        self.vectors = np.load(self.index_dir/'vectors.npy', mmap_mode='r')
        if self.vectors.shape != (len(self.entries), 384):
            raise RuntimeError('AI 索引尺寸不正确')
        self.encoder = E5()

    @functools.lru_cache(maxsize=64)
    def search(self, query: str):
        import numpy as np
        vector = self.encoder.embed(['query: '+query])[0]
        # float16 mmap holds 112k definitions without making another full RAM copy.
        scores = np.empty(len(self.entries), dtype=np.float32)
        for start in range(0, len(scores), 4096):
            scores[start:start+4096] = self.vectors[start:start+4096].astype(np.float32) @ vector
        combined = scores.copy()
        # Known Chinese terms benefit from exact dictionary lookup. Long free
        # descriptions still use neural similarity; never fabricate translations.
        if len(query) <= 12:
            for i, entry in enumerate(self.entries):
                meanings = entry['meaning'].split('；')
                if query in meanings:
                    combined[i] += 0.12
                elif query in entry['meaning']:
                    combined[i] += 0.025 + 0.04*len(query)/max(len(entry['meaning']), 1)
        indices = np.argsort(combined)[-24:][::-1]
        return [dict(self.entries[int(i)], score=round(float(scores[i]), 5)) for i in indices]


class SemanticSearch:
    def __init__(self):
        import subprocess
        import sys
        self.process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--search-worker'],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, text=True)

    def close(self):
        self.search.cache_clear()
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except __import__('subprocess').TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdin.close()
        self.process.stdout.close()

    @functools.lru_cache(maxsize=64)
    def search(self, query: str):
        import select
        self.process.stdin.write(json.dumps({'query': query})+'\n')
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], 30)[0]:
            self.close()
            raise RuntimeError('中文找词响应超时')
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError('中文找词后端退出')
        result = json.loads(line)
        if 'error' in result:
            raise RuntimeError('中文找词暂不可用')
        return result['results']


def search_worker():
    import sys
    index = SemanticIndex()
    for line in sys.stdin:
        try:
            request = json.loads(line)
            query = request['query']
            if not isinstance(query, str) or not 1 <= len(query.strip()) <= 200:
                raise ValueError('invalid query')
            print(json.dumps({'results': index.search(query.strip())}, ensure_ascii=False), flush=True)
        except Exception:
            print(json.dumps({'error': 'search failed'}), flush=True)


def build_index():
    import numpy as np
    directory = DATA/'index'
    directory.mkdir(parents=True, exist_ok=True)
    manifest_path = directory/'manifest.json'
    database_hash = hashlib.sha256(DATABASE.read_bytes()).hexdigest()
    model_manifest = json.loads((MODELS/'manifest.json').read_text())
    if manifest_path.exists():
        existing = json.loads(manifest_path.read_text())
        if (existing.get('database_sha256') == database_hash and
                existing.get('encoder', {}).get('sha256') == model_manifest[1]['sha256'] and
                all((directory/name).exists() for name in ('entries.json', 'vectors.npy'))):
            print('Existing offline index matches dictionary and encoder', flush=True)
            return
    connection = sqlite3.connect(f'file:{DATABASE}?mode=ro', uri=True)
    # Each lemma gets one vector: duplicate inflections and source definitions
    # must not crowd out independent suggestions.
    rows = connection.execute('SELECT lemma,meaning FROM entry ORDER BY priority,id')
    definitions = {}
    for word, meaning in rows:
        meanings = definitions.setdefault(word, [])
        if meaning not in meanings and len(meanings) < 4:
            meanings.append(meaning)
    readings = dict(connection.execute("SELECT word,MIN(reading) FROM form WHERE reading<>'' GROUP BY word"))
    connection.close()
    entries = [{'word': w, 'reading': readings.get(w, ''), 'meaning': '；'.join(m)[:220]}
               for w, m in definitions.items()]
    entries_path = directory/'entries.json'
    entries_path.write_text(json.dumps(entries, ensure_ascii=False, separators=(',', ':')))
    fingerprint = hashlib.sha256(entries_path.read_bytes() + model_manifest[1]['sha256'].encode()).hexdigest()
    progress_file = directory/'progress.json'
    completed = 0
    if progress_file.exists():
        progress = json.loads(progress_file.read_text())
        if progress.get('fingerprint') == fingerprint:
            completed = progress['completed']
    path = directory/'vectors.partial.npy'
    vectors = np.lib.format.open_memmap(path, mode='r+' if completed else 'w+', dtype=np.float16,
                                       shape=(len(entries), 384))
    encoder = E5()
    start_time = time.monotonic()
    for start in range(completed, len(entries), 48):
        batch = entries[start:start+48]
        vectors[start:start+len(batch)] = encoder.embed(['passage: '+e['word']+'：'+e['meaning'] for e in batch])
        vectors.flush()
        done = start+len(batch)
        progress_file.write_text(json.dumps({'fingerprint': fingerprint, 'completed': done}))
        if start % 480 == 0 or done == len(entries):
            print(f'{done}/{len(entries)} {(done-completed)/(time.monotonic()-start_time):.1f} entries/s', flush=True)
    del vectors
    path.replace(directory/'vectors.npy')
    (directory/'manifest.json').write_text(json.dumps({
        'entries': len(entries), 'dimensions': 384, 'dtype': 'float16',
        'database_sha256': hashlib.sha256(DATABASE.read_bytes()).hexdigest(),
        'encoder': json.loads((MODELS/'manifest.json').read_text())[1],
        'format': 'e5 query:/passage:, masked mean pooling, L2 normalization, max_length=96'
    }, indent=2)+'\n')
    print('Index ready', flush=True)


if __name__ == '__main__':
    import sys
    if sys.argv[1:] == ['--search-worker']:
        search_worker()
    else:
        build_index()
