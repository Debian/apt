# Order-preserving minimal perfect hash function generator

Build order-preserving minimal perfect hash functions.

[![codecov](https://codecov.io/gh/julian-klode/triehash/branch/master/graph/badge.svg)](https://codecov.io/gh/julian-klode/triehash)
[![Build Status](https://travis-ci.org/julian-klode/triehash.svg?branch=master)](https://travis-ci.org/julian-klode/triehash)

## Performance

Performance was evaluated against other hash functions. As an input set, the
fields of Debian Packages and Sources files was used, and each hash function
was run 1,000,000 times for each word. The byte count of the words were then
summed up and divided by the total number of nanoseconds each function ran, so
all speeds below are given in bytes per nanosecond, AKA gigabyte per second.

arch/function|jak-x230 (amd64)|backup (amd64)|asachi.d.o (arm64)|asachi.d.o (armel)|asachi.d.o (armhf)|plummer.d.o (ppc64el)|eller.d.o (mipsel)
-------------|----------------|--------------|------------------|------------------|------------------|---------------------|------------------
Trie         |      2.4       |      1.9     |      1.2         |      0.9         |      0.8         |      2.0            |      0.2
Trie (*)     |      2.2       |      1.7     |      0.8         |      0.7         |      0.7         |      1.8            |      0.2
re2c         |      1.7       |      1.3     |      0.9         |      0.9         |      0.7         |      1.6            |      0.2
re2c (*)     |      1.2       |      0.9     |      0.6         |      0.6         |      0.5         |      1.1            |      0.1
gperf (*)    |      0.7       |      0.5     |      0.2         |      0.2         |      0.2         |      0.5            |      0.1
gperf        |      1.3       |      0.9     |      0.3         |      0.3         |      0.2         |      0.4            |      0.1
djb (*)      |      0.7       |      0.5     |      0.3         |      0.3         |      0.3         |      0.5            |      0.1
djb (**)     |      1.0       |      0.7     |      0.4         |      0.5         |      0.5         |      0.6            |      0.2
djb          |      0.9       |      0.7     |      0.5         |      0.5         |      0.5         |      0.7            |      0.2
apt (*)      |      1.2       |      0.9     |      0.7         |      0.7         |      0.7         |      1.1            |      0.2
apt (**)     |      2.3       |      1.7     |      0.7         |      0.9         |      0.8         |      1.9            |      0.2

And transposed:

function/arch        |Trie     |Trie (*) |re2c     |re2c (*) |gperf (*)|gperf    |djb (*)  |djb (**) |djb      |apt (*)  |apt (**)
---------------------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------
jak-x230 (amd64)     |      2.4|      2.2|      1.7|      1.2|      0.7|      1.3|      0.7|      1.0|      0.9|      1.2|      2.3
backup (amd64)       |      1.9|      1.7|      1.3|      0.9|      0.5|      0.9|      0.5|      0.7|      0.7|      0.9|      1.7
asachi.d.o (arm64)   |      1.2|      0.8|      0.9|      0.6|      0.2|      0.3|      0.3|      0.4|      0.5|      0.7|      0.7
asachi.d.o (armel)   |      0.9|      0.7|      0.9|      0.6|      0.2|      0.3|      0.3|      0.5|      0.5|      0.7|      0.9
asachi.d.o (armhf)   |      0.8|      0.7|      0.7|      0.5|      0.2|      0.2|      0.3|      0.5|      0.5|      0.7|      0.8
plummer.d.o (ppc64el)|      2.0|      1.8|      1.6|      1.1|      0.5|      0.4|      0.5|      0.6|      0.7|      1.1|      1.9
eller.d.o (mipsel)   |      0.2|      0.2|      0.2|      0.1|      0.1|      0.1|      0.1|      0.2|      0.2|      0.2|      0.2


Legend:

* The (*) variants are case-insensitive, (**) are more optimised versions
  of the (*) versions.
* DJB (*) is a DJB Hash with naive lowercase conversion, DJB (**) just ORs one
  bit into each value to get alphabetical characters to be lowercase
* APT (*) is the AlphaHash function from APT which hashes the last 8 bytes in a
  word in a case-insensitive manner. APT (**) is the same function unrolled.
* All hosts except the x230 are Debian porterboxes. The x230 has a Core i5-3320M,
  barriere has an Opteron 23xx.

Notes:

* The overhead is larger than needed on some platforms due to gcc inserting
  unneeded zero extend instructions, see:
  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77729
