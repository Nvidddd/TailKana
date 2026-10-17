#!/usr/bin/env python3
"""Compact offline word/forms index from pinned Chinese Wiktionary extracts."""
from pathlib import Path
import json,sqlite3,unicodedata,hashlib
from chinese_gloss import simplified_converter
root=Path(__file__).resolve().parents[1]
simplify,conversion=simplified_converter(root)
sources=[root/'work/language-data'/name for name in ['ja-zh.jsonl','ja-zh-simplified.jsonl']]
out=root/'assets/language';out.mkdir(parents=True,exist_ok=True)
p=out/'ja-zh.sqlite';temporary=p.with_suffix('.sqlite.building')
if temporary.exists():temporary.unlink()
db=sqlite3.connect(temporary)
db.executescript('''
CREATE TABLE entry(id INTEGER PRIMARY KEY,lemma TEXT,meaning TEXT,source TEXT,priority INTEGER,UNIQUE(lemma,meaning));
CREATE TABLE form(word TEXT,reading TEXT,entry_id INTEGER,PRIMARY KEY(word,reading,entry_id)) WITHOUT ROWID;
CREATE TABLE alias(word TEXT,target TEXT,PRIMARY KEY(word,target)) WITHOUT ROWID;
CREATE VIEW gloss AS SELECT f.word,f.reading,e.meaning,e.source,e.priority FROM form f JOIN entry e ON e.id=f.entry_id;
''')
def norm(s):return unicodedata.normalize('NFKC',s)
def kana(s):return ''.join(chr(ord(c)-96) if '\u30a1'<=c<='\u30f6' else c for c in norm(s))
def is_kana(s):return bool(s) and all('\u3040'<=c<='\u30ff' or c in '・ー ' for c in s)
def reading(form):
 if form.get('hiragana'):return kana(form['hiragana'])
 text=form.get('form','')
 # Ruby only describes the kanji portions. Keep the unannotated okurigana.
 for pair in form.get('ruby') or []:
  if isinstance(pair,list) and len(pair)>1:text=text.replace(pair[0],pair[1],1)
 return kana(text) if is_kana(text) else ''
for source in sources:
 for line in source.open():
  d=json.loads(line)
  if d.get('lang_code')!='ja':continue
  word=norm(d.get('word',''))
  if not word:continue
  forms={(word,kana(word) if is_kana(word) else '')}
  for sound in d.get('sounds') or []:
   value=sound.get('other','')
   if is_kana(value):forms.add((word,kana(value)))
  for f in d.get('forms') or []:
   surface=norm(f.get('form',''))
   if surface and not any(c in surface for c in '¹²³⁴⁵'):
    sound=reading(f);forms.add((surface,sound))
    if d.get('pos')=='verb' and 'continuative' in f.get('tags',[]):
     for suffix in ['ます','ました','ません','ませんでした','ましょう','たい','たかった','たくない']:
      forms.add((surface+suffix,sound+suffix if sound else ''))
  for surface,sound in list(forms):
   if len(sound)>=3 and is_kana(sound):forms.add((sound,sound))
  for sense in d.get('senses') or []:
   for target in (sense.get('form_of') or [])+(sense.get('alt_of') or []):
    if target.get('word'):db.execute('INSERT OR IGNORE INTO alias VALUES (?,?)',(word,norm(target['word'])))
   glosses=sense.get('glosses') or []
   if not glosses:continue
   meaning=simplify('；'.join(dict.fromkeys(glosses)))[:1200]
   if not meaning:continue
   priority=0
   if d.get('pos') in ['name','character']:priority+=10
   if meaning in ['姓氏','女性人名','男性人名']:priority+=20
   if any(t in sense.get('tags',[]) for t in ['archaic','dialectal','obsolete']):priority+=15
   if d.get('pos')=='proverb' and sum('\u3040'<=c<='\u30ff' for c in meaning)>4:priority+=40
   db.execute('INSERT OR IGNORE INTO entry(lemma,meaning,source,priority) VALUES (?,?,?,?)',(word,meaning,word,priority))
   identifier=db.execute('SELECT id FROM entry WHERE lemma=? AND meaning=?',(word,meaning)).fetchone()[0]
   db.executemany('INSERT OR IGNORE INTO form VALUES (?,?,?)',[(surface,sound,identifier) for surface,sound in forms])
db.commit()
# Build-time deduplication indices need not take space in the read-only runtime.
db.executescript("""
DROP VIEW gloss;
CREATE TABLE compact_entry(id INTEGER PRIMARY KEY,lemma TEXT,meaning TEXT,priority INTEGER);
INSERT INTO compact_entry SELECT id,lemma,meaning,priority FROM entry;
DROP TABLE entry;
ALTER TABLE compact_entry RENAME TO entry;
CREATE VIEW gloss AS SELECT f.word,f.reading,e.meaning,e.lemma AS source,e.priority,e.id AS entryid FROM form f JOIN entry e ON e.id=f.entry_id;
""")
db.commit();db.execute('VACUUM')
report={'schemaVersion':3,'generatorSha256':hashlib.sha256(Path(__file__).read_bytes()+(root/'scripts/chinese_gloss.py').read_bytes()).hexdigest(),'conversion':conversion,'meaningLanguage':'zh-Hans','sources':['https://kaikki.org/zhwiktionary/日語/index.html','https://kaikki.org/zhwiktionary/日语/index.html'],
 'dumpSha256':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},
 'entries':db.execute('select count(*) from entry').fetchone()[0],
 'words':db.execute('select count(distinct lemma) from entry').fetchone()[0],
 'surfaceForms':db.execute('select count(distinct word) from form').fetchone()[0],
 'license':'CC-BY-SA-4.0; Chinese Wiktionary contributors; each source links to its word page',
 'changes':'Merged Chinese-edition Japanese extracts; Chinese meanings converted to Simplified Chinese with OpenCC t2s; Japanese word forms/readings/source links preserved; definitions deduplicated.'}
db.close();temporary.replace(p);report['bytes']=p.stat().st_size
(out/'glossary-source.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
(out/'Wiktionary-NOTICE.txt').write_text('Japanese→Chinese definitions: Chinese Wiktionary contributors, extracted by Wiktextract / Kaikki.org.\nSources: https://kaikki.org/zhwiktionary/日語/index.html and https://kaikki.org/zhwiktionary/日语/index.html\nWord-level attribution: https://zh.wiktionary.org/wiki/<source word>#日語, accessible through the candidate panel.\nCC-BY-SA-4.0: https://creativecommons.org/licenses/by-sa/4.0/\nModified: Chinese meanings converted to Simplified Chinese (OpenCC t2s); merged, normalized and indexed lexical/inflected word forms, definitions deduplicated; examples and media omitted. Derived glossary remains CC-BY-SA-4.0.\n')
print(json.dumps(report,ensure_ascii=False,indent=2))
