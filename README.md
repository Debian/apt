APT
===

apt is the main commandline package manager for Debian and its derivatives.
It provides commandline tools for searching and managing as well as querying
information about packages as well as low-level access to all features
provided by the libapt-pkg and libapt-inst libraries which higher-level
package managers can depend upon.

Included tools are:

* apt-get for retrieval of packages and information about them
  from authenticated sources and for installation, upgrade and
  removal of packages together with their dependencies
* apt-cache for querying available information about installed
  as well as installable packages
* apt-cdrom to use removable media as a source for packages
* apt-config as an interface to the configuration settings
* apt-key as an interface to manage authentication keys
* apt-extracttemplates to be used by debconf to prompt for configuration
  questions before installation.
* apt-ftparchive creates Packages and other index files
  needed to publish an archive of debian packages
* apt-sortpkgs is a Packages/Sources file normalizer.

The libraries libapt-pkg and libapt-inst are also maintained as part of this project,
alongside various additional binaries like the acquire-methods used by them.
Bindings for Python ([python-apt](https://tracker.debian.org/pkg/python-apt)) and
Perl ([libapt-pkg-perl](https://tracker.debian.org/pkg/libapt-pkg-perl)) are available as separated projects.

Discussion happens mostly on [the mailinglist](mailto:deity@lists.debian.org) ([archive](https://lists.debian.org/deity/)) and on [IRC](irc://irc.oftc.net/debian-apt).
Our bugtracker as well as a general overview can be found at the [Debian Tracker page](https://tracker.debian.org/pkg/apt).


Contributing
------------
APT is maintained in git, the official repository being located at
`git://anonscm.debian.org/apt/apt.git` ([webgit](http://anonscm.debian.org/gitweb/?p=apt/apt.git)),
but also available at other locations like [GitHub](https://github.com/Debian/apt).

The default branch is `master`, other branches targeted at different
derivatives and releases being used as needed. Various topic branches in
different stages of completion might be branched of from those, which you
are encouraged to do as well.

### Coding

APT uses cmake. To start building, you need to run

  cmake <path to source directory>

from a build directory. For example, if you want to build in the source tree,
run:

  cmake .

Then you can use make as you normally would (pass -j <count> to perform <count>
jobs in parallel).

You can also use the Ninja generator of cmake, to do that pass
  -G Ninja
to the cmake invocation, and then use ninja instead of make.

The source code uses in most parts a relatively uncommon indent convention,
namely 3 spaces with 8 space tab (see [doc/style.txt](http://anonscm.debian.org/gitweb/?p=apt/apt.git;a=blob;f=doc/style.txt) for more on this).
Adhering to it avoids unnecessary code-churn destroying history (aka: `git blame`)
and you are therefore encouraged to write patches in this style.
Your editor can surely help you with this, for vim the settings would be
`setlocal shiftwidth=3 noexpandtab tabstop=8`
(the later two are the default configuration and could therefore be omitted).

### Translations

While we welcome contributions here, we highly encourage you to contact the [Debian Internationalization (i18n) team](https://wiki.debian.org/Teams/I18n).
Various language teams have formed which can help you creating, maintaining
and improving a translation, while we could only do a basic syntax check of the
file format…

Further more, Translating APT is split into two independent parts:
The program translation, meaning the messages printed by the tools,
as well as the manpages and other documentation shipped with APT.

### Bug triage

Software tools like APT which are used by thousands of users every
day have a steady flow of incoming bugreports. Not all of them are really
bugs in APT: It can be packaging bugs like failing maintainer scripts a
user reports against apt, because apt was the command he executed leading
to this failure or various wishlist items for new features. Given enough time
also the occasional duplicate enters the system.
Our bugtracker is therefore full with open bugreports which are waiting for you! ;)

Testing
-------

### Manual execution

When you make changes and want to run them manually, you can just do so. CMake
automatically inserts an rpath so the binaries find the correct libraries.

### Integration tests

There is a extensive integration testsuite available which can be run via:

	$ ./test/integration/run-tests

While these tests are not executed at package build-time as they require additional
dependencies, the repository contains the configuration needed to run them on [Travis CI](https://travis-ci.org/)
as well as via autopkgtests e.g. on [Debian Continuous Integration](http://ci.debian.net/?q=apt#package/apt).

A testcase here is a shellscript embedded in a framework creating an environment in which
apt tools can be used naturally without root-rights to test every aspect of its behavior
itself as well as in conjunction with dpkg and other tools while working with packages.


### Unit tests

These tests are gtest-dev based, reside in `./test/libapt` and can be run with `make test`.
They are executed at package build-time, but not by `make`.

Debugging
---------

APT does many things, so there is no central debug mode which could be
activated. It uses instead various config-options to activate debug output
in certain areas. The following describes some common scenarios and generally
useful options, but is in no way exhaustive.

Note that you should *NEVER* use these settings as root to avoid accidents.
Similation mode (`-s`) is usually sufficient to help you run apt as a non-root user.

### Using different state files

If a dependency solver bug is reported, but can't be reproduced by the
triager easily, it is beneficial to ask the reporter for the
`/var/lib/dpkg/status` file, which includes the packages installed on the
system and in which version. Such a file can then be used via the option
`dir::state::status`. Beware of different architecture settings!
Bugreports usually include this information in the template. Assuming you
already have the `Packages` files for the architecture (see `sources.list`
manpage for the `arch=` option) you can change to a different architecture
with a config file like:

	APT::Architecture "arch1";
	#clear APT::Architectures;
	APT:: Architectures { "arch1"; "arch2"; }

If a certain mirror state is needed, see if you can reproduce it with [snapshot.debian.org](http://snapshot.debian.org/).
Your sources.list file (`dir::etc::sourcelist`) has to be correctly mention the repository,
but if it does, you can use different downloaded archive state files via `dir::state::lists`.

In case manually vs. automatically installed matters, you can ask the reporter for
the `/var/lib/apt/extended_states` file and use it with `dir::state::extended_states`.

### Dependency resolution

APT works in its internal resolver in two stages: First all packages are visited
and marked for installation, keep back or removal. Option `Debug::pkgDepCache::Marker`
shows this. This also decides which packages are to be installed to satisfy dependencies,
which can be seen by `Debug::pkgDepCache::AutoInstall`. After this is done, we might
be in a situation in which two packages want to be installed, but only on of them can be.
It is the job of the pkgProblemResolver to decide which of two packages 'wins' and can
therefore decide what has to happen. You can see the contenders as well as their fight and
the resulting resolution with `Debug::pkgProblemResolver`.

### Downloading files

Various binaries (called 'methods') are tasked with downloading files. The Acquire system
talks to them via simple text protocol. Depending on which side you want to see, either
`Debug::pkgAcquire::Worker` or `Debug::Acquire::http` (or similar) will show the messages.

The integration tests use a simple self-built webserver which also logs. If you find that
the http(s) methods do not behave like they should be try to implement this behavior in the
webserver for simpler and more controlled testing.

### Installation order

Dependencies are solved, packages downloaded: Everything read for the installation!
The last step in the chain is often forgotten, but still very important:
Packages have to be installed in a particular order so that their dependencies are
satisfied, but at the same time you don't want to install very important and optional
packages at the same time if possible, so that a broken optional package does not
block the correct installation of very important packages. Which option to use depends on
if you are interested in the topology sorting (`Debug::pkgOrderList`), the dependency-aware
cycle and unconfigured prevention (`Debug::pkgPackageManager`) or the actual calls
to dpkg (`Debug::pkgDpkgPm`).
