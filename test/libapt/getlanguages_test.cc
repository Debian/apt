#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(LanguagesTest,Environment)
{
   _config->Clear();

   char const* env[2];
   env[0] = "de_DE.UTF-8";
   env[1] = "";

   std::vector<std::string> vec = APT::Configuration::getLanguages(false, false, env);
   ASSERT_EQ(3u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);

   // Special: Check if the cache is actually in use
   env[0] = "en_GB.UTF-8";
   vec = APT::Configuration::getLanguages(false, true, env);
   ASSERT_EQ(3u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);

   env[0] = "en_GB.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("en_GB", vec[0]);
   EXPECT_EQ("en", vec[1]);

   // esperanto
   env[0] = "eo.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   ASSERT_EQ(2u, vec.size());
   EXPECT_EQ("eo", vec[0]);
   EXPECT_EQ("en", vec[1]);

   env[0] = "tr_DE@euro";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("tr_DE", vec[0]);
   EXPECT_EQ("tr", vec[1]);
   EXPECT_EQ("en", vec[2]);

   env[0] = "de_NO";
   env[1] = "de_NO:en_GB:nb_NO:nb:no_NO:no:nn_NO:nn:da:sv:en";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(6u, vec.size());
   EXPECT_EQ("de_NO", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en_GB", vec[2]);
   EXPECT_EQ("nb_NO", vec[3]);
   EXPECT_EQ("nb", vec[4]);
   EXPECT_EQ("en", vec[5]);

   env[0] = "pt_PR.UTF-8";
   env[1] = "";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("pt_PR", vec[0]);
   EXPECT_EQ("pt", vec[1]);
   EXPECT_EQ("en", vec[2]);

   env[0] = "ast_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env); // bogus, but syntactical correct
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("ast_DE", vec[0]);
   EXPECT_EQ("ast", vec[1]);
   EXPECT_EQ("en", vec[2]);

   env[0] = "C";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("en", vec[0]);

   _config->Set("Acquire::Languages", "none");
   env[0] = "C";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_TRUE(vec.empty());

   _config->Set("Acquire::Languages", "environment");
   env[0] = "C";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("en", vec[0]);

   _config->Set("Acquire::Languages", "de");
   env[0] = "C";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("de", vec[0]);

   _config->Set("Acquire::Languages", "fr");
   env[0] = "ast_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("fr", vec[0]);

   _config->Set("Acquire::Languages", "environment,en");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);
   _config->Set("Acquire::Languages", "");

   _config->Set("Acquire::Languages::1", "environment");
   _config->Set("Acquire::Languages::2", "en");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);

   _config->Set("Acquire::Languages::3", "de");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(3u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);

   _config->Clear();
}

TEST(LanguagesTest,TranslationFiles)
{
   _config->Clear();
   _config->Set("Acquire::Languages::1", "environment");
   _config->Set("Acquire::Languages::2", "en");
   _config->Set("Acquire::Languages::3", "de");

   char const* env[2];
   env[0] = "de_DE.UTF-8";
   env[1] = "";

   std::string tempdir;
   createTemporaryDirectory("languages", tempdir);

#define createTranslation(lang) \
   createFile(tempdir, std::string("/ftp.de.debian.org_debian_dists_sid_main_i18n_Translation-").append(lang));

   createTranslation("tr");
   createTranslation("pt");
   createTranslation("se~");
   createTranslation("st.bak");
   createTranslation("ast_DE");
   createTranslation("tlh%5fDE");

   _config->Set("Dir::State::lists", tempdir);
   std::vector<std::string> vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(8u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);
   EXPECT_EQ("de", vec[1]);
   EXPECT_EQ("en", vec[2]);
   EXPECT_EQ("none", vec[3]);
   EXPECT_NE(vec.end(), std::find(vec.begin(), vec.end(), "pt"));
   EXPECT_NE(vec.end(), std::find(vec.begin(), vec.end(), "tr"));
   EXPECT_NE(vec.end(), std::find(vec.begin(), vec.end(), "ast_DE"));
   EXPECT_NE(vec.end(), std::find(vec.begin(), vec.end(), "tlh_DE"));
   EXPECT_NE(vec[4], vec[5]);
   EXPECT_NE(vec[4], vec[6]);
   EXPECT_NE(vec[4], vec[7]);
   EXPECT_NE(vec[5], vec[6]);
   EXPECT_NE(vec[5], vec[7]);
   EXPECT_NE(vec[6], vec[7]);

   _config->Set("Acquire::Languages", "none");
   vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("none", vec[0]);
   _config->Set("Acquire::Languages", "");

   _config->Set("Dir::State::lists", "/non-existing-dir");
   _config->Set("Acquire::Languages::1", "none");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_TRUE(vec.empty());
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(2u, vec.size());
   EXPECT_EQ("en", vec[0]);
   EXPECT_EQ("de", vec[1]);

   _config->Set("Acquire::Languages::1", "fr");
   _config->Set("Acquire::Languages", "de_DE");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(false, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("de_DE", vec[0]);

   _config->Set("Acquire::Languages", "none");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("none", vec[0]);

   _error->PushToStack();
   _config->Set("Acquire::Languages", "");
   //FIXME: Remove support for this deprecated setting
   _config->Set("APT::Acquire::Translation", "ast_DE");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(2u, vec.size());
   EXPECT_EQ("ast_DE", vec[0]);
   EXPECT_EQ("en", vec[1]);
   _config->Set("APT::Acquire::Translation", "none");
   env[0] = "de_DE.UTF-8";
   vec = APT::Configuration::getLanguages(true, false, env);
   EXPECT_EQ(1u, vec.size());
   EXPECT_EQ("en", vec[0]);

   // discard the deprecation warning for APT::Acquire::Translation
   if (_error->PendingError())
      _error->MergeWithStack();
   else
      _error->RevertToStack();

   EXPECT_EQ(0, system(std::string("rm -rf ").append(tempdir).c_str()));
   _config->Clear();
}
