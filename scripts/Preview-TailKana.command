#!/bin/bash
set -euo pipefail
TAILKANA_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
open "$TAILKANA_PROJECT/work/packaged.noindex/TailKanaPreview.app"
