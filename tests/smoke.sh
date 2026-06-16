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

TEST_DIR="$(mktemp -d)"
trap 'rm -rf "${TEST_DIR}"' EXIT

python3 - "${TEST_DIR}/sample.cbz" <<'PY'
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("keep/001.jpg", b"keep-one")
    archive.writestr("drop/002.jpg", b"drop-two")
    archive.writestr("keep/003.png", b"keep-three")
PY

cc -O2 -I"${ROOT_DIR}/third_party/miniz" -c "${ROOT_DIR}/third_party/miniz/miniz.c" -o "${TEST_DIR}/miniz.o"
cc -O2 -I"${ROOT_DIR}/third_party/miniz" -c "${ROOT_DIR}/third_party/miniz/miniz_tdef.c" -o "${TEST_DIR}/miniz_tdef.o"
cc -O2 -I"${ROOT_DIR}/third_party/miniz" -c "${ROOT_DIR}/third_party/miniz/miniz_tinfl.c" -o "${TEST_DIR}/miniz_tinfl.o"
cc -O2 -I"${ROOT_DIR}/third_party/miniz" -c "${ROOT_DIR}/third_party/miniz/miniz_zip.c" -o "${TEST_DIR}/miniz_zip.o"

c++ -std=c++20 -O2 \
  -I"${ROOT_DIR}/include" \
  -I"${ROOT_DIR}/third_party/miniz" \
  "${ROOT_DIR}/tests/archive_rewrite.cpp" \
  "${ROOT_DIR}/src/file_utils.cpp" \
  "${TEST_DIR}/miniz.o" \
  "${TEST_DIR}/miniz_tdef.o" \
  "${TEST_DIR}/miniz_tinfl.o" \
  "${TEST_DIR}/miniz_zip.o" \
  -o "${TEST_DIR}/archive_rewrite"

"${TEST_DIR}/archive_rewrite" "${TEST_DIR}/sample.cbz" "drop/002.jpg"

python3 - "${TEST_DIR}/sample.cbz" <<'PY'
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    names = set(archive.namelist())
assert names == {"keep/001.jpg", "keep/003.png"}, names
PY

echo "smoke tests passed"
