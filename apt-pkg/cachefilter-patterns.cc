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

   node->end = state.offset;
   if (sentence[state.offset] != ')')
      throw Error{node->arguments.empty() ? *node : *node->arguments[node->arguments.size() - 1],
		  rstrprintf("Expected closing parenthesis or comma after last argument, received %c", sentence[state.offset])};

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
   static const APT::StringView DISALLOWED_START("?~,()\0", 6);
   static const APT::StringView DISALLOWED(",()\0", 4);
   if (DISALLOWED_START.find(sentence[state.offset]) != APT::StringView::npos)
      return nullptr;

   auto node = std::make_unique<WordNode>();
   node->start = state.offset;

   while (DISALLOWED.find(sentence[state.offset]) == APT::StringView::npos)
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

std::unique_ptr<APT::CacheFilter::Matcher> PatternParser::aPattern(std::unique_ptr<PatternTreeParser::Node> &nodeP)
{
   assert(nodeP != nullptr);
   auto node = dynamic_cast<PatternTreeParser::PatternNode *>(nodeP.get());
   if (node == nullptr)
      nodeP->error("Expected a pattern");

   if (node->matches("?architecture", 1, 1))
      return std::make_unique<APT::CacheFilter::PackageArchitectureMatchesSpecification>(aWord(node->arguments[0]));
   if (node->matches("?automatic", 0, 0))
      return std::make_unique<Patterns::PackageIsAutomatic>(file);
   if (node->matches("?broken", 0, 0))
      return std::make_unique<Patterns::PackageIsBroken>(file);
   if (node->matches("?config-files", 0, 0))
      return std::make_unique<Patterns::PackageIsConfigFiles>();
   if (node->matches("?essential", 0, 0))
      return std::make_unique<Patterns::PackageIsEssential>();
   if (node->matches("?exact-name", 1, 1))
      return std::make_unique<Patterns::PackageHasExactName>(aWord(node->arguments[0]));
   if (node->matches("?false", 0, 0))
      return std::make_unique<APT::CacheFilter::FalseMatcher>();
   if (node->matches("?garbage", 0, 0))
      return std::make_unique<Patterns::PackageIsGarbage>(file);
   if (node->matches("?installed", 0, 0))
      return std::make_unique<Patterns::PackageIsInstalled>(file);
   if (node->matches("?name", 1, 1))
      return std::make_unique<APT::CacheFilter::PackageNameMatchesRegEx>(aWord(node->arguments[0]));
   if (node->matches("?not", 1, 1))
      return std::make_unique<APT::CacheFilter::NOTMatcher>(aPattern(node->arguments[0]).release());
   if (node->matches("?obsolete", 0, 0))
      return std::make_unique<Patterns::PackageIsObsolete>();
   if (node->matches("?true", 0, 0))
      return std::make_unique<APT::CacheFilter::TrueMatcher>();
   if (node->matches("?upgradable", 0, 0))
      return std::make_unique<Patterns::PackageIsUpgradable>(file);
   if (node->matches("?virtual", 0, 0))
      return std::make_unique<Patterns::PackageIsVirtual>();
   if (node->matches("?x-name-fnmatch", 1, 1))
      return std::make_unique<APT::CacheFilter::PackageNameMatchesFnmatch>(aWord(node->arguments[0]));

   // Variable argument patterns
   if (node->matches("?and", 0, -1))
   {
      auto pattern = std::make_unique<APT::CacheFilter::ANDMatcher>();
      for (auto &arg : node->arguments)
	 pattern->AND(aPattern(arg).release());
      return pattern;
   }
   if (node->matches("?or", 0, -1))
   {
      auto pattern = std::make_unique<APT::CacheFilter::ORMatcher>();

      for (auto &arg : node->arguments)
	 pattern->OR(aPattern(arg).release());
      return pattern;
   }

   node->error(rstrprintf("Unrecognized pattern '%s'", node->term.to_string().c_str()));

   return nullptr;
}

std::string PatternParser::aWord(std::unique_ptr<PatternTreeParser::Node> &nodeP)
{
   assert(nodeP != nullptr);
   auto node = dynamic_cast<PatternTreeParser::WordNode *>(nodeP.get());
   if (node == nullptr)
      nodeP->error("Expected a word");
   return node->word.to_string();
}

} // namespace Internal

// The bridge into the public world
std::unique_ptr<APT::CacheFilter::Matcher> APT::CacheFilter::ParsePattern(APT::StringView pattern, pkgCacheFile *file)
{
   if (file != nullptr && !file->BuildDepCache())
      return nullptr;

   try
   {
      auto top = APT::Internal::PatternTreeParser(pattern).parseTop();
      APT::Internal::PatternParser parser{file};
      return parser.aPattern(top);
   }
   catch (APT::Internal::PatternTreeParser::Error &e)
   {
      std::stringstream ss;
      ss << "input:" << e.location.start << "-" << e.location.end << ": error: " << e.message << "\n";
      ss << pattern.to_string() << "\n";
      for (size_t i = 0; i < e.location.start; i++)
	 ss << " ";
      for (size_t i = e.location.start; i < e.location.end; i++)
	 ss << "^";

      ss << "\n";

      _error->Error("%s", ss.str().c_str());
      return nullptr;
   }
}

} // namespace APT
