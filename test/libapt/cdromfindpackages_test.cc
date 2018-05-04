#include <config.h>

#include <apt-pkg/cdrom.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <stddef.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

class Cdrom : public pkgCdrom {
   public:
      bool FindPackages(std::string const &CD,
	    std::vector<std::string> &List,
	    std::vector<std::string> &SList,
	    std::vector<std::string> &SigList,
	    std::vector<std::string> &TransList,
	    std::string &InfoDir) {
	 std::string const startdir = SafeGetCWD();
	 EXPECT_FALSE(startdir.empty());
	 EXPECT_TRUE(InfoDir.empty());
	 bool const result = pkgCdrom::FindPackages(CD, List, SList, SigList, TransList, InfoDir, NULL, 0);
	 EXPECT_FALSE(InfoDir.empty());
	 std::sort(List.begin(), List.end());
	 std::sort(SList.begin(), SList.end());
	 std::sort(SigList.begin(), SigList.end());
	 std::sort(TransList.begin(), TransList.end());
	 EXPECT_EQ(0, chdir(startdir.c_str()));
	 return result;
      }

      using pkgCdrom::DropRepeats;
};

TEST(CDROMTest,FindPackages)
{
   std::string path;
   createTemporaryDirectory("findpackage", path);

   createDirectory(path, ".disk");
   createDirectory(path, "pool");
   createDirectory(path, "dists/stable/main/binary-i386");
   createDirectory(path, "dists/stable/main/source");
   createDirectory(path, "dists/stable/contrib/binary-amd64");
   createDirectory(path, "dists/stable/non-free/binary-all");
   createDirectory(path, "dists/unstable/main/binary-i386");
   createDirectory(path, "dists/unstable/main/i18n");
   createDirectory(path, "dists/unstable/main/source");
   createDirectory(path, "dists/broken/non-free/source");
   createFile(path, "dists/broken/.aptignr");
   createFile(path, "dists/stable/main/binary-i386/Packages");
   createFile(path, "dists/stable/main/binary-i386/Packages.bz2");
   createFile(path, "dists/stable/main/source/Sources.xz");
   createFile(path, "dists/stable/contrib/binary-amd64/Packages");
   createFile(path, "dists/stable/contrib/binary-amd64/Packages.gz");
   createFile(path, "dists/stable/non-free/binary-all/Packages");
   createFile(path, "dists/unstable/main/binary-i386/Packages.xz");
   createFile(path, "dists/unstable/main/binary-i386/Packages.lzma");
   createFile(path, "dists/unstable/main/i18n/Translation-en");
   createFile(path, "dists/unstable/main/i18n/Translation-de.bz2");
   createFile(path, "dists/unstable/main/source/Sources.xz");
   createFile(path, "dists/broken/non-free/source/Sources.gz");
   createFile(path, "dists/stable/Release.gpg");
   createFile(path, "dists/stable/Release");
   createFile(path, "dists/unstable/InRelease");
   createFile(path, "dists/broken/Release.gpg");
   createLink(path, "dists/unstable", "dists/sid");

   Cdrom cd;
   std::vector<std::string> Packages, Sources, Signatur, Translation;
   std::string InfoDir;
   EXPECT_TRUE(cd.FindPackages(path, Packages, Sources, Signatur, Translation, InfoDir));
   EXPECT_EQ(5u, Packages.size());
   EXPECT_EQ(path + "/dists/sid/main/binary-i386/", Packages[0]);
   EXPECT_EQ(path + "/dists/stable/contrib/binary-amd64/", Packages[1]);
   EXPECT_EQ(path + "/dists/stable/main/binary-i386/", Packages[2]);
   EXPECT_EQ(path + "/dists/stable/non-free/binary-all/", Packages[3]);
   EXPECT_EQ(path + "/dists/unstable/main/binary-i386/", Packages[4]);
   EXPECT_EQ(3u, Sources.size());
   EXPECT_EQ(path + "/dists/sid/main/source/", Sources[0]);
   EXPECT_EQ(path + "/dists/stable/main/source/", Sources[1]);
   EXPECT_EQ(path + "/dists/unstable/main/source/", Sources[2]);
   EXPECT_EQ(3u, Signatur.size());
   EXPECT_EQ(path + "/dists/sid/", Signatur[0]);
   EXPECT_EQ(path + "/dists/stable/", Signatur[1]);
   EXPECT_EQ(path + "/dists/unstable/", Signatur[2]);
   EXPECT_EQ(4u, Translation.size());
   EXPECT_EQ(path + "/dists/sid/main/i18n/Translation-de", Translation[0]);
   EXPECT_EQ(path + "/dists/sid/main/i18n/Translation-en", Translation[1]);
   EXPECT_EQ(path + "/dists/unstable/main/i18n/Translation-de", Translation[2]);
   EXPECT_EQ(path + "/dists/unstable/main/i18n/Translation-en", Translation[3]);
   EXPECT_EQ(path + "/.disk/", InfoDir);

   cd.DropRepeats(Packages, "Packages");
   cd.DropRepeats(Sources, "Sources");
   _error->PushToStack();
   cd.DropRepeats(Signatur, "InRelease");
   cd.DropRepeats(Signatur, "Release.gpg");
   _error->RevertToStack();
   _error->DumpErrors();
   cd.DropRepeats(Translation, "");

   EXPECT_EQ(4u, Packages.size());
   EXPECT_EQ(path + "/dists/stable/contrib/binary-amd64/", Packages[0]);
   EXPECT_EQ(path + "/dists/stable/main/binary-i386/", Packages[1]);
   EXPECT_EQ(path + "/dists/stable/non-free/binary-all/", Packages[2]);
   EXPECT_EQ(path + "/dists/unstable/main/binary-i386/", Packages[3]);
   EXPECT_EQ(2u, Sources.size());
   EXPECT_EQ(path + "/dists/stable/main/source/", Sources[0]);
   EXPECT_EQ(path + "/dists/unstable/main/source/", Sources[1]);
   EXPECT_EQ(2u, Signatur.size());
   EXPECT_EQ(path + "/dists/stable/", Signatur[0]);
   EXPECT_EQ(path + "/dists/unstable/", Signatur[1]);
   EXPECT_EQ(2u, Translation.size());
   EXPECT_EQ(path + "/dists/unstable/main/i18n/Translation-de", Translation[0]);
   EXPECT_EQ(path + "/dists/unstable/main/i18n/Translation-en", Translation[1]);

   removeDirectory(path);
}
