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
   for(int i=0;i<100;i++)
   {
      std::vector<SrvRec> Meep;
      SrvRec foo = {target:"foo", priority: 20, weight: 0, port: 80};
      Meep.push_back(foo);
      
      SrvRec bar = {target:"bar", priority: 20, weight: 0, port: 80};
      Meep.push_back(bar);

      EXPECT_EQ(Meep.size(), 2);
      SrvRec result = PopFromSrvRecs(Meep);
      selected.insert(result.target);
      // ensure that pop removed one element
      EXPECT_EQ(Meep.size(), 1);
   }

   // ensure that after enough runs we end up with both selected
   EXPECT_EQ(selected.size(), 2);
}
