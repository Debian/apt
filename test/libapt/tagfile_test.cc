#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>

#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
}
