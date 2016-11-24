#include <config.h>

#include <string>
#include <sstream>
#include <fstream>
#include "../interactive-helper/teestream.h"

#include <gtest/gtest.h>

TEST(TeeStreamTest,TwoStringSinks)
{
   std::ostringstream one, two;
   basic_teeostream<char> tee(one, two);
   tee << "This is the " << 1 << '.' << " Test, we expect: " << std::boolalpha << true << "\n";
   std::string okay("This is the 1. Test, we expect: true\n");
   EXPECT_EQ(okay, one.str());
   EXPECT_EQ(okay, two.str());
   EXPECT_EQ(one.str(), two.str());
}

TEST(TeeStreamTest,DevNullSink1)
{
   std::ostringstream one;
   std::fstream two("/dev/null");
   basic_teeostream<char> tee(one, two);
   tee << "This is the " << 2 << '.' << " Test, we expect: " << std::boolalpha << false << "\n";
   std::string okay("This is the 2. Test, we expect: false\n");
   EXPECT_EQ(okay, one.str());
}

TEST(TeeStreamTest,DevNullSink2)
{
   std::ostringstream one;
   std::fstream two("/dev/null");
   basic_teeostream<char> tee(two, one);
   tee << "This is the " << 3 << '.' << " Test, we expect: " << std::boolalpha << false << "\n";
   std::string okay("This is the 3. Test, we expect: false\n");
   EXPECT_EQ(okay, one.str());
}
