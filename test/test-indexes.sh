#!/bin/sh -e

# Test behaviour of index retrieval and usage, in particular with uncompressed
# and gzip compressed indexes.
# Author: Martin Pitt <martin.pitt@ubuntu.com>
# (C) 2010 Canonical Ltd.

BUILDDIR=$(readlink -f $(dirname $0)/../build)

TEST_SOURCE="http://ftp.debian.org/debian unstable contrib"
GPG_KEYSERVER=gpg-keyserver.de
# should be a small package with dependencies satisfiable in TEST_SOURCE, i. e.
# ideally no depends at all
TEST_PKG="python-psyco-doc"

export LD_LIBRARY_PATH=$BUILDDIR/bin

OPTS="-qq -o RootDir=. -o Dir::Bin::Methods=$BUILDDIR/bin/methods -o Debug::NoLocking=true"
DEBUG=""
#DEBUG="-o Debug::pkgCacheGen=true"
#DEBUG="-o Debug::pkgAcquire=true"
APT_GET="$BUILDDIR/bin/apt-get $OPTS $DEBUG"
APT_CACHE="$BUILDDIR/bin/apt-cache $OPTS $DEBUG"

[ -x "$BUILDDIR/bin/apt-get" ] || {
    echo "please build the tree first" >&2
    exit 1
}

check_update() {
    echo "--- apt-get update $@ (no trusted keys)"

    rm -f etc/apt/trusted.gpg etc/apt/secring.gpg
    touch etc/apt/trusted.gpg etc/apt/secring.gpg
    out=$($APT_GET "$@" update 2>&1)
    echo "$out" | grep -q NO_PUBKEY
    key=$(echo "$out" | sed -n '/NO_PUBKEY/ { s/^.*NO_PUBKEY \([[:alnum:]]\+\)$/\1/; p}')
    # get keyring
    gpg -q --no-options --no-default-keyring --secret-keyring etc/apt/secring.gpg --trustdb-name etc/apt/trustdb.gpg --keyring etc/apt/trusted.gpg --primary-keyring etc/apt/trusted.gpg --keyserver $GPG_KEYSERVER --recv-keys $key

    echo "--- apt-get update $@ (with trusted keys)"
    $APT_GET "$@" update
}

# if $1 == "compressed", check that we have compressed indexes, otherwise
# uncompressed ones
check_indexes() {
    echo "--- only ${1:-uncompressed} index files present"
    local F
    if [ "$1" = "compressed" ]; then
	! test -e var/lib/apt/lists/*_Packages || F=1
	! test -e var/lib/apt/lists/*_Sources || F=1
	test -e var/lib/apt/lists/*_Packages.gz || F=1
	test -e var/lib/apt/lists/*_Sources.gz || F=1
    else
	test -e var/lib/apt/lists/*_Packages || F=1
	test -e var/lib/apt/lists/*_Sources || F=1
	! test -e var/lib/apt/lists/*_Packages.gz || F=1
	! test -e var/lib/apt/lists/*_Sources.gz || F=1
    fi

    if [ -n "$F" ]; then
	ls -laR var/lib/apt/lists/
	exit 1
    fi
}

# test apt-cache commands
check_cache() {
    echo "--- apt-cache commands"

    $APT_CACHE show $TEST_PKG | grep -q ^Version:
    # again (with cache)
    $APT_CACHE show $TEST_PKG | grep -q ^Version:
    rm var/cache/apt/*.bin
    $APT_CACHE policy $TEST_PKG | grep -q '500 http://'
    # again (with cache)
    $APT_CACHE policy $TEST_PKG | grep -q '500 http://'

    TEST_SRC=`$APT_CACHE show $TEST_PKG | grep ^Source: | awk '{print $2}'`
    rm var/cache/apt/*.bin
    $APT_CACHE showsrc $TEST_SRC | grep -q ^Binary:
    # again (with cache)
    $APT_CACHE showsrc $TEST_SRC | grep -q ^Binary:
}

# test apt-get install
check_install() {
    echo "--- apt-get install"

    $APT_GET install -d $TEST_PKG 
    test -e var/cache/apt/archives/$TEST_PKG*.deb
    $APT_GET clean
    ! test -e var/cache/apt/archives/$TEST_PKG*.deb
}

# test apt-get source
check_get_source() {
    echo "--- apt-get source"
    $APT_GET source $TEST_PKG
    test -f $TEST_SRC_*.dsc
    test -d $TEST_SRC-*
    rm -r $TEST_SRC*
}

############################################################################
# main
############################################################################

echo "===== building sandbox ====="
WORKDIR=$(mktemp -d)
trap "cd /; rm -rf $WORKDIR" 0 HUP INT QUIT ILL ABRT FPE SEGV PIPE TERM
cd $WORKDIR

rm -fr etc var
rm -f home
ln -s /home home
mkdir -p etc/apt/preferences.d etc/apt/trusted.gpg.d var/cache/apt/archives/partial var/lib/apt/lists/partial var/lib/dpkg
cp /etc/apt/trusted.gpg etc/apt
touch var/lib/dpkg/status
echo "deb $TEST_SOURCE" > etc/apt/sources.list
echo "deb-src $TEST_SOURCE" >> etc/apt/sources.list

echo "===== uncompressed indexes ====="
# first attempt should fail, no trusted GPG key
check_update
check_indexes
check_cache
check_install
check_get_source

echo "--- apt-get update with preexisting indexes"
$APT_GET update
check_indexes

echo "--- apt-get update with preexisting indexes and pdiff mode"
$APT_GET -o Acquire::PDiffs=true update
check_indexes

echo "===== compressed indexes ====="
find var/lib/apt/lists/ -type f | xargs -r rm
check_update -o Acquire::GzipIndexes=true
check_indexes compressed
check_cache
check_install
check_get_source

echo "--- apt-get update with preexisting indexes"
check_update -o Acquire::GzipIndexes=true
check_indexes

echo "--- apt-get update with preexisting indexes and pdiff mode"
check_update -o Acquire::GzipIndexes=true -o Acquire::PDiffs=true update
check_indexes

echo "===== ALL TESTS PASSED ====="
