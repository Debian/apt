#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/indexcopy.h>

#include <string>
#include <stdio.h>

#include <gtest/gtest.h>

class NoCopy : private IndexCopy {
   public:
      std::string ConvertToSourceList(std::string const &CD,std::string &&Path) {
	 IndexCopy::ConvertToSourceList(CD, Path);
	 return Path;
      }
      bool GetFile(std::string &/*Filename*/, unsigned long long &/*Size*/) APT_OVERRIDE { return false; }
      bool RewriteEntry(FileFd & /*Target*/, std::string const &/*File*/) APT_OVERRIDE { return false; }
      const char *GetFileName() APT_OVERRIDE { return NULL; }
      const char *Type() APT_OVERRIDE { return NULL; }

};

TEST(IndexCopyTest, ConvertToSourceList)
{
   NoCopy ic;
   std::string const CD("/media/cdrom/");

   char const * Releases[] = { "unstable", "wheezy-updates", NULL };
   char const * Components[] = { "main", "non-free", NULL };

   for (char const ** Release = Releases; *Release != NULL; ++Release)
   {
      SCOPED_TRACE(std::string("Release ") + *Release);
      for (char const ** Component = Components; *Component != NULL; ++Component)
      {
	 SCOPED_TRACE(std::string("Component ") + *Component);
	 std::string const Path = std::string("dists/") + *Release + "/" + *Component + "/";
	 std::string const Binary = Path + "binary-";
	 std::string const A = Binary + "armel/";
	 std::string const B = Binary + "mips/";
	 std::string const C = Binary + "kfreebsd-mips/";
	 std::string const S = Path + "source/";
	 std::string const List = std::string(*Release) + " " + *Component;

	 {
	 SCOPED_TRACE("no archs configured");
	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "all");
	 _config->Set("APT::Architectures::", "all");
	 APT::Configuration::getArchitectures(false);
	 EXPECT_EQ(A, ic.ConvertToSourceList("/media/cdrom/", CD + A));
	 EXPECT_EQ(B, ic.ConvertToSourceList("/media/cdrom/", CD + B));
	 EXPECT_EQ(C, ic.ConvertToSourceList("/media/cdrom/", CD + C));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + S));
	 }

	 {
	 SCOPED_TRACE("mips configured");
	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "mips");
	 _config->Set("APT::Architectures::", "mips");
	 APT::Configuration::getArchitectures(false);
	 EXPECT_EQ(A, ic.ConvertToSourceList("/media/cdrom/", CD + A));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + B));
	 EXPECT_EQ(C, ic.ConvertToSourceList("/media/cdrom/", CD + C));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + S));
	 }

	 {
	 SCOPED_TRACE("kfreebsd-mips configured");
	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "kfreebsd-mips");
	 _config->Set("APT::Architectures::", "kfreebsd-mips");
	 APT::Configuration::getArchitectures(false);
	 EXPECT_EQ(A, ic.ConvertToSourceList("/media/cdrom/", CD + A));
	 EXPECT_EQ(B, ic.ConvertToSourceList("/media/cdrom/", CD + B));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + C));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + S));
	 }

	 {
	 SCOPED_TRACE("armel configured");
	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "armel");
	 _config->Set("APT::Architectures::", "armel");
	 APT::Configuration::getArchitectures(false);
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + A));
	 EXPECT_EQ(B, ic.ConvertToSourceList("/media/cdrom/", CD + B));
	 EXPECT_EQ(C, ic.ConvertToSourceList("/media/cdrom/", CD + C));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + S));
	 }

	 {
	 SCOPED_TRACE("armel+mips configured");
	 _config->Clear("APT");
	 _config->Set("APT::Architecture", "armel");
	 _config->Set("APT::Architectures::", "armel");
	 _config->Set("APT::Architectures::", "mips");
	 APT::Configuration::getArchitectures(false);
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + A));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + B));
	 EXPECT_EQ(C, ic.ConvertToSourceList("/media/cdrom/", CD + C));
	 EXPECT_EQ(List, ic.ConvertToSourceList("/media/cdrom/", CD + S));
	 }
      }
   }
}
