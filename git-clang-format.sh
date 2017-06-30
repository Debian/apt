#!/bin/sh
cd "$(dirname "$0")"
git clang-format-3.8 --diff "$@" | sed "s#+/\*\}\}\}\*/#+									/*}}}*/#" | patch -p1
