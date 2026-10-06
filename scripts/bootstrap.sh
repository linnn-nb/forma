#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ ! -x .tools/venv/bin/cmake ]; then
  python3 -m venv .tools/venv
  .tools/venv/bin/python -m pip install --disable-pip-version-check cmake==3.31.6 ninja==1.11.1.4
fi
export PATH="$PWD/.tools/venv/bin:$PATH"
cmake --preset release
cmake --build --preset release
ctest --preset release
