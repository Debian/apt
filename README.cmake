The Make System
================

To compile this program using cmake you require cmake 3.3 or newer.

Building
--------
The recommended way is to generate a build directory and build in it, e.g.

    mkdir build
    cd build
    cmake ..        OR cmake -G Ninja ..
    make -j4        OR ninja

You can use either the make or the ninja generator; the ninja stuff is faster,
though. You can also build in-tree:

    cmake -G Ninja
    ninja

To build a subdirectory; for example, apt-pkg, use one of:

    ninja apt-pkg/all
    make -C apt-pkg -j4    (or cd apt-pkg && make -j4)

Ninja automatically parallelizes, make needs an explicit -j switch. The travis
system uses the make generator, the packaging as well.
