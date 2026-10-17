# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit setup only. Pin every model by revision, length and SHA256."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
directory = root/'work/ai-experiment/models'
directory.mkdir(parents=True, exist_ok=True)
items = json.loads((Path(__file__).parent/'model-lock.json').read_text())
for item in items:
    path = directory/item['file']
    def valid(p):
        return p.exists() and p.stat().st_size == item['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest() == item['sha256']
    if not valid(path):
        url = f"https://huggingface.co/{item['repo']}/resolve/{item['revision']}/{item['source']}?download=true"
        temporary = path.with_suffix('.partial')
        subprocess.run(['curl', '--fail', '--location', '--retry', '4', '--connect-timeout', '20',
                        '--continue-at', '-', url, '-o', str(temporary)], check=True)
        if not valid(temporary):
            raise RuntimeError('下载校验失败：'+item['file'])
        temporary.replace(path)
    print('Verified', item['file'], flush=True)
(directory/'manifest.json').write_text(json.dumps(items, indent=2)+'\n')
