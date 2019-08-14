/*
 * cachefilter-patterns.cc - Parser for aptitude-style patterns
 *
 * Copyright (c) 2019 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <apt-pkg/cachefilter-patterns.h>

namespace APT
{
namespace Internal
{

template <class... Args>
std::string rstrprintf(Args... args)
{
   std::string str;
   strprintf(str, std::forward<Args>(args)...);
   return str;
}

// Parse a complete pattern, make sure it's the entire input
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseTop()
{
   skipSpace();
   auto node = parse();
   skipSpace();

   if (node->end != sentence.size())
   {
      Node node2;

      node2.start = node->end;
      node2.end = sentence.size();
      throw Error{node2, "Expected end of file"};
   }

   return node;
}

// Parse any pattern
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parse()
{
   std::unique_ptr<Node> node;
   if ((node = parsePattern()) != nullptr)
      return node;
   if ((node = parseQuotedWord()) != nullptr)
      return node;
   if ((node = parseWord()) != nullptr)
      return node;

   Node eNode;
   eNode.end = eNode.start = state.offset;
   throw Error{eNode, "Expected pattern, quoted word, or word"};
}

// Parse a list pattern (or function call pattern)
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parsePattern()
{
   static const APT::StringView CHARS("0123456789"
				      "abcdefghijklmnopqrstuvwxyz"
				      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				      "-");
   if (sentence[state.offset] != '?')
      return nullptr;

   auto node = std::make_unique<PatternNode>();
   node->end = node->start = state.offset;
   state.offset++;

   while (CHARS.find(sentence[state.offset]) != APT::StringView::npos)
   {
      ++state.offset;
   }

   node->term = sentence.substr(node->start, state.offset - node->start);

   node->end = skipSpace();
   // We don't have any arguments, return node;
   if (sentence[state.offset] != '(')
      return node;
   node->end = ++state.offset;
   skipSpace();

   node->haveArgumentList = true;

   // Empty argument list, return
   if (sentence[state.offset] == ')')
   {
      node->end = ++state.offset;
      return node;
   }

   node->arguments.push_back(parse());
   skipSpace();
   while (sentence[state.offset] == ',')
   {
      ++state.offset;
      skipSpace();
      // This was a trailing comma - allow it and break the loop
      if (sentence[state.offset] == ')')
	 break;
      node->arguments.push_back(parse());
      skipSpace();
   }

   if (sentence[state.offset] != ')')
      throw Error{*node, rstrprintf("Expected closing parenthesis, received %d", sentence[state.offset])};

   node->end = ++state.offset;
   return node;
}

// Parse a quoted word atom
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseQuotedWord()
{
   if (sentence[state.offset] != '"')
      return nullptr;

   auto node = std::make_unique<WordNode>();
   node->start = state.offset;

   // Eat beginning of string
   state.offset++;

   while (sentence[state.offset] != '"' && sentence[state.offset] != '\0')
      state.offset++;

   // End of string
   if (sentence[state.offset] != '"')
      throw Error{*node, "Could not find end of string"};
   state.offset++;

   node->end = state.offset;
   node->word = sentence.substr(node->start + 1, node->end - node->start - 2);

   return node;
}

// Parse a bare word atom
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseWord()
{
   static const APT::StringView CHARS("0123456789"
				      "abcdefghijklmnopqrstuvwxyz"
				      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				      "-.*^$[]_\\");
   if (CHARS.find(sentence[state.offset]) == APT::StringView::npos)
      return nullptr;

   auto node = std::make_unique<WordNode>();
   node->start = state.offset;

   while (CHARS.find(sentence[state.offset]) != APT::StringView::npos)
      state.offset++;

   node->end = state.offset;
   node->word = sentence.substr(node->start, node->end - node->start);
   return node;
}

// Rendering of the tree in JSON for debugging
std::ostream &PatternTreeParser::PatternNode::render(std::ostream &os)
{
   os << "{"
      << "\"term\": \"" << term.to_string() << "\",\n"
      << "\"arguments\": [\n";
   for (auto &node : arguments)
      node->render(os) << "," << std::endl;
   os << "null]\n";
   os << "}\n";
   return os;
}

std::ostream &PatternTreeParser::WordNode::render(std::ostream &os)
{
   os << '"' << word.to_string() << '"';
   return os;
}

std::nullptr_t PatternTreeParser::Node::error(std::string message)
{
   throw Error{*this, message};
}

bool PatternTreeParser::PatternNode::matches(APT::StringView name, int min, int max)
{
   if (name != term)
      return false;
   if (max != 0 && !haveArgumentList)
      error(rstrprintf("%s expects an argument list", term.to_string().c_str()));
   if (max == 0 && haveArgumentList)
      error(rstrprintf("%s does not expect an argument list", term.to_string().c_str()));
   if (min >= 0 && min == max && (arguments.size() != size_t(min)))
      error(rstrprintf("%s expects %d arguments, but received %d arguments", term.to_string().c_str(), min, arguments.size()));
   if (min >= 0 && arguments.size() < size_t(min))
      error(rstrprintf("%s expects at least %d arguments, but received %d arguments", term.to_string().c_str(), min, arguments.size()));
   if (max >= 0 && arguments.size() > size_t(max))
      error(rstrprintf("%s expects at most %d arguments, but received %d arguments", term.to_string().c_str(), max, arguments.size()));
   return true;
}

} // namespace Internal
} // namespace APT
