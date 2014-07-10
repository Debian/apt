apt - Advanced Packaging Tool
=============================

apt is the main package management tool for Debian and its variants.
It enables to search and install deb packages. The underlying libraries
that apt is build upon are called libapt-pkg and libapt-inst.

Coding
------
Apt is maintained in git, considering creating a branch when you
start hacking on it.

Apt uses its own autoconf based build system, see README.make for
more details. To get started, just run:
```
$ make
```
from a fresh checkout.

When you make changes and want to run them, make sure your 
$LD_LIBRARY_PATH points to the new location, e.g. via:
```
$ export LD_LIBRARY_PATH=$(pwd)/build/bin
$ ./build/bin/apt-get moo
```

Testing
-------

There is a extensive integration testsuite available via:
```
$ ./test/integration/run-tests
```

as well as gtest-dev based integration tests available in
`./test/libapt` and can be run with make test.

