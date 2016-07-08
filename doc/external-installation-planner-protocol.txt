# APT External Installation Planner Protocol (EIPP) - version 0.1

This document describes the communication protocol between APT and
external installation planner. The protocol is called APT EIPP, for "APT
External Installation Planner Protocol".


## Terminology

In the following we use the term **architecture qualified package name**
(or *arch-qualified package names* for short) to refer to package
identifiers of the form "package:arch" where "package" is a package name
and "arch" a dpkg architecture.


## Components

- **APT**: we know this one.
- APT is equipped with its own **internal planner** for the order of
  package installation (and removal) which is identified by the string
  `internal`.
- **External planner**: an *external* software component able to plan an
  installation on behalf of APT.

At each interaction with APT, a single planner is in use.  When there is
a total of 2 or more planners, internals or externals, the user can
choose which one to use.

Each planner is identified by an unique string, the **planner name**.
Planner names must be formed using only alphanumeric ASCII characters,
dashes, and underscores; planner names must start with a lowercase ASCII
letter.  The special name `internal` denotes APT's internal planner, is
reserved, and cannot be used by external planners.


## Installation

Each external planner is installed as a file under Dir::Bin::Planners
(see below), which defaults to `/usr/lib/apt/planners`. We will assume
in the remainder of this section that such a default value is in effect.

The naming scheme is `/usr/lib/apt/planners/NAME`, where `NAME` is the
name of the external planner.

Each file under `/usr/lib/apt/planners` corresponding to an external
planner must be executable.

No non-planner files must be installed under `/usr/lib/apt/planners`, so
that an index of available external planners can be obtained by listing
the content of that directory.


## Configuration

Several APT options can be used to affect installation planing in APT.
An overview of them is given below. Please refer to proper APT
configuration documentation for more, and more up to date, information.

- **APT::Planner**: the name of the planner to be used for dependency
  solving.  Defaults to `internal`

- **Dir::Bin::Planners**: absolute path of the directory where to look
  for external solvers. Defaults to `/usr/lib/apt/planners`.


## Protocol

When configured to use an external planner, APT will resort to it to
decide in which order packages should be installed, configured and
removed.

The interaction happens **in batch**: APT will invoke the external
planner passing the current status of (half-)installed packages and of
packages which should be installed, as well as a request denoting the
packages to install, reinstall, remove and purge.  The external planner
will compute a valid plan of when and how to call the low-level package
manager (like dpkg) with each package to satisfy the request.

External planners are invoked by executing them. Communications happens
via the file descriptors: **stdin** (standard input) and **stdout**
(standard output). stderr is not used by the EIPP protocol. Planners can
therefore use stderr to dump debugging information that could be
inspected separately.

After invocation, the protocol passes through a sequence of phases:

1. APT invokes the external planner
2. APT send to the planner an installation planner **scenario**
3. The planner calculates the order. During this phase the planner may
   send, repeatedly, **progress** information to APT.
4. The planner sends back to APT an **answer**, i.e. either a *solution*
   or an *error* report.
5. The external planner exits


### Scenario

A scenario is a text file encoded in a format very similar to the "Deb
822" format (AKA "the format used by Debian `Packages` files"). A
scenario consists of two distinct parts: a **request** and a **package
universe**, occurring in that order. The request consists of a single
Deb 822 stanza, while the package universe consists of several such
stanzas. All stanzas occurring in a scenario are separated by an empty
line.


#### Request

Within an installation planner scenario, a request represents the action
on packages requested by the user explicitly as well as potentially
additions calculated by a dependency resolver which the user has
accepted.

An installation planner is not allowed to suggest the modification of
package states (e.g. removing additional packages) even if it can't
calculate a solution otherwise – the planner must error out in such
a case. An exception is made for scenarios which contain packages which
aren't completely installed (like half-installed or trigger-awaiting):
Solvers are free to move these packages to a fully installed state (but
are still forbidden to remove them).

A request is a single Deb 822 stanza opened by a mandatory Request field
and followed by a mixture of action, preference, and global
configuration fields.

The value of the **Request:** field is a string describing the EIPP
protocol which will be used to communicate and especially which answers
APT will understand. At present, the string must be `EIPP 0.1`. Request
fields are mainly used to identify the beginning of a request stanza;
their actual values are otherwise not used by the EIPP protocol.

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

- **ReInstall:** (optional, defaults to the empty string) Same syntax of
  Install. This field denotes a list of packages which are installed,
  but should be reinstalled again e.g. because files shipped by that
  package were removed or corrupted accidentally, usually requested via
  an APT `install` request with the `--reinstall` flag.

The following **preference fields** are supported in request stanzas:

- **Planner:** (optional, defaults to the empty string) a purely
  informational string specifying to which planner this request was send
  initially.

- **Immediate-Configuration:** (option, unset by default) A boolean
  value defining if the planner should try to configure all packages as
  quickly as possible (true) or shouldn't perform any kind of immediate
  configuration at all (false). If not explicitly set with this field
  the planner is free to pick either mode or implementing e.g. a mode
  which configures only packages immediately if they are flagged as
  `Essential` (or are dependencies of packages marked as `Essential`).

- **Allow-Temporary-Remove-of-Essentials** (optional, defaults to `no`).
  A boolean value allowing the planner (if set to yes) to temporarily
  remove an essential package. Associated with the APT::Force-LoopBreak
  configuration option its main use is highlighting that planners who do
  temporary removes must take special care in terms of essentials. Legit
  uses of this option by users is very uncommon, traditionally
  a situation in which it is needed indicates a packaging error.


#### Package universe

A package universe is a list of Deb 822 stanzas, one per package, called
**package stanzas**. Each package stanzas starts with a Package
field. The following fields are supported in package stanzas:

- The fields Package, Version, Architecture (all mandatory) and
  Multi-Arch, Pre-Depends, Depends, Conflicts, Breaks, Essential
  (optional) as they are contained in the dpkg database (see the manpage
  `dpkg-query (1)`).

- **Status:** (optional, defaults to `uninstalled`). Allowed values are
  the "package status" names as listed in `dpkg-query (1)` and visible
  e.g. in the dpkg database as the second value in the space separated
  list of values in the Status field there. In other words: Neither
  desired action nor error flags are present in this field in EIPP!

- **APT-ID:** (mandatory). Unique package identifier, according to APT.


### Answer

An answer from the external planner to APT is either a *solution* or an
*error*.

The following invariant on **exit codes** must hold true. When the
external planner is *able to find a solution*, it will write the
solution to standard output and then exit with an exit code of 0. When
the external planner is *unable to find a solution* (and is aware of
that), it will write an error to standard output and then exit with an
exit code of 0.  An exit code other than 0 will be interpreted as
a planner crash with no meaningful error about dependency resolution to
convey to the user.


#### Solution

A solution is a list of Deb 822 stanzas. Each of them could be an:

- unpack stanza to cause the extraction of a package to the disk

- configure stanza to cause an unpacked package to be configured and
  therefore the installation to be completed

- remove stanza to cause the removal of a package from the system

An **unpack stanza** starts with an Unpack field and supports the
following fields:

- **Unpack:** (mandatory). The value is a package identifier,
  referencing one of the package stanzas of the package universe via its
  APT-ID field.

- All fields supported by package stanzas.

**Configure** and **Remove stanzas** require and support the same
fields with the exception of the Unpack field which is replaced in
these instances with the Configure or Remove field respectively.

The order of the stanzas is significant (unlike in the EDSP protocol),
with the first stanza being the first performed action. If multiple
stanzas of the same type appear in direct succession the order in such
a set isn't significant through.

The solution needs to be valid (it is not allowed to configure a package
before it was unpacked, dependency relations must be satisfied, …), but
they don't need to be complete: A planner can and should expect that any
package which wasn't explicitly configured will be configured at the end
automatically. That also means through that a planner is not allowed to
produce a solution in which a package remains unconfigured. Also,
packages which are requested to be removed will be automatically removed
at the end if not marked for removal explicitly earlier.

In terms of expressivity, all stanzas can carry one single field each, as
APT-IDs are enough to pinpoint packages to be installed/removed.
Nonetheless, for protocol readability, it is recommended that planners
either add unconditionally the fields Package, Version, and Architecture
to all install/remove stanzas or, alternatively, that they support
a `--verbose` command line flag that explicitly enables the output of
those fields in solutions.

#### Error

An error is a single Deb 822 stanza, starting the field Error. The
following fields are supported in error stanzas:

- **Error:** (mandatory). The value of this field is ignored, although
  it should be a unique error identifier, such as a UUID.

- **Message:** (mandatory). The value of this field is a text string,
  meant to be read by humans, that explains the cause of the planner
  error.  Message fields might be multi-line, like the Description field
  in the dpkg database. The first line conveys a short message, which
  can be explained in more details using subsequent lines.


### Progress

During dependency solving, an external planner may send progress
information to APT using **progress stanzas**. A progress stanza starts
with the Progress field and might contain the following fields:

- **Progress:** (mandatory). The value of this field is a date and time
  timestamp from the UTC timezone, in RFC 2822 format (see 'date -uR' as
  an example). The timestamp provides a time annotation for the
  progress report.

- **Percentage:** (optional). An integer from 0 to 100, representing the
  completion of the installation planning process, as declared by the
  planner.

- **Message:** (optional). A textual message, meant to be read by the
  APT user, telling what is going on within the installation planner
  (e.g. the current phase of planning, as declared by the planner).


# Future extensions

Potential future extensions to this protocol are to be discussed on
deity@lists.debian.org.
