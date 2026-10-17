#!/usr/bin/env python3
"""Fetch hash-pinned public data; never writes system/user IME configuration."""
from pathlib import Path
import hashlib,json,urllib.request,shutil,subprocess
root=Path(__file__).resolve().parents[1]
folder=root/'work/language-data';folder.mkdir(parents=True,exist_ok=True)
manifest=json.loads((root/'upstream-lock.json').read_text())
lock=manifest['language_data']|{x['filename']:x for x in manifest['build_tools'].values()}
for name,item in lock.items():
 path=folder/name
 if not path.exists():
  temporary=path.with_suffix(path.suffix+'.download')
  with urllib.request.urlopen(item['url']) as response,temporary.open('wb') as output:shutil.copyfileobj(response,output)
  if hashlib.sha256(temporary.read_bytes()).hexdigest()!=item['sha256']:raise SystemExit('Downloaded data changed: '+name)
  temporary.replace(path)
 if hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:raise SystemExit('Data checksum mismatch: '+name)
extracted=folder/'extracted';extracted.mkdir(exist_ok=True)
for archive,member in [('dict-20260907.tar.zst','dict_sc.txt'),('lm_sc.arpa-20260629.tar.zst','lm_sc.arpa')]:
 if not (extracted/member).exists():subprocess.run(['tar','-xf',str(folder/archive),'-C',str(extracted),member],check=True)
metadata=root/'assets/language/glossary-source.json'
current=json.loads(metadata.read_text()) if metadata.exists() else {}
generator=root/'scripts/build-glossary.py'
if current.get('conversion',{}).get('sha256')!=manifest['build_tools']['opencc']['sha256'] or not (root/'assets/language/ja-zh.sqlite').exists() or current.get('generatorSha256')!=hashlib.sha256(generator.read_bytes()+(root/'scripts/chinese_gloss.py').read_bytes()).hexdigest():subprocess.run(['python3',str(generator)],check=True)
print('Pinned language data verified.')
