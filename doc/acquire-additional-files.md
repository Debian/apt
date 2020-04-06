# Acquire additional files in 'update' operations

The download and verification of data from multiple sources in different
compression formats, with partial downloads and patches is an involved
process which is hard to implement correctly and securely.

APT front-ends share the code and binaries to make this happen in libapt
with the Acquire system, supported by helpers shipped in the apt package
itself and additional transports in individual packages like
`apt-transport-https`.

For its own operation libapt needs or can make use of *Packages*, *Sources*
and *Translation-* files, which it will acquire by default, but
a repository might contain more data files (e.g. `Contents`) a front-end
(e.g. `apt-file`) might want to use and would therefore need to be
downloaded as well.

This file describes the configuration scheme such a front-end can use to
instruct the Acquire system to download those additional files.

# The Configuration Stanza

The Acquire system uses the same configuration settings to implement the
files it downloads by default. These settings are the default, but if
they would be written in a configuration file the configuration
instructing the Acquire system to download the *Packages* files would look
like this (see also `apt.conf(5)` manpage for configuration file syntax):

	Acquire::IndexTargets::deb::Packages {
		MetaKey "$(COMPONENT)/binary-$(ARCHITECTURE)/Packages";
		ShortDescription "Packages";
		Description "$(RELEASE)/$(COMPONENT) $(ARCHITECTURE) Packages";

		flatMetaKey "Packages";
		flatDescription "$(RELEASE) Packages";

		Optional "no";
	};

All files which should be downloaded (nicknamed *Targets*) are mentioned
below the `Acquire::IndexTargets` scope. `deb` is here the type of the
`sources.list` entry the file should be acquired for. The only other
supported value is hence `deb-src`. Beware: You can't specify multiple
types here and you can't download the same (evaluated) `MetaKey` from
multiple types!

After the type you can pick any valid and unique string which preferable
refers to the file it downloads (In the example we picked *Packages*).
This string is used as identifier (if not explicitly set otherwise) for
the target class and accessible as `Identifier` and `Created-By` e.g.
in the `apt-get indextargets` output as detailed below. The identifier
is also used to allow user to enable/disable targets per sources.list
entry.

All targets have three main properties you can define:

* `MetaKey`: The identifier of the file to be downloaded as used in the
  Release file.  It is also the relative location of the file from the
  Release file.  You can neither download from a different server
  entirely (absolute URI) nor access directories above the Release file
  (e.g. "../../").
* `ShortDescription`: Very short string intended to be displayed to the
  user e.g.  while reporting progress. apt will e.g. use this string in
  the last line to indicate progress of e.g. the download of a specific
  item.
* `Description`: A preferable human understandable and readable identifier
  of which file is acquired exactly. Mainly used for progress reporting
  and error messages. apt will e.g. use this string in the Get/Hit/Err
  progress lines.
  An identifier of the site accessed as seen in the sources.list (e.g.
  `http://example.org/debian` or `file:/path/to/a/repository`) is
  automatically prefixed for this property.


Additional optional properties:

* `Identifier`: The default value is the unique string identifying this
  file (in the example above it was *Packages*) also accessible as
  `Created-By`. The difference is that using this property multiple files
  can be subsumed under one identifier e.g. if you configure multiple
  possible locations for the files (with `Fallback-Of`), but the front-end
  doesn't need to handle files from the different locations differently.
* `DefaultEnabled`: The default value is `yes` which means that apt will
  try to acquire this target from all sources. If set to `no` the user
  has to explicitly enable this target in the sources.list file with the
  `Targets` option(s) – or override this value in a config file.
* `Optional`: The default value is `yes` and should be kept at this value.
  If enabled the acquire system will skip the download if the file isn't
  mentioned in the `Release` file. Otherwise this is treated as a hard
  error and the update process fails. Note that failures while
  downloading (e.g. 404 or hash verification errors) are failures,
  regardless of this setting.
* `KeepCompressed`: The default is the value of `Acquire::GzipIndexes`,
  which defaults to `false`. If `true`, the acquire system will keep the
  file compressed on disk rather than extract it. If your front-end can't
  deal with compressed files transparently you have to explicitly set
  this option to `false` to avoid problems with users setting the option
  globally. On the other hand, if you set it to `true` or don't set it you
  have to ensure your front-end can deal with all compressed file formats
  supported by apt (libapt users can e.g. use `FileFd`, others can use
  the `cat-file` command of `/usr/lib/apt/apt-helper`).
* `Fallback-Of`: Is by default not set. If it is set and specifies another
  target name (see `Created-By`) which was found in the *Release* file the
  download of this target will be skipped. This can be used to implement
  fallback(chain)s to allow transitions like the rename of target files.
  The behavior if cycles are formed with Fallback-Of is undefined!
* `flatMetaKey`, `flatDescription`: APT supports two types of repositories:
  dists-style repositories which are the default and by far the most
  common which are named after the fact that the files are in an
  elaborated directory structure.  In contrast a flat-style repository
  lumps all files together in one directory.  Support for these flat
  repositories exists mainly for legacy purposes only.  It is therefore
  recommend to not set these values.


The acquire system will automatically choose to download a compressed
file if it is available and uncompress it for you, just as it will also
use PDiff patching if provided by the repository and enabled by the
user. You only have to ensure that the Release file contains the
information about the compressed files/PDiffs to make this happen.
**NO** properties have to be set to enable this!


More properties exist, but these should **NOT** be set by front-ends
requesting files. They exist for internal and end-user usage only.
Some of these are – which are documented here only to ensure that they
aren't accidentally used by front-ends:

* `PDiffs`: controls if apt will try to use PDiffs for this target.
  Defaults to the value of `Acquire::PDiffs` which is *true* by default.
  Can be overridden per-source by the sources.list option of the same
  name. See the documentation for both of these for details.
* `By-Hash`: controls if apt will try to use an URI constructed from
  a hashsum of the file to download. See the documentation for config
  option `Acquire::By-Hash` and sources.list option `By-Hash` for details.
* `CompressionTypes`: The default value is a space separated list of
  compression types supported by apt (see `Acquire::CompressionTypes`).
  You can set this option to prevent apt from downloading a compression
  type a front-end can't open transparently. This should always be
  a temporary workaround through and a bug should be reported against
  the front-end in question.
* `KeepCompressedAs`: The default value is a space separated list of
  compression types supported by apt (see previous option) which is
  sorted by the cost-value of the compression in ascending order,
  except that cost=0 "compressions" (like uncompressed) are listed last.


# More examples

The stanzas for `Translation-*` files as well as for `Sources` files would
look like this:

Acquire::IndexTargets {
	deb::Translations {
		MetaKey "$(COMPONENT)/i18n/Translation-$(LANGUAGE)";
		ShortDescription "Translation-$(LANGUAGE)";
		Description "$(RELEASE)/$(COMPONENT) Translation-$(LANGUAGE)";

		flatMetaKey "$(LANGUAGE)";
		flatDescription "$(RELEASE) Translation-$(LANGUAGE)";
	};

	deb-src::Sources {
		MetaKey "$(COMPONENT)/source/Sources";
		ShortDescription "Sources";
		Description "$(RELEASE)/$(COMPONENT) Sources";

		flatMetaKey "Sources";
		flatDescription "$(RELEASE) Sources";

		Optional "no";
	};
};

# Substitution variables

As seen in the examples, properties can contain placeholders filled in
by the acquire system. The following variables are known; note that
unknown variables have no default value nor are they touched: They are
printed as-is.

* `$(RELEASE)`: This is usually an archive- or codename, e.g. *stable* or
  *stretch*.  Note that flat-style repositories do not have an archive-
  or codename per-se, so the value might very well be just "/" or so.
* `$(COMPONENT)`: as given in the sources.list, e.g. *main*, *non-free* or
  *universe*.  Note that flat-style repositories again do not really
  have a meaningful value here.
* `$(LANGUAGE)`: Values are all entries (expect *none*) of configuration
  option `Acquire::Languages`, e.g. *en*, *de* or *de_AT*.
* `$(ARCHITECTURE)`: Values are all entries of configuration option
  `APT::Architectures` (potentially modified by sources.list options),
  e.g. *amd64*, *i386* or *armel* for the *deb* type. In type *deb-src*
  this variable has the value *source*.
* `$(NATIVE_ARCHITECTURE)`: The architecture apt treats as the native
  architecture for this system configured as `APT::Architecture`
  defaulting to the architecture apt itself was built for.

Note that while more variables might exist in the implementation, these
are to be considered undefined and their usage strongly discouraged. If
you have a need for other variables contact us.

# Accessing files

Do **NOT** hardcode specific file locations, names or compression types in
your application! You will notice that the configuration options give
you no choice over where the downloaded files will be stored. This is by
design so multiple applications can download and use the same file
rather than each and every one of them potentially downloads and uses
its own copy somewhere on disk.

`apt-get indextargets` can be used to get the location as well as other
information about all files downloaded (aka: you will see *Packages*,
*Sources* and *Translation-* files here as well). Provide a line of the
default output format as parameter to filter out all entries which do
not have such a line. With `--format`, you can further more define your
own output style. The variables are what you see in the output, just all
uppercase and wrapped in `$()`, as in the configuration file.

To get all the filenames of all *Translation-en* files you can e.g. call:

	apt-get indextargets --format '$(FILENAME)' "Identifier: Translations" "Language: en"

The line-based filtering and the formatting is rather crude and feature-
less by design: The default format is Debian's standard format `deb822`
(in particular: Field names are case-insensitive and the order of fields
in the stanza is undefined), so instead of apt reimplementing powerful
filters and formatting for this command, it is recommend to use piping
and dedicated tools like `grep-dctrl` if you need more than the basics
provided.

Accessing this information via libapt is done by reading the
sources.lists (`pkgSourceList`), iterating over the metaIndex objects this
creates and calling `GetIndexTargets()` on them. See the source code of
`apt-get indextargets` for a complete example.

Note that by default targets are not listed if they weren't downloaded.
If you want to see all targets, you can use the `--no-release-info`, which
also removes the *Codename*, *Suite*, *Version*, *Origin*, *Label* and *Trusted*
fields from the output as these also display data which needs to be
downloaded first and could hence be inaccurate [on the pro-side: This
mode is faster as it doesn't require a valid binary cache to operate].
The most notable difference perhaps is in the *Filename* field through: By
default it indicates an existing file, potentially compressed (Hint:
libapt users can use `FileFd` to open compressed files transparently). In
the `--no-release-info` mode the indicated file doesn't need to exist and
it will always refer to an uncompressed file, even if the index would be
(or is) stored compressed.

Remarks on fields only available in (default) `--release-info mode`:

* `Trusted`: Denotes with a *yes* or *no* if the data in this file is
  authenticated by a trust chain rooted in a trusted gpg key. You should
  be careful with untrusted data and warn the user if you use it.
* `Codename`, `Suite`, `Version`, `Origin` and `Label` are fields from the
  *Release* file, are only present if they are present in the *Release* file
  and contain the same data.

Remarks on other available fields:

* `MetaKey`, `ShortDesc`, `Description`, `Site`, `Release`: as defined
  by the configuration and described further above.
* `Identifier`: Defaults to the value of `Created-By`, but can be set
  explicitly in the configuration (see above). Prefer this field over
  `Created-By` to subsume multiple file(location)s (see `Fallback-Of`).
* `Created-By`: configuration entity responsible for this target
* `Target-Of`: type of the sources.list entry
* `URI`, `Repo-URI`: avoid using. Contains potentially username/password.
  Prefer `Site`, especially for display.
* `Optional`, `DefaultEnabled`, `KeepCompressed`: Decode the options of the
  same name from the configuration.
* `Language`, `Architecture`, `Component`: as defined further above, but with
  the catch that they might be missing if they don't effect the target
  (aka: They weren't used while evaluating the `MetaKey` template).

Again, additional fields might be visible in certain implementations,
but you should avoid using them and instead talk to us about a portable
implementation.

# Multiple applications requiring the same files

It is highly encouraged that applications talk to each other and to us
about which files they require. It is usually best to have a common
package ship the configuration needed to get the files, but specific
needs might require specific solutions. Again: **talk to us**.

Bad things will happen if multiple front-ends request the same file(s)
via different targets, which is another reason why coordination is very
important!

# Acquiring files not mentioned in the Release file

You can't. This is by design as these files couldn't be verified to not
be modified in transit, corrupted by the download process or simple if
they are present at all on the server, which would require apt to probe
for them. APT did this in the past for legacy reasons, we do not intend
to go back to these dark times.

This is also why you can't request files from a different server. It
would have the additional problem that this server might not even be
accessible (e.g. proxy settings) or that local sources (file:/, cdrom:/)
start requesting online files…

In other words: We would be opening Pandora's box.

# Acquiring files to a specific location on disk

You can't by design to avoid multiple front-ends requesting the same file
to be downloaded to multiple different places on (different) disks
(among other reasons).  See the next point for a solution if you really
have to force a specific location by creating symlinks.

# Post processing the acquired files

You can't modify the files apt has downloaded as apt keeps state with
e.g. the modification times of the files and advanced features like
PDiffs break.

You can however install an `APT::Update::Post-Invoke{-Success,}` hook
script and use them to copy (modified) files to a different location.
Use `apt-get indextargets` (or similar) to get the filenames – do not
look into `/var/lib/apt/lists` directly!

Please avoid time consuming calculations in the scripts and instead just
trigger a background task as there is little to no feedback for the user
while hook scripts run.
