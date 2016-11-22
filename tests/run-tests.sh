#!/bin/sh
DIR=$(dirname $(readlink -f $0))

# Let's go into triehash.pl's dir
cd $(dirname "$DIR")

rm -rf cover_db

count=$(cd "$DIR" && echo test-* | wc -w)
i=1

for test in $DIR/test-*; do
    echo "[$i/$count] Running testcase $test"
    if ! $test > test.summary 2>&1; then
        cat test.summary
        exit 1
    fi
    i=$((i + 1))
done


cover
