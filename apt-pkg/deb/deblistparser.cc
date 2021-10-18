// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile-keys.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <string>
#include <vector>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
									/*}}}*/

using std::string;
using APT::StringView;

static const debListParser::WordList PrioList[] = {
   {"required",pkgCache::State::Required},
   {"important",pkgCache::State::Important},
   {"standard",pkgCache::State::Standard},
   {"optional",pkgCache::State::Optional},
   {"extra",pkgCache::State::Extra},
   {"", 0}};

// ListParser::debListParser - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Provide an architecture and only this one and "all" will be accepted
   in Step(), if no Architecture is given we will accept every arch
   we would accept in general with checkArchitecture() */
debListParser::debListParser(FileFd *File) :
   pkgCacheListParser(), Tags(File)
{
   // this dance allows an empty value to override the default
   if (_config->Exists("pkgCacheGen::ForceEssential"))
   {
      forceEssential = _config->FindVector("pkgCacheGen::ForceEssential");
      if (forceEssential.empty() == false && _config->Find("pkgCacheGen::ForceEssential").empty())
	 forceEssential.emplace_back("apt");
   }
   else
      forceEssential.emplace_back("apt");
   forceImportant = _config->FindVector("pkgCacheGen::ForceImportant");
   myArch = _config->Find("APT::Architecture");
}
									/*}}}*/
// ListParser::Package - Return the package name			/*{{{*/
// ---------------------------------------------------------------------
/* This is to return the name of the package this section describes */
string debListParser::Package() {
   string Result = Section.Find(pkgTagSection::Key::Package).to_string();

   // Normalize mixed case package names to lower case, like dpkg does
   // See Bug#807012 for details.
   // Only do this when the package name does not contain a / - as that
   // indicates that the package name was derived from a filename given
   // to install or build-dep or similar (Bug#854794)
   if (likely(Result.find('/') == string::npos))
   {
      for (char &c: Result)
      {
	 char l = tolower_ascii_inline(c);
	 if (unlikely(l != c))
	    c = l;
      }
   }

   if(unlikely(Result.empty() == true))
      _error->Error("Encountered a section with no Package: header");
   return Result;
}
									/*}}}*/
// ListParser::Architecture - Return the package arch			/*{{{*/
// ---------------------------------------------------------------------
/* This will return the Architecture of the package this section describes */
APT::StringView debListParser::Architecture() {
   auto const Arch = Section.Find(pkgTagSection::Key::Architecture);
   return Arch.empty() ? "none" : Arch;
}
									/*}}}*/
// ListParser::ArchitectureAll						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::ArchitectureAll() {
   return Section.Find(pkgTagSection::Key::Architecture) == "all";
}
									/*}}}*/
// ListParser::Version - Return the version string			/*{{{*/
// ---------------------------------------------------------------------
/* This is to return the string describing the version in debian form,
   epoch:upstream-release. If this returns the blank string then the 
   entry is assumed to only describe package properties */
APT::StringView debListParser::Version()
{
   return Section.Find(pkgTagSection::Key::Version);
}
									/*}}}*/
unsigned char debListParser::ParseMultiArch(bool const showErrors)	/*{{{*/
{
   unsigned char MA;
   auto const MultiArch = Section.Find(pkgTagSection::Key::Multi_Arch);
   if (MultiArch.empty() == true || MultiArch == "no")
      MA = pkgCache::Version::No;
   else if (MultiArch == "same") {
      if (ArchitectureAll() == true)
      {
	 if (showErrors == true)
	    _error->Warning("Architecture: all package '%s' can't be Multi-Arch: same",
		  Section.FindS("Package").c_str());
	 MA = pkgCache::Version::No;
      }
      else
	 MA = pkgCache::Version::Same;
   }
   else if (MultiArch == "foreign")
      MA = pkgCache::Version::Foreign;
   else if (MultiArch == "allowed")
      MA = pkgCache::Version::Allowed;
   else
   {
      if (showErrors == true)
	 _error->Warning("Unknown Multi-Arch type '%s' for package '%s'",
	       MultiArch.to_string().c_str(), Section.FindS("Package").c_str());
      MA = pkgCache::Version::No;
   }

   if (ArchitectureAll() == true)
      MA |= pkgCache::Version::All;

   return MA;
}
									/*}}}*/
// ListParser::NewVersion - Fill in the version structure		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::NewVersion(pkgCache::VerIterator &Ver)
{
   const char *Start;
   const char *Stop;

   // Parse the section
   if (Section.Find(pkgTagSection::Key::Section,Start,Stop) == true)
   {
      map_stringitem_t const idx = StoreString(pkgCacheGenerator::SECTION, Start, Stop - Start);
      Ver->Section = idx;
   }
   // Parse the source package name
   pkgCache::GrpIterator G = Ver.ParentPkg().Group();

   // Setup the defaults
   Ver->SourcePkgName = G->Name;
   Ver->SourceVerStr = Ver->VerStr;

   // Parse the name and version str
   if (Section.Find(pkgTagSection::Key::Source,Start,Stop) == true)
   {
      const char * const Space = static_cast<const char *>(memchr(Start, ' ', Stop - Start));
      pkgCache::VerIterator V;

      if (Space != NULL)
      {
	 const char * const Open = static_cast<const char *>(memchr(Space, '(', Stop - Space));
	 if (likely(Open != NULL))
	 {
	    const char * const Close = static_cast<const char *>(memchr(Open, ')', Stop - Open));
	    if (likely(Close != NULL))
	    {
	       APT::StringView const version(Open + 1, (Close - Open) - 1);
	       if (version != Ver.VerStr())
	       {
		  map_stringitem_t const idx = StoreString(pkgCacheGenerator::VERSIONNUMBER, version);
		  G = Ver.ParentPkg().Group();
		  Ver->SourceVerStr = idx;
	       }
	    }
	 }
	 Stop = Space;
      }

      APT::StringView const pkgname(Start, Stop - Start);
      // Oh, our group is the wrong one for the source package. Make a new one.
      if (pkgname != G.Name())
      {
	 if (not NewGroup(G, pkgname))
	    return false;
      }
   }

   // Link into by source package group.
   Ver->SourcePkgName = G->Name;
   Ver->NextInSource = G->VersionsInSource;
   G->VersionsInSource = Ver.MapPointer();

   Ver->MultiArch = ParseMultiArch(true);
   // Archive Size
   Ver->Size = Section.FindULL(pkgTagSection::Key::Size);
   // Unpacked Size (in K)
   Ver->InstalledSize = Section.FindULL(pkgTagSection::Key::Installed_Size);
   Ver->InstalledSize *= 1024;

   // Priority
   if (Section.Find(pkgTagSection::Key::Priority,Start,Stop) == true)
   {
      if (GrabWord(StringView(Start,Stop-Start),PrioList,Ver->Priority) == false)
	 Ver->Priority = pkgCache::State::Extra;
   }

   if (ParseDepends(Ver,pkgTagSection::Key::Pre_Depends,pkgCache::Dep::PreDepends) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Depends,pkgCache::Dep::Depends) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Conflicts,pkgCache::Dep::Conflicts) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Breaks,pkgCache::Dep::DpkgBreaks) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Recommends,pkgCache::Dep::Recommends) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Suggests,pkgCache::Dep::Suggests) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Replaces,pkgCache::Dep::Replaces) == false)
      return false;
   if (ParseDepends(Ver,pkgTagSection::Key::Enhances,pkgCache::Dep::Enhances) == false)
      return false;
   // Obsolete.
   if (ParseDepends(Ver,pkgTagSection::Key::Optional,pkgCache::Dep::Suggests) == false)
      return false;
   
   if (ParseProvides(Ver) == false)
      return false;
   if (not APT::KernelAutoRemoveHelper::getUname(Ver.ParentPkg().Name()).empty())
   {
      if (not NewProvides(Ver, "$kernel", "any", Ver.VerStr(), pkgCache::Flag::MultiArchImplicit))
	 return false;
   }

   return true;
}
									/*}}}*/
// ListParser::AvailableDescriptionLanguages				/*{{{*/
std::vector<std::string> debListParser::AvailableDescriptionLanguages()
{
   std::vector<std::string> const understood = APT::Configuration::getLanguages(true);
   std::vector<std::string> avail;
   static constexpr int prefixLen = 12;
   char buf[32] = "Description-";
   if (Section.Exists("Description") == true)
      avail.push_back("");
   for (std::vector<std::string>::const_iterator lang = understood.begin(); lang != understood.end(); ++lang)
   {
      if (unlikely(lang->size() > sizeof(buf) - prefixLen)) {
	 _error->Warning("Ignoring translated description %s", lang->c_str());
	 continue;
      }
      memcpy(buf + prefixLen, lang->c_str(), lang->size());
      if (Section.Exists(StringView(buf, prefixLen + lang->size())) == true)
	 avail.push_back(*lang);
   }
   return avail;
}
									/*}}}*/
// ListParser::Description_md5 - Return the description_md5 MD5SumValue	/*{{{*/
// ---------------------------------------------------------------------
/* This is to return the md5 string to allow the check if it is the right
   description. If no Description-md5 is found in the section it will be
   calculated.
 */
APT::StringView debListParser::Description_md5()
{
   StringView const value = Section.Find(pkgTagSection::Key::Description_md5);
   if (unlikely(value.empty() == true))
   {
      StringView const desc = Section.Find(pkgTagSection::Key::Description);
      if (desc == "\n")
	 return StringView();

      Hashes md5(Hashes::MD5SUM);
      md5.Add(desc.data(), desc.size());
      md5.Add("\n");
      MD5Buffer = md5.GetHashString(Hashes::MD5SUM).HashValue();
      return StringView(MD5Buffer);
   }
   else if (likely(value.size() == 32))
   {
      return value;
   }
   _error->Error("Malformed Description-md5 line; doesn't have the required length (32 != %d) '%.*s'", (int)value.size(), (int)value.length(), value.data());
   return StringView();
}
                                                                        /*}}}*/
// ListParser::UsePackage - Update a package structure			/*{{{*/
// ---------------------------------------------------------------------
/* This is called to update the package with any new information 
   that might be found in the section */
bool debListParser::UsePackage(pkgCache::PkgIterator &Pkg,
			       pkgCache::VerIterator &Ver)
{
   string const static myArch = _config->Find("APT::Architecture");
   // Possible values are: "all", "native", "installed" and "none"
   // The "installed" mode is handled by ParseStatus(), See #544481 and friends.
   string const static essential = _config->Find("pkgCacheGen::Essential", "all");
   if (essential == "all" ||
       (essential == "native" && Pkg->Arch != 0 && myArch == Pkg.Arch()))
      if (Section.FindFlag(pkgTagSection::Key::Essential,Pkg->Flags,pkgCache::Flag::Essential) == false)
	 return false;
   if (Section.FindFlag(pkgTagSection::Key::Important,Pkg->Flags,pkgCache::Flag::Important) == false)
      return false;
   if (Section.FindFlag(pkgTagSection::Key::Protected, Pkg->Flags, pkgCache::Flag::Important) == false)
      return false;

   if (std::find(forceEssential.begin(), forceEssential.end(), Pkg.Name()) != forceEssential.end())
   {
      if ((essential == "native" && Pkg->Arch != 0 && myArch == Pkg.Arch()) ||
	  essential == "all")
	 Pkg->Flags |= pkgCache::Flag::Essential | pkgCache::Flag::Important;
      else
	 Pkg->Flags |= pkgCache::Flag::Important;
   }
   else if (std::find(forceImportant.begin(), forceImportant.end(), Pkg.Name()) != forceImportant.end())
      Pkg->Flags |= pkgCache::Flag::Important;

   auto phased = Section.FindULL(pkgTagSection::Key::Phased_Update_Percentage, 100);
   if (phased != 100)
   {
      if (not Ver.PhasedUpdatePercentage(phased))
	 _error->Warning("Ignoring invalid Phased-Update-Percentage value");
   }

   if (ParseStatus(Pkg,Ver) == false)
      return false;
   return true;
}
									/*}}}*/
// ListParser::VersionHash - Compute a unique hash for this version	/*{{{*/
// ---------------------------------------------------------------------
/* */
uint32_t debListParser::VersionHash()
{
   static constexpr pkgTagSection::Key Sections[] ={
      pkgTagSection::Key::Installed_Size,
      pkgTagSection::Key::Depends,
      pkgTagSection::Key::Pre_Depends,
//    pkgTagSection::Key::Suggests,
//    pkgTagSection::Key::Recommends",
      pkgTagSection::Key::Conflicts,
      pkgTagSection::Key::Breaks,
      pkgTagSection::Key::Replaces};
   unsigned long Result = 5381;
   for (auto I : Sections)
   {
      const char *Start;
      const char *End;
      if (Section.Find(I,Start,End) == false)
	 continue;
      
      /* Strip out any spaces from the text, this undoes dpkgs reformatting
         of certain fields. dpkg also has the rather interesting notion of
         reformatting depends operators < -> <=, so we drop all = from the
	 string to make that not matter. */
      for (; Start != End; ++Start)
      {
	 // Strip away 0: epochs from input
	 if (*Start == '0' && Start[1] == ':') {
	    Start++;	// Skip the :
	    continue;	// Skip the 0
	 }
	 if (isspace_ascii(*Start) != 0 || *Start == '=')
	    continue;
	 Result = 33 * Result + tolower_ascii_unsafe(*Start);
      }


   }
   
   return Result;
}
									/*}}}*/
// StatusListParser::ParseStatus - Parse the status field		/*{{{*/
// ---------------------------------------------------------------------
/* Status lines are of the form,
     Status: want flag status
   want = unknown, install, hold, deinstall, purge
   flag = ok, reinstreq
   status = not-installed, config-files, half-installed, unpacked,
            half-configured, triggers-awaited, triggers-pending, installed
 */
bool debListParser::ParseStatus(pkgCache::PkgIterator &,
				pkgCache::VerIterator &Ver)
{
   // the status file has no info about the download size and
   // usually this is fine as we will have picked that info up already –
   // except if we have volatile sources which are parsed after the status file.
   if (Ver->Size == 0)
      Ver->Size = Section.FindULL(pkgTagSection::Key::Size);
   return true;
}
bool debStatusListParser::ParseStatus(pkgCache::PkgIterator &Pkg,
				pkgCache::VerIterator &Ver)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(pkgTagSection::Key::Status,Start,Stop) == false)
      return true;

   // UsePackage() is responsible for setting the flag in the default case
   bool const static essential = _config->Find("pkgCacheGen::Essential", "") == "installed";
   if (essential == true &&
       Section.FindFlag(pkgTagSection::Key::Essential,Pkg->Flags,pkgCache::Flag::Essential) == false)
      return false;

   // Isolate the first word
   const char *I = Start;
   for(; I < Stop && *I != ' '; I++);
   if (I >= Stop || *I != ' ')
      return _error->Error("Malformed Status line");

   // Process the want field
   WordList WantList[] = {{"unknown",pkgCache::State::Unknown},
                          {"install",pkgCache::State::Install},
                          {"hold",pkgCache::State::Hold},
                          {"deinstall",pkgCache::State::DeInstall},
                          {"purge",pkgCache::State::Purge},
                          {"", 0}};
   if (GrabWord(StringView(Start,I-Start),WantList,Pkg->SelectedState) == false)
      return _error->Error("Malformed 1st word in the Status line");

   // Isloate the next word
   I++;
   Start = I;
   for(; I < Stop && *I != ' '; I++);
   if (I >= Stop || *I != ' ')
      return _error->Error("Malformed status line, no 2nd word");

   // Process the flag field
   WordList FlagList[] = {{"ok",pkgCache::State::Ok},
                          {"reinstreq",pkgCache::State::ReInstReq},
                          {"hold",pkgCache::State::HoldInst},
                          {"hold-reinstreq",pkgCache::State::HoldReInstReq},
                          {"", 0}};
   if (GrabWord(StringView(Start,I-Start),FlagList,Pkg->InstState) == false)
      return _error->Error("Malformed 2nd word in the Status line");

   // Isloate the last word
   I++;
   Start = I;
   for(; I < Stop && *I != ' '; I++);
   if (I != Stop)
      return _error->Error("Malformed Status line, no 3rd word");

   // Process the flag field
   WordList StatusList[] = {{"not-installed",pkgCache::State::NotInstalled},
                            {"config-files",pkgCache::State::ConfigFiles},
                            {"half-installed",pkgCache::State::HalfInstalled},
                            {"unpacked",pkgCache::State::UnPacked},
                            {"half-configured",pkgCache::State::HalfConfigured},
                            {"triggers-awaited",pkgCache::State::TriggersAwaited},
                            {"triggers-pending",pkgCache::State::TriggersPending},
                            {"installed",pkgCache::State::Installed},
                            {"", 0}};
   if (GrabWord(StringView(Start,I-Start),StatusList,Pkg->CurrentState) == false)
      return _error->Error("Malformed 3rd word in the Status line");

   /* A Status line marks the package as indicating the current
      version as well. Only if it is actually installed.. Otherwise
      the interesting dpkg handling of the status file creates bogus 
      entries. */
   if (!(Pkg->CurrentState == pkgCache::State::NotInstalled ||
	 Pkg->CurrentState == pkgCache::State::ConfigFiles))
   {
      if (Ver.end() == true)
	 _error->Warning("Encountered status field in a non-version description");
      else
	 Pkg->CurrentVer = Ver.MapPointer();
   }
   
   return true;
}

const char *debListParser::ConvertRelation(const char *I,unsigned int &Op)
{
   // Determine the operator
   switch (*I)
   {
      case '<':
      I++;
      if (*I == '=')
      {
	 I++;
	 Op = pkgCache::Dep::LessEq;
	 break;
      }
      
      if (*I == '<')
      {
	 I++;
	 Op = pkgCache::Dep::Less;
	 break;
      }
      
      // < is the same as <= and << is really Cs < for some reason
      Op = pkgCache::Dep::LessEq;
      break;
      
      case '>':
      I++;
      if (*I == '=')
      {
	 I++;
	 Op = pkgCache::Dep::GreaterEq;
	 break;
      }
      
      if (*I == '>')
      {
	 I++;
	 Op = pkgCache::Dep::Greater;
	 break;
      }
      
      // > is the same as >= and >> is really Cs > for some reason
      Op = pkgCache::Dep::GreaterEq;
      break;
      
      case '=':
      Op = pkgCache::Dep::Equals;
      I++;
      break;
      
      // HACK around bad package definitions
      default:
      Op = pkgCache::Dep::Equals;
      break;
   }
   return I;
}
									/*}}}*/
// ListParser::ParseDepends - Parse a dependency element		/*{{{*/
// ---------------------------------------------------------------------
/* This parses the dependency elements out of a standard string in place,
   bit by bit. */
const char *debListParser::ParseDepends(const char *Start,const char *Stop,
					string &Package,string &Ver,
					unsigned int &Op, bool const &ParseArchFlags,
					bool const &StripMultiArch,
					bool const &ParseRestrictionsList,
				        string const &Arch)
{
   StringView PackageView;
   StringView VerView;

   auto res = ParseDepends(Start, Stop, PackageView, VerView, Op, (bool)ParseArchFlags,
   (bool) StripMultiArch, (bool) ParseRestrictionsList, Arch);
   Package = PackageView.to_string();
   Ver = VerView.to_string();

   return res;
}

const char *debListParser::ParseDepends(const char *Start, const char *Stop,
					StringView &Package, StringView &Ver,
					unsigned int &Op, bool ParseArchFlags,
					bool StripMultiArch,
					bool ParseRestrictionsList, string Arch)
{
   if (Arch.empty())
      Arch = _config->Find("APT::Architecture");
   // Strip off leading space
   for (;Start != Stop && isspace_ascii(*Start) != 0; ++Start);
   
   // Parse off the package name
   const char *I = Start;
   for (;I != Stop && isspace_ascii(*I) == 0 && *I != '(' && *I != ')' &&
	*I != ',' && *I != '|' && *I != '[' && *I != ']' &&
	*I != '<' && *I != '>'; ++I);
   
   // Malformed, no '('
   if (I != Stop && *I == ')')
      return 0;

   if (I == Start)
      return 0;
   
   // Stash the package name
   Package = StringView(Start, I - Start);

   // We don't want to confuse library users which can't handle MultiArch
   if (StripMultiArch == true) {
      size_t const found = Package.rfind(':');
      if (found != StringView::npos &&
	  (Package.substr(found) == ":any" ||
	   Package.substr(found) == ":native" ||
	   Package.substr(found +1) == Arch))
	 Package = Package.substr(0,found);
   }

   // Skip white space to the '('
   for (;I != Stop && isspace_ascii(*I) != 0 ; I++);
   
   // Parse a version
   if (I != Stop && *I == '(')
   {
      // Skip the '('
      for (I++; I != Stop && isspace_ascii(*I) != 0 ; I++);
      if (I + 3 >= Stop)
	 return 0;
      I = ConvertRelation(I,Op);
      
      // Skip whitespace
      for (;I != Stop && isspace_ascii(*I) != 0; I++);
      Start = I;
      I = (const char*) memchr(I, ')', Stop - I);
      if (I == NULL || Start == I)
	 return 0;
      
      // Skip trailing whitespace
      const char *End = I;
      for (; End > Start && isspace_ascii(End[-1]); End--);
      
      Ver = StringView(Start,End-Start);
      I++;
   }
   else
   {
      Ver = StringView();
      Op = pkgCache::Dep::NoOp;
   }
   
   // Skip whitespace
   for (;I != Stop && isspace_ascii(*I) != 0; I++);

   if (unlikely(ParseArchFlags == true))
   {
      APT::CacheFilter::PackageArchitectureMatchesSpecification matchesArch(Arch, false);

      // Parse an architecture
      if (I != Stop && *I == '[')
      {
	 ++I;
	 // malformed
	 if (unlikely(I == Stop))
	    return 0;

	 const char *End = I;
	 bool Found = false;
	 bool NegArch = false;
	 while (I != Stop)
	 {
	    // look for whitespace or ending ']'
	    for (;End != Stop && !isspace_ascii(*End) && *End != ']'; ++End);

	    if (unlikely(End == Stop))
	       return 0;

	    if (*I == '!')
	    {
	       NegArch = true;
	       ++I;
	    }

	    std::string const arch(I, End);
	    if (arch.empty() == false && matchesArch(arch.c_str()) == true)
	    {
	       Found = true;
	       if (I[-1] != '!')
		  NegArch = false;
	       // we found a match, so fast-forward to the end of the wildcards
	       for (; End != Stop && *End != ']'; ++End);
	    }

	    if (*End++ == ']') {
	       I = End;
	       break;
	    }

	    I = End;
	    for (;I != Stop && isspace_ascii(*I) != 0; I++);
	 }

	 if (NegArch == true)
	    Found = !Found;

	 if (Found == false)
	    Package = ""; /* not for this arch */
      }

      // Skip whitespace
      for (;I != Stop && isspace_ascii(*I) != 0; I++);
   }

   if (unlikely(ParseRestrictionsList == true))
   {
      // Parse a restrictions formula which is in disjunctive normal form:
      // (foo AND bar) OR (blub AND bla)

      std::vector<string> const profiles = APT::Configuration::getBuildProfiles();

      // if the next character is a restriction list, then by default the
      // dependency does not apply and the conditions have to be checked
      // if the next character is not a restriction list, then by default the
      // dependency applies
      bool applies1 = (*I != '<');
      while (I != Stop)
      {
	 if (*I != '<')
	     break;

	 ++I;
	 // malformed
	 if (unlikely(I == Stop))
	    return 0;

	 const char *End = I;

	 // if of the prior restriction list is already fulfilled, then
	 // we can just skip to the end of the current list
	 if (applies1) {
	    for (;End != Stop && *End != '>'; ++End);
	    I = ++End;
	    // skip whitespace
	    for (;I != Stop && isspace_ascii(*I) != 0; I++);
	 } else {
	    bool applies2 = true;
	    // all the conditions inside a restriction list have to be
	    // met so once we find one that is not met, we can skip to
	    // the end of this list
	    while (I != Stop)
	    {
	       // look for whitespace or ending '>'
	       // End now points to the character after the current term
	       for (;End != Stop && !isspace_ascii(*End) && *End != '>'; ++End);

	       if (unlikely(End == Stop))
		  return 0;

	       bool NegRestriction = false;
	       if (*I == '!')
	       {
		  NegRestriction = true;
		  ++I;
	       }

	       std::string const restriction(I, End);
	       if (restriction.empty() == false && profiles.empty() == false &&
		  std::find(profiles.begin(), profiles.end(), restriction) != profiles.end())
	       {
		  if (NegRestriction) {
		     applies2 = false;
		     // since one of the terms does not apply we don't have to check the others
		     for (; End != Stop && *End != '>'; ++End);
		  }
	       } else {
		  if (!NegRestriction) {
		     applies2 = false;
		     // since one of the terms does not apply we don't have to check the others
		     for (; End != Stop && *End != '>'; ++End);
		  }
	       }

	       if (*End++ == '>') {
		  I = End;
		  // skip whitespace
		  for (;I != Stop && isspace_ascii(*I) != 0; I++);
		  break;
	       }

	       I = End;
	       // skip whitespace
	       for (;I != Stop && isspace_ascii(*I) != 0; I++);
	    }
	    if (applies2) {
	       applies1 = true;
	    }
	 }
      }

      if (applies1 == false) {
	 Package = ""; //not for this restriction
      }
   }

   if (I != Stop && *I == '|')
      Op |= pkgCache::Dep::Or;
   
   if (I == Stop || *I == ',' || *I == '|')
   {
      if (I != Stop)
	 for (I++; I != Stop && isspace_ascii(*I) != 0; I++);
      return I;
   }
   
   return 0;
}
									/*}}}*/
// ListParser::ParseDepends - Parse a dependency list			/*{{{*/
// ---------------------------------------------------------------------
/* This is the higher level depends parser. It takes a tag and generates
   a complete depends tree for the given version. */
bool debListParser::ParseDepends(pkgCache::VerIterator &Ver,
				 pkgTagSection::Key Key,unsigned int Type)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Key,Start,Stop) == false || Start == Stop)
      return true;

   string const pkgArch = Ver.Arch();
   bool const barbarianArch = not APT::Configuration::checkArchitecture(pkgArch);

   while (1)
   {
      StringView Package;
      StringView Version;
      unsigned int Op;

      Start = ParseDepends(Start, Stop, Package, Version, Op, false, false, false, myArch);
      if (Start == 0)
	 return _error->Error("Problem parsing dependency %zu of %s:%s=%s", static_cast<size_t>(Key), // TODO
			      Ver.ParentPkg().Name(), Ver.Arch(), Ver.VerStr());
      size_t const found = Package.rfind(':');

      if (found == string::npos)
      {
	 if (NewDepends(Ver,Package,pkgArch,Version,Op,Type) == false)
	    return false;
      }
      else if (Package.substr(found) == ":any")
      {
	 if (barbarianArch)
	 {
	    if (not NewDepends(Ver, Package, "any", Version, Op | pkgCache::Dep::Or, Type))
	       return false;
	    if (not NewDepends(Ver, Package.substr(0, found), pkgArch, Version, Op, Type))
	       return false;
	 }
	 else if (not NewDepends(Ver, Package, "any", Version, Op, Type))
	    return false;
      }
      else
      {
	 // Such dependencies are not supposed to be accepted …
	 // … but this is probably the best thing to do anyway
	 if (Package.substr(found + 1) == "native")
	 {
	    std::string const Pkg = Package.substr(0, found).to_string() + ':' + Ver.Cache()->NativeArch();
	    if (NewDepends(Ver, Pkg, "any", Version, Op | pkgCache::Dep::ArchSpecific, Type) == false)
	       return false;
	 }
	 else if (NewDepends(Ver, Package, "any", Version, Op | pkgCache::Dep::ArchSpecific, Type) == false)
	    return false;
      }

      if (Start == Stop)
	 break;
   }
   return true;
}
									/*}}}*/
// ListParser::ParseProvides - Parse the provides list			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::ParseProvides(pkgCache::VerIterator &Ver)
{
   /* it is unlikely, but while parsing dependencies, we might have already
      picked up multi-arch implicit provides which we do not want to duplicate here */
   bool hasProvidesAlready = false;
   std::string const spzName = Ver.ParentPkg().FullName(false);
   {
      for (pkgCache::PrvIterator Prv = Ver.ProvidesList(); Prv.end() == false; ++Prv)
      {
	 if (Prv.IsMultiArchImplicit() == false || (Prv->Flags & pkgCache::Flag::ArchSpecific) == 0)
	    continue;
	 if (spzName != Prv.OwnerPkg().FullName(false))
	    continue;
	 hasProvidesAlready = true;
	 break;
      }
   }

   string const Arch = Ver.Arch();
   bool const barbarianArch = not APT::Configuration::checkArchitecture(Arch);
   const char *Start;
   const char *Stop;
   if (Section.Find(pkgTagSection::Key::Provides,Start,Stop) == true)
   {
      StringView Package;
      StringView Version;
      unsigned int Op;

      do
      {
	 Start = ParseDepends(Start,Stop,Package,Version,Op, false, false, false);
	 const size_t archfound = Package.rfind(':');
	 if (Start == 0)
	    return _error->Error("Problem parsing Provides line of %s:%s=%s", Ver.ParentPkg().Name(), Ver.Arch(), Ver.VerStr());
	 if (unlikely(Op != pkgCache::Dep::NoOp && Op != pkgCache::Dep::Equals)) {
	    _error->Warning("Ignoring non-equal Provides for package %s in %s:%s=%s", Package.to_string().c_str(), Ver.ParentPkg().Name(), Ver.Arch(), Ver.VerStr());
	 } else if (archfound != string::npos) {
	    StringView spzArch = Package.substr(archfound + 1);
	    if (spzArch != "any")
	    {
	       if (NewProvides(Ver, Package.substr(0, archfound), spzArch, Version, pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific) == false)
		  return false;
	    }
	    if (NewProvides(Ver, Package, "any", Version, pkgCache::Flag::ArchSpecific) == false)
	       return false;
	 } else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign) {
	    if (not barbarianArch)
	    {
	       if (NewProvidesAllArch(Ver, Package, Version, 0) == false)
		  return false;
	    }
	    else if (NewProvides(Ver, Package, Arch, Version, 0) == false)
	       return false;
	 } else {
	    if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed && not barbarianArch)
	    {
	       if (NewProvides(Ver, Package.to_string().append(":any"), "any", Version, pkgCache::Flag::MultiArchImplicit) == false)
		  return false;
	    }
	    if (NewProvides(Ver, Package, Arch, Version, 0) == false)
	       return false;
	 }
	 if (archfound == std::string::npos)
	 {
	    string spzName = Package.to_string();
	    spzName.push_back(':');
	    spzName.append(Ver.ParentPkg().Arch());
	    pkgCache::PkgIterator const spzPkg = Ver.Cache()->FindPkg(spzName, "any");
	    if (spzPkg.end() == false)
	    {
	       if (NewProvides(Ver, spzName, "any", Version, pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific) == false)
		  return false;
	    }
	 }
      } while (Start != Stop);
   }

   if (not barbarianArch)
   {
      if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
      {
	 string const Package = string(Ver.ParentPkg().Name()).append(":").append("any");
	 if (NewProvides(Ver, Package, "any", Ver.VerStr(), pkgCache::Flag::MultiArchImplicit) == false)
	    return false;
      }
      else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
      {
	 if (NewProvidesAllArch(Ver, Ver.ParentPkg().Name(), Ver.VerStr(), pkgCache::Flag::MultiArchImplicit) == false)
	    return false;
      }
   }

   if (hasProvidesAlready == false)
   {
      pkgCache::PkgIterator const spzPkg = Ver.Cache()->FindPkg(spzName, "any");
      if (spzPkg.end() == false)
      {
	 if (NewProvides(Ver, spzName, "any", Ver.VerStr(), pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific) == false)
	    return false;
      }
   }
   return true;
}
									/*}}}*/
// ListParser::GrabWord - Matches a word and returns			/*{{{*/
// ---------------------------------------------------------------------
/* Looks for a word in a list of words - for ParseStatus */
bool debListParser::GrabWord(StringView Word, WordList const *List, unsigned char &Out)
{
   for (unsigned int C = 0; List[C].Str.empty() == false; C++)
   {
      if (Word.length() == List[C].Str.length() &&
	  strncasecmp(Word.data(), List[C].Str.data(), Word.length()) == 0)
      {
	 Out = List[C].Val;
	 return true;
      }
   }
   return false;
}
									/*}}}*/
// ListParser::Step - Move to the next section in the file		/*{{{*/
// ---------------------------------------------------------------------
/* This has to be careful to only process the correct architecture */
bool debListParser::Step()
{
   iOffset = Tags.Offset();
   return Tags.Step(Section);
}
									/*}}}*/
// ListParser::GetPrio - Convert the priority from a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned char debListParser::GetPrio(string Str)
{
   unsigned char Out;
   if (GrabWord(Str,PrioList,Out) == false)
      Out = pkgCache::State::Extra;
   
   return Out;
}
									/*}}}*/
bool debListParser::SameVersion(uint32_t Hash,		/*{{{*/
      pkgCache::VerIterator const &Ver)
{
   if (pkgCacheListParser::SameVersion(Hash, Ver) == false)
      return false;
   // status file has no (Download)Size, but all others are fair game
   // status file is parsed last, so the first version we encounter is
   // probably also the version we have downloaded
   unsigned long long const Size = Section.FindULL(pkgTagSection::Key::Size);
   if (Size != 0 && Ver->Size != 0 && Size != Ver->Size)
      return false;
   // available everywhere, but easier to check here than to include in VersionHash
   unsigned char MultiArch = ParseMultiArch(false);
   if (MultiArch != Ver->MultiArch)
      return false;
   // for all practical proposes (we can check): same version
   return true;
}
									/*}}}*/

debDebFileParser::debDebFileParser(FileFd *File, std::string const &DebFile)
   : debListParser(File), DebFile(DebFile)
{
}

bool debDebFileParser::UsePackage(pkgCache::PkgIterator &Pkg,
                                  pkgCache::VerIterator &Ver)
{
   if (not debListParser::UsePackage(Pkg, Ver))
      return false;
   // we use the full file path as a provides so that the file is found by its name
   // using the MultiArchImplicit flag for this is a bit of a stretch
   return NewProvides(Ver, DebFile, Pkg.Cache()->NativeArch(), Ver.VerStr(), pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific);
}

debListParser::~debListParser() {}
