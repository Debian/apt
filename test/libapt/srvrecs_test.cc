#include <config.h>

#include <apt-pkg/srvrec.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

TEST(SrvRecTest, PopFromSrvRecs)
{
   std::vector<SrvRec> Meep;
   Meep.emplace_back("foo", 20, 0, 80);
   Meep.emplace_back("bar", 20, 0, 80);
   Meep.emplace_back("baz", 30, 0, 80);

   EXPECT_EQ(Meep.size(), 3);
   SrvRec const result = PopFromSrvRecs(Meep);
   // ensure that pop removed one element
   EXPECT_EQ(Meep.size(), 2);
   EXPECT_NE(result.target, "baz");

   SrvRec const result2 = PopFromSrvRecs(Meep);
   EXPECT_NE(result.target, result2.target);
   EXPECT_NE(result2.target, "baz");
   EXPECT_EQ(Meep.size(), 1);

   SrvRec const result3 = PopFromSrvRecs(Meep);
   EXPECT_EQ(result3.target, "baz");
   EXPECT_TRUE(Meep.empty());
}

TEST(SrvRecTest,Randomness)
{
   constexpr unsigned int testLength = 100;
   std::vector<SrvRec> base1;
   std::vector<SrvRec> base2;
   std::vector<SrvRec> base3;
   for (unsigned int i = 0; i < testLength; ++i)
   {
      std::string name;
      strprintf(name, "foo%d", i);
      base1.emplace_back(name, 20, 0, 80);
      base2.emplace_back(name, 20, 0, 80);
      base3.emplace_back(name, 30, 0, 80);
   }
   EXPECT_EQ(testLength, base1.size());
   EXPECT_EQ(testLength, base2.size());
   EXPECT_EQ(testLength, base3.size());
   std::move(base3.begin(), base3.end(), std::back_inserter(base2));
   EXPECT_EQ(testLength*2, base2.size());

   std::vector<SrvRec> first_pull;
   auto const startingClock = clock();
   for (unsigned int i = 0; i < testLength; ++i)
      first_pull.push_back(PopFromSrvRecs(base1));
   EXPECT_TRUE(base1.empty());
   EXPECT_FALSE(first_pull.empty());
   EXPECT_EQ(testLength, first_pull.size());

   // busy-wait for a cpu-clock change as we use it as "random" value
   if (startingClock != -1)
      for (int i = 0; i < 100000; ++i)
	 if (startingClock != clock())
	    break;

   std::vector<SrvRec> second_pull;
   for (unsigned int i = 0; i < testLength; ++i)
      second_pull.push_back(PopFromSrvRecs(base2));
   EXPECT_FALSE(base2.empty());
   EXPECT_FALSE(second_pull.empty());
   EXPECT_EQ(testLength, second_pull.size());

   EXPECT_EQ(first_pull.size(), second_pull.size());
   EXPECT_TRUE(std::all_of(first_pull.begin(), first_pull.end(), [](SrvRec const &R) { return R.priority == 20; }));
   EXPECT_TRUE(std::all_of(second_pull.begin(), second_pull.end(), [](SrvRec const &R) { return R.priority == 20; }));
   if (startingClock != -1 && startingClock != clock())
      EXPECT_FALSE(std::equal(first_pull.begin(), first_pull.end(), second_pull.begin()));

   EXPECT_TRUE(std::all_of(base2.begin(), base2.end(), [](SrvRec const &R) { return R.priority == 30; }));
}
