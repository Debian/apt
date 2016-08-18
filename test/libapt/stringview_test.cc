#if !(defined APT_PKG_EXPOSE_STRING_VIEW)
	#define APT_PKG_EXPOSE_STRING_VIEW
#endif

#include <config.h>
#include <apt-pkg/string_view.h>
#include <string>

#include <type_traits>

#include <gtest/gtest.h>

TEST(StringViewTest,EmptyString)
{
   constexpr APT::StringView defString;
   static_assert( 0 == defString.length(), "def right size");

   APT::StringView strString{std::string{}};
   EXPECT_EQ(0, strString.length());

   constexpr char const * const charp = "";
   constexpr APT::StringView charpString{charp, 0};
   static_assert( 0 == charpString.length(), "charp right size");

   APT::StringView charp2String{charp};
   EXPECT_EQ(0, strString.length());

   const APT::StringView charaString{""};
   EXPECT_EQ(0, charaString.length());

   EXPECT_TRUE(APT::StringView("") == "");
   EXPECT_FALSE(APT::StringView("") != "");
}

TEST(StringViewTest,FooString)
{
   constexpr APT::StringView defString("fooGARBAGE", 3);
   static_assert( 3 == defString.length(), "def right size");
   EXPECT_EQ(0, defString.to_string().compare(0, defString.length(), defString.data(), 3));

   APT::StringView strString{std::string{"foo"}};
   EXPECT_EQ(3, strString.length());
   EXPECT_EQ(0, strString.to_string().compare(0, strString.length(), strString.data(), 3));

   constexpr char const * const charp = "fooGARBAGE";
   constexpr APT::StringView charpString{charp, 3};
   EXPECT_EQ(3, charpString.length());
   EXPECT_EQ(0, charpString.to_string().compare(0, charpString.length(), charpString.data(), 3));

   char * charp2 = strdup("foo");
   APT::StringView charp2String{charp2};
   EXPECT_EQ(3, charp2String.length());
   EXPECT_EQ(0, charp2String.to_string().compare(0, charp2String.length(), charp2String.data(), 3));
   free(charp2);

   const APT::StringView charaString{"foo"};
   EXPECT_EQ(3, charaString.length());
   EXPECT_EQ(0, charaString.to_string().compare(0, charaString.length(), charaString.data(), 3));

   EXPECT_TRUE(APT::StringView("foo") == "foo");
   EXPECT_FALSE(APT::StringView("foo") != "foo");
}

TEST(StringViewTest,SubStr)
{
   const APT::StringView defString("Hello World!");
   EXPECT_EQ(defString.to_string().substr(6), defString.substr(6).to_string());
   EXPECT_EQ(defString.to_string().substr(0,5), defString.substr(0,5).to_string());
   EXPECT_EQ(defString.to_string().substr(6,5), defString.substr(6,5).to_string());
}

TEST(StringViewTest,Find)
{
   const APT::StringView defString("Hello World!");
   EXPECT_EQ(defString.to_string().find('l'), defString.find('l'));
   EXPECT_EQ(defString.to_string().find('X'), defString.find('X'));
   EXPECT_EQ(defString.to_string().find('e',3), defString.find('e',3));
   EXPECT_EQ(defString.to_string().find('l',6), defString.find('l',6));
   EXPECT_EQ(defString.to_string().find('l',11), defString.find('l',11));
}

TEST(StringViewTest,RFind)
{
   const APT::StringView defString("Hello World!");
   EXPECT_EQ(defString.to_string().rfind('l'), defString.rfind('l'));
   EXPECT_EQ(defString.to_string().rfind('X'), defString.rfind('X'));
   EXPECT_EQ(defString.to_string().rfind('e',3), defString.rfind('e',3));
   EXPECT_EQ(defString.to_string().rfind('l',6), defString.rfind('l',6));
   EXPECT_EQ(defString.to_string().rfind('l',11), defString.rfind('l',11));
}
