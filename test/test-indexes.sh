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

OPTS="-qq -o Dir::Bin::Methods=$BUILDDIR/bin/methods -o Debug::NoLocking=true"
DEBUG=""
#DEBUG="-o Debug::pkgCacheGen=true"
#DEBUG="-o Debug::pkgAcquire=true"
APT_GET="$BUILDDIR/bin/apt-get $OPTS $DEBUG"
APT_CACHE="$BUILDDIR/bin/apt-cache $OPTS $DEBUG"
APT_FTPARCHIVE="$BUILDDIR/bin/apt-ftparchive"

[ -x "$BUILDDIR/bin/apt-get" ] || {
    echo "please build the tree first" >&2
    exit 1
}

check_update() {
    echo "--- apt-get update $@ (no trusted keys)"

    rm -f etc/apt/trusted.gpg etc/apt/secring.gpg
    touch etc/apt/trusted.gpg etc/apt/secring.gpg
    find var/lib/apt/lists/ -type f | xargs -r rm

    # first attempt should fail, no trusted GPG key
    out=$($APT_GET "$@" update 2>&1)
    echo "$out" | grep -q NO_PUBKEY
    key=$(echo "$out" | sed -n '/NO_PUBKEY/ { s/^.*NO_PUBKEY \([[:alnum:]]\+\)$/\1/; p}')

    # get keyring
    gpg -q --no-options --no-default-keyring --secret-keyring etc/apt/secring.gpg --trustdb-name etc/apt/trustdb.gpg --keyring etc/apt/trusted.gpg --primary-keyring etc/apt/trusted.gpg --keyserver $GPG_KEYSERVER --recv-keys $key

    # now it should work
    echo "--- apt-get update $@ (with trusted keys)"
    find var/lib/apt/lists/ -type f | xargs -r rm
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
    $APT_CACHE policy $TEST_PKG | egrep -q '500 (http://|file:/)'
    # again (with cache)
    $APT_CACHE policy $TEST_PKG | egrep -q '500 (http://|file:/)'

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
    # quiesce: it'll complain about not being able to verify the signature
    $APT_GET source $TEST_PKG >/dev/null 2>&1
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
mkdir -p etc/apt/preferences.d etc/apt/trusted.gpg.d etc/apt/apt.conf.d var/cache/apt/archives/partial var/lib/apt/lists/partial var/lib/dpkg
cp /etc/apt/trusted.gpg etc/apt
touch var/lib/dpkg/status
echo "deb $TEST_SOURCE" > etc/apt/sources.list
echo "deb-src $TEST_SOURCE" >> etc/apt/sources.list

# specifying -o RootDir at the command line does not work for
# etc/apt/apt.conf.d/ since it is parsed after pkgInitConfig(); $APT_CONFIG is
# checked first, so this works
echo "RootDir \"$WORKDIR\";" > apt_config
export APT_CONFIG=`pwd`/apt_config

echo "==== no indexes ===="
echo '--- apt-get check works without indexes'
[ -z `$APT_GET check` ]
echo '--- apt-cache policy works without indexes'
$APT_CACHE policy bash >/dev/null
echo '--- apt-cache show works without indexes'
! LC_MESSAGES=C $APT_CACHE show bash 2>&1| grep -q 'E: No packages found'

echo "===== uncompressed indexes ====="
echo 'Acquire::GzipIndexes "false";' > etc/apt/apt.conf.d/02compress-indexes
check_update
check_indexes
check_cache
check_install
check_get_source

echo "--- apt-get update with preexisting indexes"
$APT_GET update
check_indexes
check_cache

echo "--- apt-get update with preexisting indexes and pdiff mode"
$APT_GET -o Acquire::PDiffs=true update
check_indexes
check_cache

echo "===== compressed indexes (CLI option) ====="
check_update -o Acquire::GzipIndexes=true
check_indexes compressed
check_cache
check_install
check_get_source

echo "--- apt-get update with preexisting indexes"
$APT_GET -o Acquire::GzipIndexes=true update
check_indexes compressed
check_cache

echo "--- apt-get update with preexisting indexes and pdiff mode"
$APT_GET -o Acquire::GzipIndexes=true -o Acquire::PDiffs=true update
check_indexes compressed
check_cache

echo "===== compressed indexes (apt.conf.d option) ====="
cat <<EOF > etc/apt/apt.conf.d/02compress-indexes
Acquire::GzipIndexes "true";
Acquire::CompressionTypes::Order:: "gz";
EOF

check_update
check_indexes compressed
check_cache
check_install
check_get_source

echo "--- apt-get update with preexisting indexes"
$APT_GET update
check_indexes compressed
check_cache

echo "--- apt-get update with preexisting indexes and pdiff mode"
$APT_GET -o Acquire::PDiffs=true update
check_indexes compressed
check_cache

rm etc/apt/apt.conf.d/02compress-indexes

echo "==== apt-ftparchive ===="
mkdir arch
$APT_GET install -d $TEST_PKG 
cp var/cache/apt/archives/$TEST_PKG*.deb arch/
cd arch
$APT_GET source -d $TEST_PKG >/dev/null 2>&1
$APT_FTPARCHIVE packages . | gzip -9 > Packages.gz
$APT_FTPARCHIVE sources . | gzip -9 > Sources.gz
cd ..

echo "deb file://$WORKDIR/arch /
deb-src file://$WORKDIR/arch /" > etc/apt/sources.list
$APT_GET clean

echo "==== uncompressed indexes from local file:// archive ===="
echo "--- apt-get update"
$APT_GET update
check_indexes
check_cache
check_get_source

echo "==== compressed indexes from local file:// archive ===="
echo "--- apt-get update"
$APT_GET -o Acquire::GzipIndexes=true update
# EXFAIL: file:/ URIs currently decompress even with above option
#check_indexes compressed
check_indexes
check_cache
check_get_source

echo "===== ALL TESTS PASSED ====="
