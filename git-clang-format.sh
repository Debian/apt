#!/bin/sh
cd "$(dirname "$0")"
CLANG_FORMAT="$(find $(echo "$PATH" | tr ':' ' ') -name 'git-clang-format-*' | sort | tail -n1 )"
if [ -z "$CLANG_FORMAT" ]; then
   echo >&2 'Could not find a clang-format to use. Is the package clang-format installed?'
   exit 1
fi
git "$(basename "$CLANG_FORMAT" | cut -d'-' -f 2-)" --diff "$@" | \
   sed "s#+/\*\}\}\}\*/#+									/*}}}*/#" | \
   patch -p1
