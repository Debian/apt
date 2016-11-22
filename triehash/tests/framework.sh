#!/bin/sh
# Simple integration test framework

set -e


cleanup() {
    rm -f test.output test.c test.h test.tree
}

dumpone() {
    if [ -e "$@" ]; then
        echo "Content of $@:"
        cat "$@" | sed "s#^#\t#g"
    fi

}

dump() {
    dumpone test.output
    dumpone test.c
    dumpone test.h
    dumpone test.tree
    return 1
}

testsuccess() {
    [ "$INNER" ] || cleanup
    [ "$INNER" ] || echo "Testing success of $@"
    if ! "$@" > test.output 2>&1; then
        echo "ERROR: Running $@ failed with error $?, messages were:" >&2
        dump
        return 1
    fi
}

testfailure() {
    [ "$INNER" ] || cleanup
    [ "$INNER" ] || echo "Testing failure of $@"
    if "$@" > test.output 2>&1; then
        echo "ERROR: Running $@ unexpectedly succeeded, messages were:" >&2
        dump
        return 1
    fi
}

testfileequal() {
    [ "$INNER" ] ||  echo "Testing output of $2"
    printf "%b\n" "$1" > expected
    if ! diff -u "expected" "$2" > test.diff; then
        echo "ERROR: Differences between expected output and and $2:" >&2
        cat test.diff | sed "s#^#\t#g"
        dump
        return 1
    fi
}

testgrep() {
    [ "$INNER" ] ||  echo "Testing grep $@"
    INNER=1 testsuccess grep "$@"
    unset INNER
}

testsuccessequal() {
    expect="$1"
    shift
    cleanup
    echo "Testing success and output of $@"
    INNER=1 testsuccess "$@"
    INNER=1 testfileequal "$expect" test.output
    unset INNER
}


WORDS="Word-_0
Word = 42
VeryLongWord
Label ~ Word2
= -9"

triehash() {
    printf "%b\n" "$WORDS" | perl -MDevel::Cover=-summary,0,-silent,1 $(dirname $(dirname $(readlink -f $0)))/triehash.pl "$@" || return $?
    return $?
}
