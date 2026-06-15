#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${ROOT_DIR}/build/trimanga"

if [[ ! -x "${BINARY}" ]]; then
  echo "trimanga has not been built. Run: make"
  exit 1
fi

"${BINARY}" --help >/dev/null
"${BINARY}" --version >/dev/null

echo "smoke tests passed"
