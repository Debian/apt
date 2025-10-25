#include <config.h>

#include <apt-private/private-cmndline.h>

#include "common.h"

TEST(CliVersionTest, TestShort)
{

   EXPECT_TRUE(cliVersionIsCompatible("0.0", "0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.1", "0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.0", "1"));
   EXPECT_TRUE(cliVersionIsCompatible("0.1", "1"));
   EXPECT_FALSE(cliVersionIsCompatible("0.9", "0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.9", "1"));
   EXPECT_FALSE(cliVersionIsCompatible("0.10", "1"));
}

TEST(CliVersionTest, Test_0_10)
{
   EXPECT_TRUE(cliVersionIsCompatible("0.0", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.1", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.2", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.3", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.4", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.5", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.6", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.7", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.8", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.10", "0.10"));
   // 0.9 does not migrate to 0.10
   EXPECT_FALSE(cliVersionIsCompatible("0.9", "0.10"));
   // 0.11 is newer than 0.10
   EXPECT_FALSE(cliVersionIsCompatible("0.11", "0.10"));
}

TEST(CliVersionTest, TestSame)
{
   EXPECT_TRUE(cliVersionIsCompatible("0.0", "0.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.1", "0.1"));
   EXPECT_TRUE(cliVersionIsCompatible("0.2", "0.2"));
   EXPECT_TRUE(cliVersionIsCompatible("0.3", "0.3"));
   EXPECT_TRUE(cliVersionIsCompatible("0.4", "0.4"));
   EXPECT_TRUE(cliVersionIsCompatible("0.5", "0.5"));
   EXPECT_TRUE(cliVersionIsCompatible("0.6", "0.6"));
   EXPECT_TRUE(cliVersionIsCompatible("0.7", "0.7"));
   EXPECT_TRUE(cliVersionIsCompatible("0.8", "0.8"));
   EXPECT_TRUE(cliVersionIsCompatible("0.9", "0.9"));
   EXPECT_TRUE(cliVersionIsCompatible("0.10", "0.10"));
   EXPECT_TRUE(cliVersionIsCompatible("0.11", "0.11"));
}
TEST(CliVersionTest, Test_1_0)
{
   EXPECT_TRUE(cliVersionIsCompatible("0.0", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.1", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.2", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.3", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.4", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.5", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.6", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.7", "1.0"));
   EXPECT_TRUE(cliVersionIsCompatible("0.8", "1.0"));
   // 0.9 *does* migrate to 0.10
   EXPECT_TRUE(cliVersionIsCompatible("0.9", "1.0"));
   // 0.11 is a continuation of the 0 branch in parallel to 1.1
   EXPECT_FALSE(cliVersionIsCompatible("0.10", "1.0"));
   EXPECT_FALSE(cliVersionIsCompatible("0.11", "1.0"));
}
