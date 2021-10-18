#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(ArchitecturesTest,SimpleLists)
{
   _config->Clear();
   std::vector<std::string> vec;

   _config->Set("APT::Architectures::1", "i386");
   _config->Set("APT::Architectures::2", "amd64");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);

   _config->Set("APT::Architecture", "i386");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);

   _config->Set("APT::Architectures::2", "");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(1u, vec.size());
   EXPECT_EQ("i386", vec[0]);

   _config->Set("APT::Architecture", "armel");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("armel", vec[0]);
   EXPECT_EQ("i386", vec[1]);

   _config->Set("APT::Architectures::2", "armel");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("armel", vec[1]);

   _config->Set("APT::Architectures", "armel,armhf");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("armel", vec[0]);
   EXPECT_EQ("armhf", vec[1]);
   _config->Clear();
}
TEST(ArchitecturesTest,Duplicates)
{
   _config->Clear();

   _config->Set("APT::Architecture", "armel");
   _config->Set("APT::Architectures::", "i386");
   _config->Set("APT::Architectures::", "amd64");
   _config->Set("APT::Architectures::", "i386");
   _config->Set("APT::Architectures::", "armel");
   _config->Set("APT::Architectures::", "i386");
   _config->Set("APT::Architectures::", "amd64");
   _config->Set("APT::Architectures::", "armel");
   _config->Set("APT::Architectures::", "armel");
   _config->Set("APT::Architectures::", "amd64");
   _config->Set("APT::Architectures::", "amd64");
   std::vector<std::string> vec = _config->FindVector("APT::Architectures");
   ASSERT_EQ(10u, vec.size());
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(3u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);
   EXPECT_EQ("armel", vec[2]);

   _config->Clear();
}
TEST(ArchitecturesTest,VeryForeign)
{
   _config->Clear();
   _config->Set("APT::Architectures::", "i386");
   _config->Set("APT::Architectures::", "amd64");
   _config->Set("APT::Architectures::", "armel");

   auto vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(3u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);
   EXPECT_EQ("armel", vec[2]);

   _config->Set("APT::BarbarianArchitectures::", "mipsel");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(3u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);
   EXPECT_EQ("armel", vec[2]);

   _config->Set("APT::BarbarianArchitectures::", "armel");
   vec = APT::Configuration::getArchitectures(false);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("i386", vec[0]);
   EXPECT_EQ("amd64", vec[1]);

   _config->Clear();
}
