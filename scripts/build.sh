#!/bin/bash
set -euo pipefail
TAILKANA_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$TAILKANA_PROJECT/work/tooling/bin:$TAILKANA_PROJECT/work/tooling/node_modules/.bin:$PATH"
export CLANG_MODULE_CACHE_PATH="$TAILKANA_PROJECT/work/fcitx5-macos/build/module-cache"
python3 "$TAILKANA_PROJECT/scripts/prepare.py"
cd "$TAILKANA_PROJECT/work/fcitx5-macos"
mkdir -p build
pnpm --dir=fcitx5-webview run check > build/panel-typecheck.log 2>&1
cmake -B build/arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKEY_LOGGING=OFF \
  -DCMAKE_Swift_FLAGS="-module-cache-path $CLANG_MODULE_CACHE_PATH" > build/configure.log 2>&1
cmake --build build/arm64 --target tailkana-core-test tailkana-backend-test tailkana-engine-test tailkana-language-test tailkana-policy-test tailkana-ai-engine-test TailKanaAI TailKanaPreview Fcitx5 -j 6 > build/tailkana-build.log 2>&1
ctest --test-dir build/arm64 -R '^tailkana-' --output-on-failure
