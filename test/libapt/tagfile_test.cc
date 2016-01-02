#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>

#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sstream>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(TagFileTest,SingleField)
{
   FileFd fd;
   createTemporaryFile("singlefield", fd, NULL, "FieldA-12345678: the value of the field");

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   ASSERT_TRUE(tfile.Step(section));

   // It has one field
   EXPECT_EQ(1, section.Count());
   // ... and it is called FieldA-12345678
   EXPECT_TRUE(section.Exists("FieldA-12345678"));
   // its value is correct
   EXPECT_EQ("the value of the field", section.FindS("FieldA-12345678"));
   // A non-existent field has an empty string as value
   EXPECT_EQ("", section.FindS("FieldB-12345678"));
   // ... and Exists does not lie about missing fields...
   EXPECT_FALSE(section.Exists("FieldB-12345678"));
   // There is only one section in this tag file
   EXPECT_FALSE(tfile.Step(section));

   // Now we scan an empty section to test reset
   ASSERT_TRUE(section.Scan("\n\n", 2, true));
   EXPECT_EQ(0, section.Count());
   EXPECT_FALSE(section.Exists("FieldA-12345678"));
   EXPECT_FALSE(section.Exists("FieldB-12345678"));

   createTemporaryFile("emptyfile", fd, NULL, NULL);
   ASSERT_FALSE(tfile.Step(section));
   EXPECT_EQ(0, section.Count());
}

TEST(TagFileTest,MultipleSections)
{
   FileFd fd;
   createTemporaryFile("bigsection", fd, NULL, "Package: pkgA\n"
	 "Version: 1\n"
	 "Size: 100\n"
	 "Description: aaa\n"
	 " aaa\n"
	 "\n"
	 "Package: pkgB\n"
	 "Version: 1\n"
	 "Flag: no\n"
	 "Description: bbb\n"
	 "\n"
	 "Package: pkgC\n"
	 "Version: 2\n"
	 "Flag: yes\n"
	 "Description:\n"
	 " ccc\n"
	 );

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   EXPECT_FALSE(section.Exists("Version"));

   EXPECT_TRUE(tfile.Step(section));
   EXPECT_EQ(4, section.Count());
   EXPECT_TRUE(section.Exists("Version"));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("Size"));
   EXPECT_FALSE(section.Exists("Flag"));
   EXPECT_TRUE(section.Exists("Description"));
   EXPECT_EQ("pkgA", section.FindS("Package"));
   EXPECT_EQ("1", section.FindS("Version"));
   EXPECT_EQ(1, section.FindULL("Version"));
   EXPECT_EQ(100, section.FindULL("Size"));
   unsigned long Flags = 1;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(1, Flags);
   Flags = 0;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(0, Flags);
   EXPECT_EQ("aaa\n aaa", section.FindS("Description"));


   EXPECT_TRUE(tfile.Step(section));
   EXPECT_EQ(4, section.Count());
   EXPECT_TRUE(section.Exists("Version"));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("Size"));
   EXPECT_TRUE(section.Exists("Flag"));
   EXPECT_TRUE(section.Exists("Description"));
   EXPECT_EQ("pkgB", section.FindS("Package"));
   EXPECT_EQ("1", section.FindS("Version"));
   EXPECT_EQ(1, section.FindULL("Version"));
   EXPECT_EQ(0, section.FindULL("Size"));
   Flags = 1;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(0, Flags);
   Flags = 0;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(0, Flags);
   EXPECT_EQ("bbb", section.FindS("Description"));

   EXPECT_TRUE(tfile.Step(section));
   EXPECT_EQ(4, section.Count());
   EXPECT_TRUE(section.Exists("Version"));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("Size"));
   EXPECT_TRUE(section.Exists("Flag"));
   EXPECT_TRUE(section.Exists("Description"));
   EXPECT_EQ("pkgC", section.FindS("Package"));
   EXPECT_EQ("2", section.FindS("Version"));
   EXPECT_EQ(2, section.FindULL("Version"));
   Flags = 0;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(1, Flags);
   Flags = 1;
   EXPECT_TRUE(section.FindFlag("Flag", Flags, 1));
   EXPECT_EQ(1, Flags);
   EXPECT_EQ("ccc", section.FindS("Description"));

   // There is no section left in this tag file
   EXPECT_FALSE(tfile.Step(section));
}

TEST(TagFileTest,BigSection)
{
   size_t const count = 500;
   std::stringstream content;
   for (size_t i = 0; i < count; ++i)
      content << "Field-" << i << ": " << (2000 + i) << std::endl;

   FileFd fd;
   createTemporaryFile("bigsection", fd, NULL, content.str().c_str());

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   EXPECT_TRUE(tfile.Step(section));

   EXPECT_EQ(count, section.Count());
   for (size_t i = 0; i < count; ++i)
   {
      std::stringstream name;
      name << "Field-" << i;
      EXPECT_TRUE(section.Exists(name.str().c_str())) << name.str() << " does not exist";
      EXPECT_EQ((2000 + i), section.FindULL(name.str().c_str()));
   }

   // There is only one section in this tag file
   EXPECT_FALSE(tfile.Step(section));
}

TEST(TagFileTest, PickedUpFromPreviousCall)
{
   size_t const count = 500;
   std::stringstream contentstream;
   for (size_t i = 0; i < count; ++i)
      contentstream << "Field-" << i << ": " << (2000 + i) << std::endl;
   contentstream << std::endl << std::endl;
   std::string content = contentstream.str();

   pkgTagSection section;
   EXPECT_FALSE(section.Scan(content.c_str(), content.size()/2));
   EXPECT_NE(0, section.Count());
   EXPECT_NE(count, section.Count());
   EXPECT_TRUE(section.Scan(content.c_str(), content.size(), false));
   EXPECT_EQ(count, section.Count());

   for (size_t i = 0; i < count; ++i)
   {
      std::stringstream name;
      name << "Field-" << i;
      EXPECT_TRUE(section.Exists(name.str().c_str())) << name.str() << " does not exist";
      EXPECT_EQ((2000 + i), section.FindULL(name.str().c_str()));
   }
}

TEST(TagFileTest, SpacesEverywhere)
{
   std::string content =
      "Package: pkgA\n"
      "Package: pkgB\n"
      "NoSpaces:yes\n"
      "NoValue:\n"
      "TagSpaces\t    :yes\n"
      "ValueSpaces:   \tyes\n"
      "BothSpaces     \t:\t   yes\n"
      "TrailingSpaces: yes\t   \n"
      "Naming Space: yes\n"
      "Naming  Spaces: yes\n"
      "Package    :   pkgC    \n"
      "Multi-Colon::yes:\n"
      "\n\n";

   pkgTagSection section;
   EXPECT_TRUE(section.Scan(content.c_str(), content.size()));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("NoSpaces"));
   EXPECT_TRUE(section.Exists("NoValue"));
   EXPECT_TRUE(section.Exists("TagSpaces"));
   EXPECT_TRUE(section.Exists("ValueSpaces"));
   EXPECT_TRUE(section.Exists("BothSpaces"));
   EXPECT_TRUE(section.Exists("TrailingSpaces"));
   EXPECT_TRUE(section.Exists("Naming Space"));
   EXPECT_TRUE(section.Exists("Naming  Spaces"));
   EXPECT_TRUE(section.Exists("Multi-Colon"));
   EXPECT_EQ("pkgC", section.FindS("Package"));
   EXPECT_EQ("yes", section.FindS("NoSpaces"));
   EXPECT_EQ("", section.FindS("NoValue"));
   EXPECT_EQ("yes", section.FindS("TagSpaces"));
   EXPECT_EQ("yes", section.FindS("ValueSpaces"));
   EXPECT_EQ("yes", section.FindS("BothSpaces"));
   EXPECT_EQ("yes", section.FindS("TrailingSpaces"));
   EXPECT_EQ("yes", section.FindS("Naming Space"));
   EXPECT_EQ("yes", section.FindS("Naming  Spaces"));
   EXPECT_EQ(":yes:", section.FindS("Multi-Colon"));
   // overridden values are still present, but not really accessible
   EXPECT_EQ(12, section.Count());
}

TEST(TagFileTest, Comments)
{
   FileFd fd;
   createTemporaryFile("commentfile", fd, NULL, "# Leading comments should be ignored.\n"
"\n"
"Source: foo\n"
"#Package: foo\n"
"Section: bar\n"
"#Section: overriden\n"
"Priority: optional\n"
"Build-Depends: debhelper,\n"
"# apt-utils, (temporarily disabled)\n"
" apt\n"
"\n"
"# Comments in the middle shouldn't result in extra blank paragraphs either.\n"
"\n"
"# Ditto.\n"
"\n"
"# A comment at the top of a paragraph should be ignored.\n"
"Package: foo\n"
"Architecture: any\n"
"Description: An awesome package\n"
"  # This should still appear in the result.\n"
"# this one shouldn't\n"
"  Blah, blah, blah. # but this again.\n"
"# A comment at the end of a paragraph should be ignored.\n"
"\n"
"# Trailing comments shouldn't cause extra blank paragraphs."
	 );

   pkgTagFile tfile(&fd, pkgTagFile::SUPPORT_COMMENTS, 1);
   pkgTagSection section;
   EXPECT_TRUE(tfile.Step(section));
   EXPECT_FALSE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("Source"));
   EXPECT_EQ("foo", section.FindS("Source"));
   EXPECT_TRUE(section.Exists("Section"));
   EXPECT_EQ("bar", section.FindS("Section"));
   EXPECT_TRUE(section.Exists("Priority"));
   EXPECT_EQ("optional", section.FindS("Priority"));
   EXPECT_TRUE(section.Exists("Build-Depends"));
   EXPECT_EQ("debhelper,\n apt", section.FindS("Build-Depends"));

   EXPECT_TRUE(tfile.Step(section));
   EXPECT_FALSE(section.Exists("Source"));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_EQ("foo", section.FindS("Package"));
   EXPECT_FALSE(section.Exists("Section"));
   EXPECT_TRUE(section.Exists("Architecture"));
   EXPECT_EQ("any", section.FindS("Architecture"));
   EXPECT_FALSE(section.Exists("Build-Depends"));
   EXPECT_TRUE(section.Exists("Description"));
   EXPECT_EQ("An awesome package\n  # This should still appear in the result.\n  Blah, blah, blah. # but this again.", section.FindS("Description"));

   EXPECT_FALSE(tfile.Step(section));
}
