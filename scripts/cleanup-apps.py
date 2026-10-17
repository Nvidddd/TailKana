#!/usr/bin/env python3
"""Remove this project's generated app bundles and their LaunchServices records.
Never touches the installed /Applications or /Library/Input Methods copies.
"""
from pathlib import Path
import subprocess, shutil, re, json
root=Path(__file__).resolve().parents[1]
registry='/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'
paths={p for p in (root/'work').rglob('*.app') if p.name in ('TailKana.app','TailKanaInput.app','TailKanaPreview.app','Fcitx5.app','FcitxTestApp.app')}
dump=subprocess.run([registry,'-dump'],capture_output=True,text=True,check=True).stdout
for match in re.finditer(r'^path:\s+(.+?\.app)(?: \(0x[^)]+\))?$',dump,re.M):
 p=Path(match.group(1))
 if p.is_relative_to(root/'work') and p.name in ('TailKana.app','TailKanaInput.app','TailKanaPreview.app','Fcitx5.app','FcitxTestApp.app'):paths.add(p)
removed=[]
for p in sorted(paths,key=lambda p:len(p.parts),reverse=True):
 assert p.is_relative_to(root/'work') and p.suffix=='.app'
 subprocess.run([registry,'-u',str(p)],capture_output=True,text=True)
 if p.exists():shutil.rmtree(p)
 removed.append(str(p))
for p in (root/'work').glob('tailkana-package-check-*'):
 if p.is_dir():shutil.rmtree(p)
for p in (root/'work').glob('tailkana-package-check-*'):assert not p.exists()
for folder in ('installer/root','stage','packaged'):
 p=root/'work'/folder
 if p.exists():shutil.rmtree(p)
report={'removedGeneratedBundles':removed,'keptInstalledApplications':['/Applications/TailKana.app','/Library/Input Methods/TailKanaInput.app']}
(root/'work/cleanup-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
print(f'Removed {len(removed)} generated/stale app copies; installed copies preserved.')
