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
   constexpr APT::StringView defString("foo", 3);
   static_assert( 3 == defString.length(), "def right size");
   EXPECT_EQ(defString.to_string(), defString.data());

   APT::StringView strString{std::string{"foo"}};
   EXPECT_EQ(3, strString.length());
   EXPECT_EQ(strString.to_string(), strString.data());

   constexpr char const * const charp = "foo";
   constexpr APT::StringView charpString{charp, 3};
   EXPECT_EQ( 3, charpString.length());
   EXPECT_EQ(charpString.to_string(), charpString.data());

   APT::StringView charp2String{charp};
   EXPECT_EQ(3, charp2String.length());
   EXPECT_EQ(charp2String.to_string(), charp2String.data());

   const APT::StringView charaString{"foo"};
   EXPECT_EQ(3, charaString.length());
   EXPECT_EQ(charaString.to_string(), charaString.data());

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
