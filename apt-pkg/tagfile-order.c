/* In this file is the order defined in which e.g. apt-ftparchive will write stanzas in.
   Other commands might or might not use this. 'apt-cache show' e.g. does NOT!

   The order we chose here is inspired by both dpkg and dak.
   The testcase test/integration/test-apt-tagfile-fields-order intends to ensure that
   this file isn't lacking (too far) behind dpkg over time. */

static const char *iTFRewritePackageOrder[] = {
   "Package",
   "Package-Type",
   "Architecture",
   "Subarchitecture", // Used only by d-i
   "Version",
   "Revision",         // Obsolete (warning in dpkg)
   "Package-Revision", // Obsolete (warning in dpkg)
   "Package_Revision", // Obsolete (warning in dpkg)
   "Kernel-Version", // Used only by d-i
   "Built-Using",
   "Built-For-Profiles",
   "Auto-Built-Package",
   "Multi-Arch",
   "Status",
   "Priority",
   "Class", // dpkg nickname for Priority
   "Essential",
   "Installer-Menu-Item", // Used only by d-i
   "Section",
   "Source",
   "Origin",
   "Maintainer",
   "Original-Maintainer", // unknown in dpkg order
   "Bugs",
   "Config-Version", // Internal of dpkg
   "Conffiles",
   "Triggers-Awaited",
   "Triggers-Pending",
   "Installed-Size",
   "Provides",
   "Pre-Depends",
   "Depends",
   "Recommends",
   "Recommended", // dpkg nickname for Recommends
   "Suggests",
   "Optional", // dpkg nickname for Suggests
   "Conflicts",
   "Breaks",
   "Replaces",
   "Enhances",
   "Filename",
   "MSDOS-Filename", // Obsolete (used by dselect)
   "Size",
   "MD5sum",
   "SHA1",
   "SHA256",
   "SHA512",
   "Homepage",
   "Description",
   "Tag",
   "Task",
   0
};
static const char *iTFRewriteSourceOrder[] = {
   "Package",
   "Source", // dsc file, renamed to Package in Sources
   "Format",
   "Binary",
   "Architecture",
   "Version",
   "Priority",
   "Class", // dpkg nickname for Priority
   "Section",
   "Origin",
   "Maintainer",
   "Original-Maintainer", // unknown in dpkg order
   "Uploaders",
   "Dm-Upload-Allowed", // Obsolete (ignored by dak)
   "Standards-Version",
   "Build-Depends",
   "Build-Depends-Arch",
   "Build-Depends-Indep",
   "Build-Conflicts",
   "Build-Conflicts-Arch",
   "Build-Conflicts-Indep",
   "Testsuite",
   "Testsuite-Triggers",
   "Homepage",
   "Vcs-Browser",
   "Vcs-Browse", // dak only (nickname?)
   "Vcs-Arch",
   "Vcs-Bzr",
   "Vcs-Cvs",
   "Vcs-Darcs",
   "Vcs-Git",
   "Vcs-Hg",
   "Vcs-Mtn",
   "Vcs-Svn",
   "Directory",
   "Package-List",
   "Files",
   "Checksums-Md5",
   "Checksums-Sha1",
   "Checksums-Sha256",
   "Checksums-Sha512",
   0
};

/* Two levels of initialization are used because gcc will set the symbol
   size of an array to the length of the array, causing dynamic relinking
   errors. Doing this makes the symbol size constant */
const char **TFRewritePackageOrder = iTFRewritePackageOrder;
const char **TFRewriteSourceOrder = iTFRewriteSourceOrder;
