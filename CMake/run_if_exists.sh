#!/bin/sh
# Small helper for running a command
out=""
if [ "$1" = "--stdout" ]; then
    out="$2"
    shift 2
fi

if [ -e "$1" ]; then
    shift
    if [ "$out" ]; then
        exec "$@" > $out
    else
        exec "$@"
    fi
fi
