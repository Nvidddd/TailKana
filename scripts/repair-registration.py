#!/usr/bin/env python3
"""Stage the connection-name repair from the installed runtime; never install it.

installer.py packages this with the launcher. verify-installer.py removes stages.
"""
from pathlib import Path
import hashlib, plistlib, shutil, subprocess
root = Path(__file__).resolve().parents[1]
installed = Path('/Library/Input Methods/TailKanaInput.app')
before = plistlib.loads((installed / 'Contents/Info.plist').read_bytes())
assert before['CFBundleIdentifier'] == 'org.tailkana.inputmethod.TailKana'
assert before['CFBundleShortVersionString'] in ('0.2.4', '0.2.5', '0.2.6', '0.2.7')
app = root / 'work/packaged.noindex/TailKanaInput.app'
app.parent.mkdir(parents=True, exist_ok=True)
if app.exists(): shutil.rmtree(app)
shutil.copytree(installed, app, symlinks=True)
info = dict(before)
info.update(CFBundleVersion='9', CFBundleShortVersionString='0.2.7',
            InputMethodConnectionName=before['CFBundleIdentifier'] + '_Connection')
info['TISInputSourceID'] = info['CFBundleIdentifier']
source = info['CFBundleIdentifier'] + '.japanese'
modes = info['ComponentInputModeDict']['tsInputModeListKey']
mode = next(iter(modes.values()))
mode['TISInputSourceID'] = source
info['ComponentInputModeDict'] = {'tsInputModeListKey': {source: mode}, 'tsVisibleInputModeOrderedArrayKey': [source]}
(app / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
for localized in (app / 'Contents/Resources').glob('*.lproj/InfoPlist.strings'):
    text = localized.read_text(encoding='utf-16').replace('org.tailkana.inputmethod.japanese', source)
    localized.write_bytes(b'\xff\xfe' + text.encode('utf-16-le'))
binary = Path('Contents/MacOS/Fcitx5')
assert hashlib.sha256((app / binary).read_bytes()).digest() == hashlib.sha256((installed / binary).read_bytes()).digest()
subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
print('Staged connection-name repair; runtime code unchanged, signature refreshed.')
