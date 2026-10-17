#!/bin/bash
set -euo pipefail
TAILKANA_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$TAILKANA_PROJECT/scripts"
TAILKANA_PACKAGE="$(python3 -c 'from release import PACKAGE_NAME; print(PACKAGE_NAME)' 2>/dev/null)"
open "$TAILKANA_PROJECT/outputs/$TAILKANA_PACKAGE"
