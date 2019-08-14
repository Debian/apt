/*
 * cachefilter-patterns.h - Pattern parser and additional patterns as matchers
 *
 * Copyright (c) 2019 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef APT_CACHEFILTER_PATTERNS_H
#define APT_CACHEFILTER_PATTERNS_H
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/error.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <assert.h>
namespace APT
{

namespace Internal
{
/**
 * \brief PatternTreeParser parses the given sentence into a parse tree.
 *
 * The parse tree consists of nodes:
 *  - Word nodes which contains words or quoted words
 *  - Patterns, which represent ?foo and ?foo(...) patterns
 */
struct PatternTreeParser
{

   struct Node
   {
      size_t start = 0;
      size_t end = 0;

      virtual std::ostream &render(std::ostream &os) { return os; };
      std::nullptr_t error(std::string message);
   };

   struct Error : public std::exception
   {
      Node location;
      std::string message;

      Error(Node location, std::string message) : location(location), message(message) {}
      const char *what() const throw() override { return message.c_str(); }
   };

   struct PatternNode : public Node
   {
      APT::StringView term;
      std::vector<std::unique_ptr<Node>> arguments;
      bool haveArgumentList = false;

      std::ostream &render(std::ostream &stream) override;
      bool matches(APT::StringView name, int min, int max);
   };

   struct WordNode : public Node
   {
      APT::StringView word;
      bool quoted = false;
      std::ostream &render(std::ostream &stream) override;
   };

   struct State
   {
      off_t offset = 0;
   };

   APT::StringView sentence;
   State state;

   PatternTreeParser(APT::StringView sentence) : sentence(sentence){};
   off_t skipSpace()
   {
      while (sentence[state.offset] == ' ' || sentence[state.offset] == '\t' || sentence[state.offset] == '\r' || sentence[state.offset] == '\n')
	 state.offset++;
      return state.offset;
   };

   /// \brief Parse a complete pattern
   ///
   /// There may not be anything before or after the pattern, except for
   /// whitespace.
   std::unique_ptr<Node> parseTop();

   private:
   std::unique_ptr<Node> parse();
   std::unique_ptr<Node> parsePattern();
   std::unique_ptr<Node> parseWord();
   std::unique_ptr<Node> parseQuotedWord();
};

} // namespace Internal
} // namespace APT
#endif
