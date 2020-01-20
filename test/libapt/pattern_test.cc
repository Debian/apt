/*
 * cachefilter-patterns.h - Pattern parser and additional patterns as matchers
 *
 * Copyright (c) 2019 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <apt-pkg/cachefilter-patterns.h>
#include <apt-pkg/cachefilter.h>

#include <gtest/gtest.h>

using namespace APT::Internal;

TEST(TreeParserTest, ParseWord)
{
   auto node = PatternTreeParser("word").parseTop();
   auto wordNode = dynamic_cast<PatternTreeParser::WordNode *>(node.get());

   EXPECT_EQ(node.get(), wordNode);
   EXPECT_EQ(wordNode->word, "word");
}

TEST(TreeParserTest, ParseQuotedWord)
{
   auto node = PatternTreeParser("\"a word\"").parseTop();
   auto wordNode = dynamic_cast<PatternTreeParser::WordNode *>(node.get());

   EXPECT_EQ(node.get(), wordNode);
   EXPECT_EQ(wordNode->word, "a word");
}

TEST(TreeParserTest, ParsePattern)
{
   auto node = PatternTreeParser("?hello").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_TRUE(patternNode->arguments.empty());
   EXPECT_FALSE(patternNode->haveArgumentList);
}

TEST(TreeParserTest, ParseWithEmptyArgs)
{
   auto node = PatternTreeParser("?hello()").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_TRUE(patternNode->arguments.empty());
   EXPECT_TRUE(patternNode->haveArgumentList);
}

TEST(TreeParserTest, ParseWithOneArgs)
{
   auto node = PatternTreeParser("?hello(foo)").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_EQ(1u, patternNode->arguments.size());
}

TEST(TreeParserTest, ParseWithManyArgs)
{
   auto node = PatternTreeParser("?hello(foo,bar)").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_EQ(2u, patternNode->arguments.size());
}

TEST(TreeParserTest, ParseWithManyArgsWithSpaces)
{
   auto node = PatternTreeParser("?hello (foo, bar)").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_EQ(2u, patternNode->arguments.size());
}

TEST(TreeParserTest, ParseWithManyArgsWithSpacesWithTrailingComma)
{
   auto node = PatternTreeParser("?hello (foo, bar,)").parseTop();
   auto patternNode = dynamic_cast<PatternTreeParser::PatternNode *>(node.get());

   EXPECT_EQ(node.get(), patternNode);
   EXPECT_EQ(patternNode->term, "?hello");
   EXPECT_EQ(2u, patternNode->arguments.size());
}

// Helper
static bool samePattern(const std::unique_ptr<PatternTreeParser::Node> &a, const std::unique_ptr<PatternTreeParser::Node> &b)
{
   auto pa = dynamic_cast<const PatternTreeParser::PatternNode *>(a.get());
   auto pb = dynamic_cast<const PatternTreeParser::PatternNode *>(b.get());

   if (pa && pb)
   {
      if (pa->term != pb->term || pa->haveArgumentList != pb->haveArgumentList || pa->arguments.size() != pb->arguments.size())
	 return false;

      for (size_t i = 0; i < pa->arguments.size(); i++)
      {
	 if (!samePattern(pa->arguments[i], pb->arguments[i]))
	    return false;
      }
      return true;
   }

   auto wa = dynamic_cast<const PatternTreeParser::WordNode *>(a.get());
   auto wb = dynamic_cast<const PatternTreeParser::WordNode *>(b.get());
   if (wa && wb)
      return wa->word == wb->word && wa->quoted == wb->quoted;

   return false;
}

#define EXPECT_PATTERN_EQ(shrt, lng) \
   EXPECT_TRUE(samePattern(PatternTreeParser(shrt).parseTop(), PatternTreeParser(lng).parseTop()))
#define EXPECT_PATTERN_EQ_ATOMIC(shrt, lng)                           \
   EXPECT_TRUE(PatternTreeParser(shrt).parseTop());                   \
   caught = false;                                                    \
   try                                                                \
   {                                                                  \
      PatternTreeParser(shrt "XXX").parseTop();                       \
   }                                                                  \
   catch (PatternTreeParser::Error & e)                               \
   {                                                                  \
      caught = true;                                                  \
   };                                                                 \
   EXPECT_TRUE(caught) << shrt "XXX should have thrown an exception"; \
   EXPECT_PATTERN_EQ(shrt, lng)

TEST(TreeParserTest, ParseShortPattern)
{
   bool caught;
   EXPECT_PATTERN_EQ("~ramd64", "?architecture(amd64)");
   EXPECT_PATTERN_EQ("~AanArchive", "?archive(anArchive)");
   EXPECT_PATTERN_EQ_ATOMIC("~M", "?automatic");
   EXPECT_PATTERN_EQ_ATOMIC("~b", "?broken");
   EXPECT_PATTERN_EQ_ATOMIC("~c", "?config-files");
   EXPECT_PATTERN_EQ_ATOMIC("~E", "?essential");
   EXPECT_PATTERN_EQ_ATOMIC("~F", "?false");
   EXPECT_PATTERN_EQ_ATOMIC("~g", "?garbage");
   EXPECT_PATTERN_EQ_ATOMIC("~i", "?installed");
   EXPECT_PATTERN_EQ("~napt", "?name(apt)");
   EXPECT_PATTERN_EQ_ATOMIC("~o", "?obsolete");
   EXPECT_PATTERN_EQ("~Obar", "?origin(bar)");
   EXPECT_PATTERN_EQ("~sfoo", "?section(foo)");
   EXPECT_PATTERN_EQ("~esourcename", "?source-package(sourcename)");
   EXPECT_PATTERN_EQ_ATOMIC("~T", "?true");
   EXPECT_PATTERN_EQ_ATOMIC("~U", "?upgradable");
   EXPECT_PATTERN_EQ("~Vverstr", "?version(verstr)");
   EXPECT_PATTERN_EQ_ATOMIC("~v", "?virtual");
}
