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
#include <apt-pkg/header-is-private.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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
struct APT_PUBLIC PatternTreeParser
{

   struct Node
   {
      size_t start = 0;
      size_t end = 0;

      explicit Node(size_t start = 0, size_t end = 0) : start(start), end(end) {}

      virtual std::ostream &render(std::ostream &os) { return os; };
      std::nullptr_t error(std::string message);
      virtual ~Node() = default;
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

      APT_HIDDEN std::ostream &render(std::ostream &stream) override;
      APT_HIDDEN bool matches(APT::StringView name, int min, int max);
   };

   struct WordNode : public Node
   {
      APT::StringView word;
      bool quoted = false;
      APT_HIDDEN std::ostream &render(std::ostream &stream) override;
   };

   struct State
   {
      size_t offset = 0;
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
   std::unique_ptr<Node> parse(); // public for test cases only

   private:
   APT_HIDDEN std::unique_ptr<Node> parseOr();
   APT_HIDDEN std::unique_ptr<Node> parseAnd();
   APT_HIDDEN std::unique_ptr<Node> parseUnary();
   APT_HIDDEN std::unique_ptr<Node> parsePrimary();
   APT_HIDDEN std::unique_ptr<Node> parseGroup();
   APT_HIDDEN std::unique_ptr<Node> parsePattern();
   APT_HIDDEN std::unique_ptr<Node> parseShortPattern();
   APT_HIDDEN std::unique_ptr<Node> parseArgument(bool shrt);
   APT_HIDDEN std::unique_ptr<Node> parseWord(bool shrt);
   APT_HIDDEN std::unique_ptr<Node> parseQuotedWord();
};

/**
 * \brief PatternParser parses the given sentence into a parse tree.
 *
 * The parse tree consists of nodes:
 *  - Word nodes which contains words or quoted words
 *  - Patterns, which represent ?foo and ?foo(...) patterns
 */
struct APT_HIDDEN PatternParser
{
   pkgCacheFile *file;

   std::unique_ptr<APT::CacheFilter::Matcher> aPattern(std::unique_ptr<PatternTreeParser::Node> &nodeP);
   std::string aWord(std::unique_ptr<PatternTreeParser::Node> &nodeP);
};

namespace Patterns
{
using namespace APT::CacheFilter;

/** \brief Basic helper class for matching regex */
class BaseRegexMatcher
{
   regex_t *pattern;

   public:
   BaseRegexMatcher(std::string const &string);
   ~BaseRegexMatcher();
   bool operator()(const char *cstring);
   bool operator()(std::string const &string)
   {
      return (*this)(string.c_str());
   }
};

struct APT_HIDDEN PackageIsAutomatic : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsAutomatic(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      assert(Cache != nullptr);
      return ((*Cache)[Pkg].Flags & pkgCache::Flag::Auto) != 0;
   }
};

struct APT_HIDDEN PackageIsBroken : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsBroken(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      assert(Cache != nullptr);
      auto state = (*Cache)[Pkg];
      return state.InstBroken() || state.NowBroken();
   }
};

struct APT_HIDDEN PackageIsConfigFiles : public PackageMatcher
{
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      return Pkg->CurrentState == pkgCache::State::ConfigFiles;
   }
};

struct APT_HIDDEN PackageIsGarbage : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsGarbage(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      assert(Cache != nullptr);
      return (*Cache)[Pkg].Garbage;
   }
};
struct APT_HIDDEN PackageIsEssential : public PackageMatcher
{
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      return (Pkg->Flags & pkgCache::Flag::Essential) != 0;
   }
};

struct APT_HIDDEN PackageHasExactName : public PackageMatcher
{
   std::string name;
   explicit PackageHasExactName(std::string name) : name(name) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      return Pkg.Name() == name;
   }
};

struct APT_HIDDEN PackageIsInstalled : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsInstalled(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      assert(Cache != nullptr);
      return Pkg->CurrentVer != 0;
   }
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      assert(Cache != nullptr);
      return Ver == Ver.ParentPkg().CurrentVer();
   }
};

struct APT_HIDDEN PackageIsObsolete : public PackageMatcher
{
   bool operator()(pkgCache::PkgIterator const &pkg) override
   {
      // This code can be written without loops, as aptitude does, but it
      // is far less readable.
      if (pkg.CurrentVer().end())
	 return false;

      // See if there is any version that exists in a repository,
      // if so return false
      for (auto ver = pkg.VersionList(); !ver.end(); ver++)
      {
	 for (auto file = ver.FileList(); !file.end(); file++)
	 {
	    if ((file.File()->Flags & pkgCache::Flag::NotSource) == 0)
	       return false;
	 }
      }

      return true;
   }
};

struct APT_HIDDEN PackageIsPhasing : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsPhasing(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &pkg) override
   {
      return (*Cache)->PhasingApplied(pkg);
   }
};

struct APT_HIDDEN PackageIsUpgradable : public PackageMatcher
{
   pkgCacheFile *Cache;
   explicit PackageIsUpgradable(pkgCacheFile *Cache) : Cache(Cache) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      assert(Cache != nullptr);
      return Pkg->CurrentVer != 0 && (*Cache)[Pkg].Upgradable();
   }
};

struct APT_HIDDEN PackageIsVirtual : public PackageMatcher
{
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      return Pkg->VersionList == 0;
   }
};

struct APT_HIDDEN VersionAnyMatcher : public Matcher
{
   bool operator()(pkgCache::GrpIterator const &) override { return false; }
   bool operator()(pkgCache::VerIterator const &Ver) override = 0;
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      for (auto Ver = Pkg.VersionList(); not Ver.end(); Ver++)
      {
         if ((*this)(Ver))
            return true;
      }
      return false;
   }
};

struct APT_HIDDEN VersionIsAllVersions : public Matcher
{
   std::unique_ptr<APT::CacheFilter::Matcher> base;
   VersionIsAllVersions(std::unique_ptr<APT::CacheFilter::Matcher> base) : base(std::move(base)) {}
   bool operator()(pkgCache::GrpIterator const &) override { return false; }
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return (*base)(Ver);
   }
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      for (auto Ver = Pkg.VersionList(); not Ver.end(); Ver++)
      {
	 if (not(*this)(Ver))
	    return false;
      }
      return true;
   }
};

struct APT_HIDDEN VersionDepends : public VersionAnyMatcher
{
   std::unique_ptr<APT::CacheFilter::Matcher> base;
   pkgCache::Dep::DepType type;
   VersionDepends(std::unique_ptr<APT::CacheFilter::Matcher> base, pkgCache::Dep::DepType type = pkgCache::Dep::Depends) : base(std::move(base)), type(type) {}
   bool operator()(pkgCache::GrpIterator const &) override { return false; }
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      for (auto D = Ver.DependsList(); not D.end(); D++)
      {
	 if (D.IsImplicit())
	    continue;
	 if (D->Type != type)
	    continue;
	 if ((*base)(D.TargetPkg()))
	    return true;
      }

      return false;
   }
};

struct APT_HIDDEN PackageReverseDepends : public PackageMatcher
{
   std::unique_ptr<APT::CacheFilter::Matcher> base;
   pkgCache::Dep::DepType type;
   PackageReverseDepends(std::unique_ptr<APT::CacheFilter::Matcher> base, pkgCache::Dep::DepType type = pkgCache::Dep::Depends) : base(std::move(base)), type(type) {}
   bool operator()(pkgCache::PkgIterator const &Pkg) override
   {
      for (auto D = Pkg.RevDependsList(); not D.end(); D++)
      {
	 if (D.IsImplicit())
	    continue;
	 if (D->Type != type)
	    continue;
	 if ((*base)(D.ParentVer()))
	    return true;
      }

      return false;
   }
};

struct APT_HIDDEN VersionIsAnyVersion : public VersionAnyMatcher
{
   std::unique_ptr<APT::CacheFilter::Matcher> base;
   VersionIsAnyVersion(std::unique_ptr<APT::CacheFilter::Matcher> base) : base(std::move(base)) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return (*base)(Ver);
   }
};

struct APT_HIDDEN VersionIsArchive : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsArchive(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      for (auto VF = Ver.FileList(); not VF.end(); VF++)
      {
	 if (VF.File().Archive() && matcher(VF.File().Archive()))
	    return true;
      }
      return false;
   }
};

struct APT_HIDDEN VersionIsCodename : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsCodename(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      for (auto VF = Ver.FileList(); not VF.end(); VF++)
      {
	 if (VF.File().Codename() && matcher(VF.File().Codename()))
	    return true;
      }
      return false;
   }
};

struct APT_HIDDEN VersionIsOrigin : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsOrigin(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      for (auto VF = Ver.FileList(); not VF.end(); VF++)
      {
	 if (VF.File().Origin() && matcher(VF.File().Origin()))
	    return true;
      }
      return false;
   }
};

struct APT_HIDDEN VersionIsSection : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsSection(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return matcher(Ver.Section());
   }
};

struct APT_HIDDEN VersionIsSecurity : public VersionAnyMatcher
{
   VersionIsSecurity() {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return Ver.IsSecurityUpdate();
   }
};

struct APT_HIDDEN VersionIsSourcePackage : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsSourcePackage(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return matcher(Ver.SourcePkgName());
   }
};

struct APT_HIDDEN VersionIsSourceVersion : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsSourceVersion(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return matcher(Ver.SourceVerStr());
   }
};

struct APT_HIDDEN VersionIsVersion : public VersionAnyMatcher
{
   BaseRegexMatcher matcher;
   VersionIsVersion(std::string const &pattern) : matcher(pattern) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      return matcher(Ver.VerStr());
   }
};

struct APT_HIDDEN VersionIsPriority : public VersionAnyMatcher
{
   std::string name;
   explicit VersionIsPriority(std::string name) : name(name) {}
   bool operator()(pkgCache::VerIterator const &Ver) override
   {
      std::string Mapping[] = {"", "required","important","standard",
                            "optional","extra"};
      if (Ver->Priority > 0 && Ver->Priority < APT_ARRAY_SIZE(Mapping)) {
         return name == Mapping[Ver->Priority];
      }
      return false;
   }
};

} // namespace Patterns
} // namespace Internal
} // namespace APT
#endif
