#!/bin/sh
git clang-format-3.8 --diff "$@" | sed "s#+/\*\}\}\}\*/#+									/*}}}*/#" | patch -p1
