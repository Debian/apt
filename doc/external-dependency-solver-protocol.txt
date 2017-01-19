# APT External Dependency Solver Protocol (EDSP) - version 0.5

This document describes the communication protocol between APT and
external dependency solvers. The protocol is called APT EDSP, for "APT
External Dependency Solver Protocol".


## Terminology

In the following we use the term **architecture qualified package name**
(or *arch-qualified package names* for short) to refer to package
identifiers of the form "package:arch" where "package" is a package name
and "arch" a dpkg architecture.


## Components

- **APT**: we know this one.
- APT is equipped with its own **internal solver** for dependencies,
  which is identified by the string `internal`.
- **External solver**: an *external* software component able to resolve
  dependencies on behalf of APT.
  
At each interaction with APT, a single solver is in use.  When there is
a total of 2 or more solvers, internals or externals, the user can
choose which one to use.

Each solver is identified by an unique string, the **solver
name**. Solver names must be formed using only alphanumeric ASCII
characters, dashes, and underscores; solver names must start with a
lowercase ASCII letter. The special name `internal` denotes APT's
internal solver, is reserved, and cannot be used by external solvers.


## Installation

Each external solver is installed as a file under Dir::Bin::Solvers (see
below), which defaults to `/usr/lib/apt/solvers`. We will assume in the
remainder of this section that such a default value is in effect.

The naming scheme is `/usr/lib/apt/solvers/NAME`, where `NAME` is the
name of the external solver.

Each file under `/usr/lib/apt/solvers` corresponding to an external
solver must be executable.

No non-solver files must be installed under `/usr/lib/apt/solvers`, so
that an index of available external solvers can be obtained by listing
the content of that directory.


## Configuration

Several APT options can be used to affect dependency solving in APT. An
overview of them is given below. Please refer to proper APT
configuration documentation for more, and more up to date, information.

- **APT::Solver**: the name of the solver to be used for
  dependency solving. Defaults to `internal`

- **Dir::Bin::Solvers**: absolute path of the directory where to look for
  external solvers. Defaults to `/usr/lib/apt/solvers`.

- **APT::Solver::Strict-Pinning**: whether pinning must be strictly
  respected (as the internal solver does) or can be slightly deviated
  from. Defaults to `yes`.

- **APT::Solver::Preferences**: user preference string used during
  dependency solving by the requested solver. Check the documentation
  of the solver you are using if and what is supported as a value here.
  Defaults to the empty string.

- **APT::Solver::RunAsUser**: if APT itself is run as root it will
  change to this user before executing the solver. Defaults to the value
  of APT::Sandbox::User, which itself defaults to `_apt`. Can be
  disabled by set this option to `root`.

The options **Strict-Pinning** and **Preferences** can also be set for
a specific solver only via **APT::Solver::NAME::Strict-Pinning** and
**APT::Solver::NAME::Preferences** respectively where `NAME` is the name
of the external solver this option should apply to. These options if set
override the generic options; for simplicity the documentation will
refer only to the generic options.


## Protocol

When configured to use an external solver, APT will resort to it to
decide which packages should be installed or removed.

The interaction happens **in batch**: APT will invoke the external
solver passing the current status of installed and available packages,
as well as the user request to alter the set of installed packages. The
external solver will compute a new complete set of installed packages
and gives APT a "diff" listing of which *additional* packages should be
installed and of which currently installed packages should be
*removed*. (Note: the order in which those actions have to be performed
will be up to APT to decide.)

External solvers are invoked by executing them. Communications happens
via the file descriptors: **stdin** (standard input) and **stdout**
(standard output). stderr is not used by the EDSP protocol. Solvers can
therefore use stderr to dump debugging information that could be
inspected separately.

After invocation, the protocol passes through a sequence of phases:

1. APT invokes the external solver
2. APT send to the solver a dependency solving **scenario**
3. The solver solves dependencies. During this phase the solver may
   send, repeatedly, **progress** information to APT.
4. The solver sends back to APT an **answer**, i.e. either a *solution*
   or an *error* report.
5. The external solver exits


### Scenario

A scenario is a text file encoded in a format very similar to the "Deb
822" format (AKA "the format used by Debian `Packages` files"). A
scenario consists of two distinct parts: a **request** and a **package
universe**, occurring in that order. The request consists of a single
Deb 822 stanza, while the package universe consists of several such
stanzas. All stanzas occurring in a scenario are separated by an empty
line.


#### Request

Within a dependency solving scenario, a request represents the action on
installed packages requested by the user.

A request is a single Deb 822 stanza opened by a mandatory Request field
and followed by a mixture of action, preference, and global
configuration fields.

The value of the **Request:** field is a string describing the EDSP
protocol which will be used to communicate. At present, the string must
be `EDSP 0.5`. Request fields are mainly used to identify the beginning
of a request stanza; their actual values are otherwise not used by the
EDSP protocol.

The following **configuration fields** are supported in request stanzas:

- **Architecture:** (mandatory) The name of the *native* architecture on
  the user machine (see also: `dpkg --print-architecture`)

- **Architectures:** (optional, defaults to the native architecture) A
  space separated list of *all* architectures known to APT (this is
  roughly equivalent to the union of `dpkg --print-architecture` and
  `dpkg --print-foreign-architectures`)

The following **action fields** are supported in request stanzas:

- **Install:** (optional, defaults to the empty string) A space
  separated list of arch-qualified package names, with *no version
  attached*, to install. This field denotes a list of packages that the
  user wants to install, usually via an APT `install` request.

- **Remove:** (optional, defaults to the empty string) Same syntax of
  Install. This field denotes a list of packages that the user wants to
  remove, usually via APT `remove` or `purge` requests.

- **Upgrade-All:** (optional, defaults to `no`). Allowed values `yes`,
  `no`. When set to `yes`, an upgrade of all installed packages has been
  requested, usually via an upgrade command like 'apt full-upgrade'.

- **Autoremove:** (optional, defaults to `no`). Allowed values: `yes`,
  `no`. When set to `yes`, a clean up of unused automatically installed
  packages has been requested, usually via an APT `autoremove` request.

- **Upgrade:** (deprecated, optional, defaults to `no`). Allowed values:
  `yes`, `no`. When set to `yes`, an upgrade of all installed packages
  has been requested, usually via an APT `upgrade` request. A value of
  `yes` is equivalent to the fields `Upgrade-All`,
  `Forbid-New-Install`and `Forbid-Remove` all set to `yes`.

- **Dist-Upgrade:** (deprecated, optional, defaults to `no`). Allowed
  values: `yes`, `no`. Same as Upgrade, but for APT `dist-upgrade`
  requests. A value of `yes` is equivalent to the field `Upgrade-All`
  set to `yes` and the fields `Forbid-New-Install`and `Forbid-Remove`
  set to `no`.

The following **preference fields** are supported in request stanzas:

- **Strict-Pinning:** (optional, defaults to `yes`). Allowed values:
  `yes`, `no`. When set to `yes`, APT pinning is strict, in the sense
  that the solver must not propose to install packages which are not APT
  candidates (see the `APT-Pin` and `APT-Candidate` fields in the
  package universe). When set to `no`, the solver does only a best
  effort attempt to install APT candidates. Usually, the value of this
  field comes from the `APT::Solver::Strict-Pinning` configuration
  option.

- **Forbid-New-Install:* (optional, defaults to `no`). Allowed values:
  `yes`, `no`. When set to `yes` the resolver is forbidden to install
  new packages in its returned solution.

- **Forbid-Remove:* (optional, defaults to `no`). Allowed values: `yes`,
  `no`.  When set to `yes` the resolver is forbidden to remove currently
  installed packages in its returned solution.

- **Solver:** (optional, defaults to the empty string) a purely
  informational string specifying to which solver this request was send
  initially.

- **Preferences:** (optional, defaults to the empty string)
  a solver-specific optimization string, usually coming from the
  `APT::Solver::Preferences` configuration option.


#### Package universe

A package universe is a list of Deb 822 stanzas, one per package, called
**package stanzas**. Each package stanzas starts with a Package
field. The following fields are supported in package stanzas:

- All fields contained in the dpkg database, with the exception of
  fields marked as "internal" (see the manpage `dpkg-query (1)`). Among
  those fields, the following are mandatory for all package stanzas:
  Package, Version, Architecture.
  
  It is recommended not to pass the Description field to external
  solvers or, alternatively, to trim it to the short description only.

- **Installed:** (optional, defaults to `no`). Allowed values: `yes`,
  `no`. When set to `yes`, the corresponding package is currently
  installed.
  
  Note: the Status field present in the dpkg database must not be passed
  to the external solver, as it's an internal dpkg field. Installed and
  other fields permit one to encode the most relevant aspects of Status
  in communications with solvers.

- **Hold:** (optional, defaults to `no`). Allowed values: `yes`,
  `no`. When set to `yes`, the corresponding package is marked as "on
  hold" by dpkg.

- **APT-ID:** (mandatory). Unique package identifier, according to APT.

- **APT-Pin:** (mandatory). Must be an integer. Package pin value,
  according to APT policy.

- **APT-Candidate:** (optional, defaults to `no`). Allowed values:
  `yes`, `no`. When set to `yes`, the corresponding package is the APT
  candidate for installation among all available packages with the same
  name and architecture.

- **APT-Automatic:** (optional, defaults to `no`). Allowed values:
  `yes`, `no`. When set to `yes`, the corresponding package is marked by
  APT as automatic installed. Note that automatic installed packages
  should be removed by the solver only when the Autoremove action is
  requested (see Request section).

- **APT-Release:** (optional) The releases the package belongs to, according to
  APT. The format of this field is multiline with one value per line and the
  first line (the one containing the field name) empty. Each subsequent line
  corresponds to one of the releases the package belongs to and looks like
  this: `o=Debian,a=unstable,n=sid,l=Debian,c=main`. That is, each release line
  is a comma-separated list of "key=value" pairs, each of which denotes a
  Release file entry (Origin, Label, Codename, etc.) in the format of
  APT_PREFERENCES(5).

- **Source:** (optional) The name of the source package the binary
  package this record is for was built from.
  This field does NOT include the version of the source package unlike
  the Source field in the dpkg database. The version is optionally
  available in the **Source-Version:** field.


### Answer

An answer from the external solver to APT is either a *solution* or an
*error*.

The following invariant on **exit codes** must hold true. When the
external solver is *able to find a solution*, it will write the solution
to standard output and then exit with an exit code of 0. When the
external solver is *unable to find a solution* (and is aware of that),
it will write an error to standard output and then exit with an exit
code of 0. An exit code other than 0 will be interpreted as a solver
crash with no meaningful error about dependency resolution to convey to
the user.


#### Solution

A solution is a list of Deb 822 stanzas. Each of them could be an install
stanza (telling APT to install a specific new package or to upgrade or
downgrade a package to a specific version), a remove stanza (telling APT to
remove one), or an autoremove stanza (telling APT about the *future*
possibility of removing a package using the Autoremove action).

An **install stanza** starts with an Install field and supports the
following fields:

- **Install:** (mandatory). The value is a package identifier,
  referencing one of the package stanzas of the package universe via its
  APT-ID field.

- All fields supported by package stanzas.

**Remove stanzas** are similar to install stanzas, but have **Remove**
fields instead of Install fields.

**Autoremove stanzas** are similar to install stanzas, but have
**Autoremove** fields instead of Install fields. Autoremove stanzas
should be output so that APT can inform the user of which packages they
can now autoremove, as a consequence of the executed action. However,
this protocol makes no assumption on the fact that a subsequent
invocation of an Autoremove action will actually remove the very same
packages indicated by Autoremove stanzas in the former solution.

A package can't be installed in multiple versions at the same time, so
for each package there can at most one version be selected either for
installation or removal. This especially means that a solver is neither
allowed to represent package upgrades as a remove of the installed
version and the installation of another (the remove is implicit and must
be omitted from the solution) nor is it supported to revert previous
actions in the solution with later actions. APT is allowed to show
warnings and might even misbehave in earlier versions if a solver is
violating this assumption.

In terms of expressivity, install and remove stanzas can carry one
single field each, as APT-IDs are enough to pinpoint packages to be
installed/removed. Nonetheless, for protocol readability, it is
recommended that solvers either add unconditionally the fields Package,
Version, and Architecture to all install/remove stanzas or,
alternatively, that they support a `--verbose` command line flag that
explicitly enables the output of those fields in solutions.


#### Error

An error is a single Deb 822 stanza, starting the field Error. The
following fields are supported in error stanzas:

- **Error:** (mandatory). The value of this field is ignored, although
  it should be a unique error identifier, such as a UUID.

- **Message:** (mandatory). The value of this field is a text string,
  meant to be read by humans, that explains the cause of the solver
  error. Message fields might be multi-line, like the Description field
  in the dpkg database. The first line conveys a short message, which
  can be explained in more details using subsequent lines.


### Progress

During dependency solving, an external solver may send progress
information to APT using **progress stanzas**. A progress stanza starts
with the Progress field and might contain the following fields:

- **Progress:** (mandatory). The value of this field is a date and time
  timestamp from the UTC timezone, in RFC 2822 format (see 'date -uR' as
  an example). The timestamp provides a time annotation for the
  progress report.

- **Percentage:** (optional). An integer from 0 to 100, representing the
  completion of the dependency solving process, as declared by the
  solver.

- **Message:** (optional). A textual message, meant to be read by the
  APT user, telling what is going on within the dependency solving
  (e.g. the current phase of dependency solving, as declared by the
  solver).


# Future extensions

Potential future extensions to this protocol, listed in no specific
order, include:

- fixed error types to identify common failures across solvers and
  enable APT to translate error messages
- structured error data to explain failures in terms of packages and
  dependencies
