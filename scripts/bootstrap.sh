#!/bin/bash
set -euo pipefail
TAILKANA_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$TAILKANA_PROJECT"
mkdir -p work
python3 - <<'PY'
import json,subprocess
from pathlib import Path
lock=json.loads(Path('upstream-lock.json').read_text())
for name in ('fcitx5-macos','fcitx5-mozc','libime'):
    item=lock[name];path=Path(item['path'])
    if not path.exists():
        subprocess.run(['git','clone','--no-checkout',item['url'],str(path)],check=True)
        subprocess.run(['git','-C',str(path),'checkout',item['commit']],check=True)
        subprocess.run(['git','-C',str(path),'submodule','update','--init','--recursive'],check=True)
PY
mkdir -p work/tooling/bin
ln -sf "$(command -v python3)" work/tooling/bin/python
if [[ ! -x work/tooling/node_modules/.bin/pnpm ]]; then npm install --prefix work/tooling pnpm; fi
work/tooling/node_modules/.bin/pnpm --dir work/fcitx5-macos/fcitx5-webview install
(cd work/fcitx5-macos; mkdir -p cache; bash scripts/install-deps.sh; zsh scripts/patch.sh)
mkdir -p work/fcitx5-macos/build/arm64/tailkana-mozc/data_manager/oss
TAILKANA_DATA="work/fcitx5-macos/build/arm64/tailkana-mozc/data_manager/oss/mozc_data.inc"
if [[ ! -f "$TAILKANA_DATA" && -f work/source-data/mozc_data.inc ]]; then cp work/source-data/mozc_data.inc "$TAILKANA_DATA"; fi
if [[ ! -f "$TAILKANA_DATA" ]]; then curl -fL --retry 2 https://github.com/fcitx-contrib/fcitx5-mozc/releases/download/latest/mozc_data.inc -o "$TAILKANA_DATA"; fi
python3 - <<'PY'
import json,hashlib
from pathlib import Path
lock=json.loads(Path('upstream-lock.json').read_text())
p=Path('work/fcitx5-macos/build/arm64/tailkana-mozc/data_manager/oss/mozc_data.inc')
if hashlib.sha256(p.read_bytes()).hexdigest()!=lock['mozc_data']['sha256']:
    raise SystemExit('Upstream dictionary changed. Verify it against the pinned Mozc version before rebuilding.')
PY

python3 "$TAILKANA_PROJECT/scripts/fetch-language-data.py"
python3 "$TAILKANA_PROJECT/scripts/prepare-ai.py"
