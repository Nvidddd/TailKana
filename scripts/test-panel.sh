#!/bin/bash
set -euo pipefail
TAILKANA_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$TAILKANA_PROJECT"
python3 scripts/prepare.py
work/tooling/node_modules/.bin/pnpm --dir work/fcitx5-macos/fcitx5-webview run check
work/tooling/node_modules/.bin/pnpm --dir work/fcitx5-macos/fcitx5-webview exec playwright test tests/tailkana.spec.ts --workers=1
