#include <config.h>
#include "../../apt-private/private-cachefile.cc"
#include "../../apt-private/private-json-hooks.cc"
#include <gtest/gtest.h>
#include <string>

TEST(JsonTest, JsonString)
{
   std::ostringstream os;

   // Check for escaping backslash and quotation marks, and ensure that we do not change number formatting
   JsonWriter(os).value("H al\"l\\o").value(17);

   EXPECT_EQ("\"H al\\u0022l\\u005Co\"17", os.str());

   for (int i = 0; i <= 0x1F; i++)
   {
      os.str("");

      JsonWriter(os).encodeString(os, std::string("X") + char(i) + "Z");

      std::string exp;
      strprintf(exp, "\"X\\u%04XZ\"", i);

      EXPECT_EQ(exp, os.str());
   }
}

TEST(JsonTest, JsonObject)
{
   std::ostringstream os;

   JsonWriter(os).beginObject().name("key").value("value").endObject();

   EXPECT_EQ("{\"key\":\"value\"}", os.str());
}

TEST(JsonTest, JsonArrayAndValues)
{
   std::ostringstream os;

   JsonWriter(os).beginArray().value(0).value("value").value(1).value(true).endArray();

   EXPECT_EQ("[0,\"value\",1,true]", os.str());
}
