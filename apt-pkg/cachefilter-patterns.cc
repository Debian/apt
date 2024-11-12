/*
 * cachefilter-patterns.cc - Parser for aptitude-style patterns
 *
 * Copyright (c) 2019 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>

#include <apt-pkg/cachefilter-patterns.h>

#include <apti18n.h>

using namespace std::literals;

namespace APT
{
namespace Internal
{

static const constexpr struct
{
   std::string_view shortName;
   std::string_view longName;
   bool takesArgument;
} shortPatterns[] = {
   {"r"sv, "?architecture"sv, true},
   {"A"sv, "?archive"sv, true},
   {"M"sv, "?automatic"sv, false},
   {"b"sv, "?broken"sv, false},
   {"c"sv, "?config-files"sv, false},
   // FIXME: The words after ~D should be case-insensitive
   {"DDepends:"sv, "?depends"sv, true},
   {"DPre-Depends:"sv, "?pre-depends"sv, true},
   {"DSuggests:"sv, "?suggests"sv, true},
   {"DRecommends:"sv, "?recommends"sv, true},
   {"DConflicts:"sv, "?conflicts"sv, true},
   {"DReplaces:"sv, "?replaces"sv, true},
   {"DObsoletes:"sv, "?obsoletes"sv, true},
   {"DBreaks:"sv, "?breaks"sv, true},
   {"DEnhances:"sv, "?enhances"sv, true},
   {"D"sv, "?depends"sv, true},
   {"RDepends:"sv, "?reverse-depends"sv, true},
   {"RPre-Depends:"sv, "?reverse-pre-depends"sv, true},
   {"RSuggests:"sv, "?reverse-suggests"sv, true},
   {"RRecommends:"sv, "?reverse-recommends"sv, true},
   {"RConflicts:"sv, "?reverse-conflicts"sv, true},
   {"RReplaces:"sv, "?reverse-replaces"sv, true},
   {"RObsoletes:"sv, "?reverse-obsoletes"sv, true},
   {"RBreaks:"sv, "?reverse-breaks"sv, true},
   {"REnhances:"sv, "?reverse-enhances"sv, true},
   {"R"sv, "?reverse-depends"sv, true},
   {"E"sv, "?essential"sv, false},
   {"F"sv, "?false"sv, false},
   {"g"sv, "?garbage"sv, false},
   {"i"sv, "?installed"sv, false},
   {"n"sv, "?name"sv, true},
   {"o"sv, "?obsolete"sv, false},
   {"O"sv, "?origin"sv, true},
   {"p"sv, "?priority"sv, true},
   {"s"sv, "?section"sv, true},
   {"e"sv, "?source-package"sv, true},
   {"T"sv, "?true"sv, false},
   {"U"sv, "?upgradable"sv, false},
   {"V"sv, "?version"sv, true},
   {"v"sv, "?virtual"sv, false},
};

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

   if (node == nullptr)
      throw Error{Node{0, sentence.size()}, "Expected pattern"};

   if (node->end != sentence.size())
      throw Error{Node{node->end, sentence.size()}, "Expected end of file"};

   return node;
}

// Parse any pattern
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parse()
{
   return parseOr();
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseOr()
{
   auto start = state.offset;
   std::vector<std::unique_ptr<PatternTreeParser::Node>> nodes;

   auto firstNode = parseAnd();

   if (firstNode == nullptr)
      return nullptr;

   nodes.push_back(std::move(firstNode));
   for (skipSpace(); sentence[state.offset] == '|'; skipSpace())
   {
      state.offset++;
      skipSpace();
      auto node = parseAnd();

      if (node == nullptr)
	 throw Error{Node{state.offset, sentence.size()}, "Expected pattern after |"};

      nodes.push_back(std::move(node));
   }

   if (nodes.size() == 0)
      return nullptr;
   if (nodes.size() == 1)
      return std::move(nodes[0]);

   auto node = std::make_unique<PatternNode>();
   node->start = start;
   node->end = nodes[nodes.size() - 1]->end;
   node->term = "?or";
   node->arguments = std::move(nodes);
   node->haveArgumentList = true;

   return node;
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseAnd()
{
   auto start = state.offset;
   std::vector<std::unique_ptr<PatternTreeParser::Node>> nodes;

   for (skipSpace(); state.offset < sentence.size(); skipSpace())
   {
      auto node = parseUnary();

      if (node == nullptr)
	 break;

      nodes.push_back(std::move(node));
   }

   if (nodes.size() == 0)
      return nullptr;
   if (nodes.size() == 1)
      return std::move(nodes[0]);

   auto node = std::make_unique<PatternNode>();
   node->start = start;
   node->end = nodes[nodes.size() - 1]->end;
   node->term = "?and";
   node->arguments = std::move(nodes);
   node->haveArgumentList = true;

   return node;
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseUnary()
{

   if (sentence[state.offset] != '!')
      return parsePrimary();

   auto start = ++state.offset;
   auto primary = parsePrimary();

   if (primary == nullptr)
      throw Error{Node{start, sentence.size()}, "Expected pattern"};

   auto node = std::make_unique<PatternNode>();
   node->start = start;
   node->end = primary->end;
   node->term = "?not";
   node->arguments.push_back(std::move(primary));
   node->haveArgumentList = true;
   return node;
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parsePrimary()
{
   std::unique_ptr<Node> node;
   if ((node = parseShortPattern()) != nullptr)
      return node;
   if ((node = parsePattern()) != nullptr)
      return node;
   if ((node = parseGroup()) != nullptr)
      return node;

   return nullptr;
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseGroup()
{
   if (sentence[state.offset] != '(')
      return nullptr;

   auto start = state.offset++;

   skipSpace();
   auto node = parse();
   if (node == nullptr)
      throw Error{Node{state.offset, sentence.size()},
		  "Expected pattern after '('"};
   skipSpace();

   if (sentence[state.offset] != ')')
      throw Error{Node{state.offset, sentence.size()},
		  "Expected closing parenthesis"};

   auto end = ++state.offset;
   node->start = start;
   node->end = end;
   return node;
}

std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseArgument(bool shrt)
{
   std::unique_ptr<Node> node;
   if ((node = parseQuotedWord()) != nullptr)
      return node;
   if ((node = parseWord(shrt)) != nullptr)
      return node;
   if ((node = parse()) != nullptr)
      return node;

   throw Error{Node{state.offset, sentence.size()},
	       "Expected pattern, quoted word, or word"};
}

// Parse a short pattern
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseShortPattern()
{
   if (sentence[state.offset] != '~')
      return nullptr;

   for (auto &sp : shortPatterns)
   {
      if (sentence.substr(state.offset + 1, sp.shortName.size()) != sp.shortName)
	 continue;

      auto node = std::make_unique<PatternNode>();
      node->end = node->start = state.offset;
      node->term = sp.longName;

      state.offset += sp.shortName.size() + 1;
      if (sp.takesArgument)
      {
	 node->arguments.push_back(parseArgument(true));
	 node->haveArgumentList = true;
      }
      node->end = state.offset;

      return node;
   }

   throw Error{Node{state.offset, sentence.size()}, "Unknown short pattern"};
}

// Parse a list pattern (or function call pattern)
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parsePattern()
{
   static constexpr auto CHARS = ("0123456789"
				  "abcdefghijklmnopqrstuvwxyz"
				  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				  "-"sv);
   if (sentence[state.offset] != '?')
      return nullptr;

   auto node = std::make_unique<PatternNode>();
   node->end = node->start = state.offset;
   state.offset++;

   while (CHARS.find(sentence[state.offset]) != std::string_view::npos)
   {
      ++state.offset;
   }

   node->term = sentence.substr(node->start, state.offset - node->start);

   if (node->term.size() <= 1)
      throw Error{*node, "Pattern must have a term/name"};

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

   node->arguments.push_back(parseArgument(false));
   skipSpace();
   while (sentence[state.offset] == ',')
   {
      ++state.offset;
      skipSpace();
      // This was a trailing comma - allow it and break the loop
      if (sentence[state.offset] == ')')
	 break;
      node->arguments.push_back(parseArgument(false));
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
std::unique_ptr<PatternTreeParser::Node> PatternTreeParser::parseWord(bool shrt)
{
   // Characters not allowed at the start of a word (also see ..._SHRT)
   static const constexpr auto DISALLOWED_START = "!?~|,() \0"sv;
   // Characters terminating a word inside a long pattern
   static const constexpr auto DISALLOWED_LONG = "|,()\0"sv;
   // Characters terminating a word as a short form argument, should contain all of START.
   static const constexpr auto DISALLOWED_SHRT = "!?~|,() \0"sv;
   const auto DISALLOWED = shrt ? DISALLOWED_SHRT : DISALLOWED_LONG;

   if (DISALLOWED_START.find(sentence[state.offset]) != std::string_view::npos)
      return nullptr;

   auto node = std::make_unique<WordNode>();
   node->start = state.offset;

   while (DISALLOWED.find(sentence[state.offset]) == std::string_view::npos)
      state.offset++;

   node->end = state.offset;
   node->word = sentence.substr(node->start, node->end - node->start);
   return node;
}

// Rendering of the tree in JSON for debugging
std::ostream &PatternTreeParser::PatternNode::render(std::ostream &os)
{

   os << term;
   if (haveArgumentList)
   {
      os << "(";
      for (auto &node : arguments)
	 node->render(os) << ",";
      os << ")";
   }
   return os;
}

std::ostream &PatternTreeParser::WordNode::render(std::ostream &os)
{
   return quoted ? os << '"' << word << '"' : os << word;
}

std::nullptr_t PatternTreeParser::Node::error(std::string message)
{
   throw Error{*this, message};
}

bool PatternTreeParser::PatternNode::matches(std::string_view name, int min, int max)
{
   if (name != term)
      return false;
   if (max != 0 && !haveArgumentList)
      error(rstrprintf("%.*s expects an argument list", (int)term.size(), term.data()));
   if (max == 0 && haveArgumentList)
      error(rstrprintf("%.*s does not expect an argument list", (int)term.size(), term.data()));
   if (min >= 0 && min == max && (arguments.size() != size_t(min)))
      error(rstrprintf("%.*s expects %d arguments, but received %d arguments", (int)term.size(), term.data(), min, arguments.size()));
   if (min >= 0 && arguments.size() < size_t(min))
      error(rstrprintf("%.*s expects at least %d arguments, but received %d arguments", (int)term.size(), term.data(), min, arguments.size()));
   if (max >= 0 && arguments.size() > size_t(max))
      error(rstrprintf("%.*s expects at most %d arguments, but received %d arguments", (int)term.size(), term.data(), max, arguments.size()));
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
   if (node->matches("?archive", 1, 1))
      return std::make_unique<Patterns::VersionIsArchive>(aWord(node->arguments[0]));
   if (node->matches("?codename", 1, 1))
      return std::make_unique<Patterns::VersionIsCodename>(aWord(node->arguments[0]));
   if (node->matches("?all-versions", 1, 1))
      return std::make_unique<Patterns::VersionIsAllVersions>(aPattern(node->arguments[0]));
   if (node->matches("?any-version", 1, 1))
      return std::make_unique<Patterns::VersionIsAnyVersion>(aPattern(node->arguments[0]));
   if (node->matches("?automatic", 0, 0))
      return std::make_unique<Patterns::PackageIsAutomatic>(file);
   if (node->matches("?broken", 0, 0))
      return std::make_unique<Patterns::PackageIsBroken>(file);
   if (node->matches("?config-files", 0, 0))
      return std::make_unique<Patterns::PackageIsConfigFiles>();
   if (node->matches("?depends", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]));
   if (node->matches("?predepends", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::PreDepends);
   if (node->matches("?suggests", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Suggests);
   if (node->matches("?recommends", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Recommends);
   if (node->matches("?conflicts", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Conflicts);
   if (node->matches("?replaces", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Replaces);
   if (node->matches("?obsoletes", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Obsoletes);
   if (node->matches("?breaks", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::DpkgBreaks);
   if (node->matches("?enhances", 1, 1))
      return std::make_unique<Patterns::VersionDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Enhances);
   if (node->matches("?reverse-depends", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]));
   if (node->matches("?reverse-predepends", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::PreDepends);
   if (node->matches("?reverse-suggests", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Suggests);
   if (node->matches("?reverse-recommends", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Recommends);
   if (node->matches("?reverse-conflicts", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Conflicts);
   if (node->matches("?reverse-replaces", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Replaces);
   if (node->matches("?reverse-obsoletes", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Obsoletes);
   if (node->matches("?reverse-breaks", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::DpkgBreaks);
   if (node->matches("?reverse-enhances", 1, 1))
      return std::make_unique<Patterns::PackageReverseDepends>(aPattern(node->arguments[0]), pkgCache::Dep::Enhances);
   if (node->matches("?essential", 0, 0))
      return std::make_unique<Patterns::PackageIsEssential>();
   if (node->matches("?priority", 1, 1))
      return std::make_unique<Patterns::VersionIsPriority>(aWord(node->arguments[0]));
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
   if (node->matches("?origin", 1, 1))
      return std::make_unique<Patterns::VersionIsOrigin>(aWord(node->arguments[0]));
   if (node->matches("?phasing", 0, 0))
      return std::make_unique<Patterns::PackageIsPhasing>(file);
   if (node->matches("?section", 1, 1))
      return std::make_unique<Patterns::VersionIsSection>(aWord(node->arguments[0]));
   if (node->matches("?security", 0, 0))
      return std::make_unique<Patterns::VersionIsSecurity>();
   if (node->matches("?source-package", 1, 1))
      return std::make_unique<Patterns::VersionIsSourcePackage>(aWord(node->arguments[0]));
   if (node->matches("?source-version", 1, 1))
      return std::make_unique<Patterns::VersionIsSourceVersion>(aWord(node->arguments[0]));
   if (node->matches("?true", 0, 0))
      return std::make_unique<APT::CacheFilter::TrueMatcher>();
   if (node->matches("?upgradable", 0, 0))
      return std::make_unique<Patterns::PackageIsUpgradable>(file);
   if (node->matches("?version", 1, 1))
      return std::make_unique<Patterns::VersionIsVersion>(aWord(node->arguments[0]));
   if (node->matches("?virtual", 0, 0))
      return std::make_unique<Patterns::PackageIsVirtual>();
   if (node->matches("?x-name-fnmatch", 1, 1))
      return std::make_unique<APT::CacheFilter::PackageNameMatchesFnmatch>(aWord(node->arguments[0]));

   // Variable argument patterns
   if (node->matches("?and", 0, -1) || node->matches("?narrow", 0, -1))
   {
      auto pattern = std::make_unique<APT::CacheFilter::ANDMatcher>();
      for (auto &arg : node->arguments)
	 pattern->AND(aPattern(arg).release());
      if (node->term == "?narrow")
	 return std::make_unique<Patterns::VersionIsAnyVersion>(std::move(pattern));
      return pattern;
   }
   if (node->matches("?or", 0, -1))
   {
      auto pattern = std::make_unique<APT::CacheFilter::ORMatcher>();

      for (auto &arg : node->arguments)
	 pattern->OR(aPattern(arg).release());
      return pattern;
   }

   node->error(rstrprintf("Unrecognized pattern '%.*s'", (int)node->term.size(), node->term.data()));

   return nullptr;
}

std::string PatternParser::aWord(std::unique_ptr<PatternTreeParser::Node> &nodeP)
{
   assert(nodeP != nullptr);
   auto node = dynamic_cast<PatternTreeParser::WordNode *>(nodeP.get());
   if (node == nullptr)
      nodeP->error("Expected a word");
   return std::string{node->word};
}

namespace Patterns
{

BaseRegexMatcher::BaseRegexMatcher(std::string const &Pattern)
{
   pattern = new regex_t;
   int const Res = regcomp(pattern, Pattern.c_str(), REG_EXTENDED | REG_ICASE | REG_NOSUB);
   if (Res == 0)
      return;

   delete pattern;
   pattern = NULL;
   char Error[300];
   regerror(Res, pattern, Error, sizeof(Error));
   _error->Error(_("Regex compilation error - %s"), Error);
}
bool BaseRegexMatcher::operator()(const char *string)
{
   if (unlikely(pattern == nullptr) || string == nullptr)
      return false;
   else
      return regexec(pattern, string, 0, 0, 0) == 0;
}
BaseRegexMatcher::~BaseRegexMatcher()
{
   if (pattern == NULL)
      return;
   regfree(pattern);
   delete pattern;
}
} // namespace Patterns

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
      ss << pattern << "\n";
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
