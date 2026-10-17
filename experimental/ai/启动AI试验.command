#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/../.."
TAILKANA_AI_PYTHON="$PWD/work/ai-experiment/venv/bin/python"
if [[ ! -x "$TAILKANA_AI_PYTHON" || ! -f work/ai-experiment/index/manifest.json ]]; then
  print 'AI 试验资源尚未准备好，请先运行 experimental/ai/setup.sh。'
  read -k 1
  exit 1
fi
exec "$TAILKANA_AI_PYTHON" experimental/ai/launch.py
