#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/../.."
export PATH="$PWD/work/tooling/bin:$PWD/work/tooling/node_modules/.bin:$PATH"
uv --cache-dir work/ai-experiment/uv-cache venv work/ai-experiment/venv --python 3.12 --allow-existing
uv --cache-dir work/ai-experiment/uv-cache pip install --python work/ai-experiment/venv/bin/python -r experimental/ai/requirements.txt
work/ai-experiment/venv/bin/python experimental/ai/fetch_models.py
if [[ ! -d work/ai-experiment/llama.cpp/.git ]]; then
  git clone https://github.com/azooKey/llama.cpp.git work/ai-experiment/llama.cpp
fi
git -C work/ai-experiment/llama.cpp checkout --detach 88b97a47dc7f5892e2d5a6856fbe9cfe237f9e5c
cmake -S experimental/ai -B work/ai-experiment/native-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build work/ai-experiment/native-build --target tailkana-zenz -j 6
python3 scripts/prepare.py
cd work/fcitx5-macos
./scripts/patch.sh
cmake -B build/arm64 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTAILKANA_AI_TRIAL=ON -DKEY_LOGGING=OFF
cmake --build build/arm64 --target tailkana-ai-candidates -j 6
cd ../..
work/ai-experiment/venv/bin/python experimental/ai/models.py
