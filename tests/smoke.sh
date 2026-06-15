#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${ROOT_DIR}/build/scanlation_tool"

if [[ ! -x "${BINARY}" ]]; then
  echo "scanlation_tool has not been built. Run: make"
  exit 1
fi

"${BINARY}" --help >/dev/null
"${BINARY}" --version >/dev/null

echo "smoke tests passed"
