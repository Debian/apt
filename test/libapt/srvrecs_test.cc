#include <config.h>

#include <apt-pkg/srvrec.h>

#include <string>
#include <iostream>

#include <gtest/gtest.h>

TEST(SrvRecTest, PopFromSrvRecs)
{
   // the PopFromSrvRecs() is using a random number so we must
   // run it a bunch of times to ensure we are not fooled by randomness
   std::set<std::string> selected;
   for(size_t i = 0; i < 100; ++i)
   {
      std::vector<SrvRec> Meep;
      Meep.emplace_back("foo", 20, 0, 80);
      Meep.emplace_back("bar", 20, 0, 80);
      Meep.emplace_back("baz", 30, 0, 80);

      EXPECT_EQ(Meep.size(), 3);
      SrvRec const result = PopFromSrvRecs(Meep);
      selected.insert(result.target);
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

   // ensure that after enough runs we end up with both selected
   EXPECT_EQ(selected.size(), 2);
}
