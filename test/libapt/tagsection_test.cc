#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>

#include <string>
#include <sstream>

#include <gtest/gtest.h>

#include "file-helpers.h"

std::string packageValue = "aaaa";
std::string typoValue = "aa\n"
   " .\n"
   " cc";
std::string typoRawValue = "\n " + typoValue;
std::string overrideValue = "1";
/*
   std::cerr << "FILECONTENT: »";
   char buffer[3000];
   while (fd.ReadLine(buffer, sizeof(buffer)))
      std::cerr << buffer;
   std::cerr << "«" << std::endl;;
*/

static void setupTestcaseStart(FileFd &fd, pkgTagSection &section, std::string &content)
{
   createTemporaryFile("writesection", fd, NULL, NULL);
   content = "Package: " + packageValue + "\n"
      "TypoA:\n " + typoValue + "\n"
      "Override: " + overrideValue + "\n"
      "Override-Backup: " + overrideValue + "\n"
      "\n";
   EXPECT_TRUE(section.Scan(content.c_str(), content.length(), true));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoA"));
   EXPECT_EQ(typoRawValue, section.FindRawS("TypoA"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteUnmodified)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   EXPECT_TRUE(section.Write(fd));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoA"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteUnmodifiedOrder)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   char const * const order[] = { "Package", "TypoA", "Override", NULL };
   EXPECT_TRUE(section.Write(fd, order));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoA"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteUnmodifiedOrderReversed)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   char const * const order[] = { "Override", "TypoA", "Package", NULL };
   EXPECT_TRUE(section.Write(fd, order));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoA"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteUnmodifiedOrderNotAll)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   char const * const order[] = { "Override", NULL };
   EXPECT_TRUE(section.Write(fd, order));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoA"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteNoOrderRename)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Rename("TypoA", "TypoB"));
   EXPECT_TRUE(section.Write(fd, NULL, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("TypoB"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoB"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteNoOrderRemove)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Remove("TypoA"));
   rewrite.push_back(pkgTagSection::Tag::Rewrite("Override", ""));
   EXPECT_TRUE(section.Write(fd, NULL, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("TypoA"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_FALSE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(2, section.Count());
}
TEST(TagSectionTest,WriteNoOrderRewrite)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Rewrite("Override", "42"));
   EXPECT_TRUE(section.Write(fd, NULL, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(42, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteOrderRename)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Rename("TypoA", "TypoB"));
   char const * const order[] = { "Package", "TypoA", "Override", NULL };
   EXPECT_TRUE(section.Write(fd, order, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("TypoA"));
   EXPECT_TRUE(section.Exists("TypoB"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(typoValue, section.FindS("TypoB"));
   EXPECT_EQ(1, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
TEST(TagSectionTest,WriteOrderRemove)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Remove("TypoA"));
   rewrite.push_back(pkgTagSection::Tag::Rewrite("Override", ""));
   char const * const order[] = { "Package", "TypoA", "Override", NULL };
   EXPECT_TRUE(section.Write(fd, order, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_FALSE(section.Exists("TypoA"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_FALSE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(2, section.Count());
}
TEST(TagSectionTest,WriteOrderRewrite)
{
   FileFd fd;
   pkgTagSection section;
   std::string content;
   setupTestcaseStart(fd, section, content);
   std::vector<pkgTagSection::Tag> rewrite;
   rewrite.push_back(pkgTagSection::Tag::Rewrite("Override", "42"));
   char const * const order[] = { "Package", "TypoA", "Override", NULL };
   EXPECT_TRUE(section.Write(fd, order, rewrite));
   EXPECT_TRUE(fd.Seek(0));
   pkgTagFile tfile(&fd);
   ASSERT_TRUE(tfile.Step(section));
   EXPECT_TRUE(section.Exists("Package"));
   EXPECT_TRUE(section.Exists("TypoA"));
   EXPECT_FALSE(section.Exists("TypoB"));
   EXPECT_TRUE(section.Exists("Override"));
   EXPECT_TRUE(section.Exists("Override-Backup"));
   EXPECT_EQ(packageValue, section.FindS("Package"));
   EXPECT_EQ(42, section.FindI("Override"));
   EXPECT_EQ(1, section.FindI("Override-Backup"));
   EXPECT_EQ(4, section.Count());
}
