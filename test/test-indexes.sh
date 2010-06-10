#!/bin/sh -e

# Test behaviour of index retrieval and usage, in particular with uncompressed
# and gzip compressed indexes.
# Author: Martin Pitt <martin.pitt@ubuntu.com>
# (C) 2010 Canonical Ltd.

BUILDDIR=$(readlink -f $(dirname $0)/../build)

TEST_SOURCE="deb http://ftp.debian.org/debian unstable contrib"
TEST_SOURCE_KEYID=55BE302B
GPG_KEYSERVER=gpg-keyserver.de
# should be a small package with dependencies satisfiable in TEST_SOURCE, i. e.
# ideally no depends at all
TEST_PKG="python-psyco-doc"

export LD_LIBRARY_PATH=$BUILDDIR/bin

OPTS="-o RootDir=. -o Dir::Bin::Methods=$BUILDDIR/bin/methods -o Debug::NoLocking=true"
DEBUG=""
#DEBUG="-o Debug::pkgCacheGen=true"
#DEBUG="-o Debug::pkgAcquire=true"
APT_GET="$BUILDDIR/bin/apt-get $OPTS $DEBUG"
APT_CACHE="$BUILDDIR/bin/apt-cache $OPTS $DEBUG"

[ -x "$BUILDDIR/bin/apt-get" ] || {
    echo "please build the tree first" >&2
    exit 1
}

echo "---- building sandbox----"
WORKDIR=$(mktemp -d)
trap "cd /; rm -rf $WORKDIR" 0 HUP INT QUIT ILL ABRT FPE SEGV PIPE TERM
cd $WORKDIR

rm -fr etc var
rm -f home
ln -s /home home
mkdir -p etc/apt/preferences.d etc/apt/trusted.gpg.d var/cache/apt/archives/partial var/lib/apt/lists/partial var/lib/dpkg
cp /etc/apt/trusted.gpg etc/apt
touch var/lib/dpkg/status
echo "$TEST_SOURCE" > etc/apt/sources.list

# get keyring
gpg --no-options --no-default-keyring --secret-keyring etc/apt/secring.gpg --trustdb-name etc/apt/trustdb.gpg --keyring etc/apt/trusted.gpg --primary-keyring etc/apt/trusted.gpg --keyserver $GPG_KEYSERVER --recv-keys $TEST_SOURCE_KEYID

echo "---- uncompressed update ----"
$APT_GET update
test -e var/lib/apt/lists/*_Packages
! test -e var/lib/apt/lists/*_Packages.gz

echo "---- uncompressed cache ----"
$APT_CACHE show $TEST_PKG | grep -q ^Version:
# again (with cache)
$APT_CACHE show $TEST_PKG | grep -q ^Version:
rm var/cache/apt/*.bin
$APT_CACHE policy $TEST_PKG | grep -q '500 http://'
# again (with cache)
$APT_CACHE policy $TEST_PKG | grep -q '500 http://'

echo "---- uncompressed install ----"
$APT_GET install -d $TEST_PKG 
test -e var/cache/apt/archives/$TEST_PKG*.deb
$APT_GET clean
! test -e var/cache/apt/archives/$TEST_PKG*.deb

echo "----- uncompressed update with preexisting indexes, no pdiff ----"
$APT_GET -o Acquire::PDiffs=false update
test -e var/lib/apt/lists/*_Packages
! test -e var/lib/apt/lists/*_Packages.gz

echo "----- uncompressed update with preexisting indexes, with pdiff ----"
$APT_GET -o Acquire::PDiffs=true update
test -e var/lib/apt/lists/*_Packages
! test -e var/lib/apt/lists/*_Packages.gz

echo "----- compressed update ----"
find var/lib/apt/lists/ -type f | xargs -r rm
$APT_GET -o Acquire::GzipIndexes=true update
! test -e var/lib/apt/lists/*_Packages
test -e var/lib/apt/lists/*_Packages.gz

echo "---- compressed cache ----"
$APT_CACHE show $TEST_PKG | grep -q ^Version:
# again (with cache)
$APT_CACHE show $TEST_PKG | grep -q ^Version:
rm var/cache/apt/*.bin
$APT_CACHE policy $TEST_PKG | grep -q '500 http://'
# again (with cache)
$APT_CACHE policy $TEST_PKG | grep -q '500 http://'

echo "---- compressed install ----"
$APT_GET install -d $TEST_PKG 
! test -e var/cache/apt/archives/$TEST_PKG*.deb

echo "----- compressed update with preexisting indexes, no pdiff ----"
$APT_GET -o Acquire::PDiffs=false -o Acquire::GzipIndexes=true update
! test -e var/lib/apt/lists/*_Packages
test -e var/lib/apt/lists/*_Packages.gz

echo "----- compressed update with preexisting indexes, with pdiff ----"
$APT_GET -o Acquire::PDiffs=true -o Acquire::GzipIndexes=true update
! test -e var/lib/apt/lists/*_Packages
test -e var/lib/apt/lists/*_Packages.gz

echo "---- ALL TESTS PASSED ----"
