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
