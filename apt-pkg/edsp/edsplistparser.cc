// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Cache Generator - Generator for the cache structure.

   This builds the cache structure from the abstract package list parser.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/edsplistparser.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/string_view.h>

#include <array>

									/*}}}*/

// ListParser::edspListParser - Constructor				/*{{{*/
edspLikeListParser::edspLikeListParser(FileFd * const File) : debListParser(File)
{
}
edspListParser::edspListParser(FileFd * const File) : edspLikeListParser(File)
{
   std::string const states = _config->FindFile("Dir::State::extended_states");
   RemoveFile("edspListParserPrivate", states);
   extendedstates.Open(states, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive, 0600);
   std::string const prefs = _config->FindFile("Dir::Etc::preferences");
   RemoveFile("edspListParserPrivate", prefs);
   preferences.Open(prefs, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive, 0600);
}
									/*}}}*/
// ListParser::NewVersion - Fill in the version structure		/*{{{*/
bool edspLikeListParser::NewVersion(pkgCache::VerIterator &Ver)
{
   _system->SetVersionMapping(Ver->ID, Section.FindI("APT-ID", Ver->ID));
   return debListParser::NewVersion(Ver);
}
									/*}}}*/
// ListParser::Description - Return the description string		/*{{{*/
// ---------------------------------------------------------------------
/* Sorry, no description for the resolvers… */
std::vector<std::string> edspLikeListParser::AvailableDescriptionLanguages()
{
   return {};
}
APT::StringView edspLikeListParser::Description_md5()
{
   return APT::StringView();
}
									/*}}}*/
// ListParser::VersionHash - Compute a unique hash for this version	/*{{{*/
unsigned short edspLikeListParser::VersionHash()
{
   if (Section.Exists("APT-Hash") == true)
      return Section.FindI("APT-Hash");
   else if (Section.Exists("APT-ID") == true)
      return Section.FindI("APT-ID");
   return 0;
}
									/*}}}*/
// ListParser::LoadReleaseInfo - Load the release information		/*{{{*/
APT_CONST bool edspLikeListParser::LoadReleaseInfo(pkgCache::RlsFileIterator & /*FileI*/,
				    FileFd & /*File*/, std::string const &/*component*/)
{
   return true;
}
									/*}}}*/
// ListParser::ParseStatus - Parse the status field			/*{{{*/
// ---------------------------------------------------------------------
/* The Status: line here is not a normal dpkg one but just one which tells
   use if the package is installed or not, where missing means not. */
bool edspListParser::ParseStatus(pkgCache::PkgIterator &Pkg,
				pkgCache::VerIterator &Ver)
{
   unsigned long state = 0;
   if (Section.FindFlag("Hold",state,pkgCache::State::Hold) == false)
      return false;
   if (state != 0)
      Pkg->SelectedState = pkgCache::State::Hold;

   state = 0;
   if (Section.FindFlag("Installed",state,pkgCache::State::Installed) == false)
      return false;
   if (state != 0)
   {
      Pkg->CurrentState = pkgCache::State::Installed;
      Pkg->CurrentVer = Ver.Index();
   }

   if (Section.FindB("APT-Automatic", false))
   {
      std::string out;
      strprintf(out, "Package: %s\nArchitecture: %s\nAuto-Installed: 1\n\n", Pkg.Name(), Pkg.Arch());
      if (extendedstates.Write(out.c_str(), out.length()) == false)
	 return false;
   }

   // FIXME: Using an overriding pin is wrong.
   if (Section.FindB("APT-Candidate", false))
   {
      std::string out;
      strprintf(out, "Package: %s\nPin: version %s\nPin-Priority: 9999\n\n", Pkg.FullName().c_str(), Ver.VerStr());
      if (preferences.Write(out.c_str(), out.length()) == false)
	 return false;
   }

   signed short const pinvalue = Section.FindI("APT-Pin", 500);
   if (pinvalue != 500)
   {
      std::string out;
      strprintf(out, "Package: %s\nPin: version %s\nPin-Priority: %d\n\n", Pkg.FullName().c_str(), Ver.VerStr(), pinvalue);
      if (preferences.Write(out.c_str(), out.length()) == false)
	 return false;
   }

   return true;
}
									/*}}}*/

// ListParser::eippListParser - Constructor				/*{{{*/
eippListParser::eippListParser(FileFd *File) : edspLikeListParser(File)
{
}
									/*}}}*/
// ListParser::ParseStatus - Parse the status field			/*{{{*/
// ---------------------------------------------------------------------
/* The Status: line here is not a normal dpkg one but just one which tells
   use if the package is installed or not, where missing means not. */
bool eippListParser::ParseStatus(pkgCache::PkgIterator &Pkg,
				pkgCache::VerIterator &Ver)
{
   // Process the flag field
   static std::array<WordList, 8> const statusvalues = {{
      {"not-installed",pkgCache::State::NotInstalled},
      {"config-files",pkgCache::State::ConfigFiles},
      {"half-installed",pkgCache::State::HalfInstalled},
      {"unpacked",pkgCache::State::UnPacked},
      {"half-configured",pkgCache::State::HalfConfigured},
      {"triggers-awaited",pkgCache::State::TriggersAwaited},
      {"triggers-pending",pkgCache::State::TriggersPending},
      {"installed",pkgCache::State::Installed},
   }};
   auto const status = Section.Find("Status");
   if (status.empty() == false)
   {
      for (auto && sv: statusvalues)
      {
	 if (status != sv.Str)
	    continue;
	 Pkg->CurrentState = sv.Val;
	 switch (Pkg->CurrentState)
	 {
	    case pkgCache::State::NotInstalled:
	    case pkgCache::State::ConfigFiles:
	       break;
	    case pkgCache::State::HalfInstalled:
	    case pkgCache::State::UnPacked:
	    case pkgCache::State::HalfConfigured:
	    case pkgCache::State::TriggersAwaited:
	    case pkgCache::State::TriggersPending:
	    case pkgCache::State::Installed:
	       Pkg->CurrentVer = Ver.Index();
	       break;
	 }
	 break;
      }
   }

   return true;
}
									/*}}}*/

edspLikeListParser::~edspLikeListParser() {}
edspListParser::~edspListParser() {}
eippListParser::~eippListParser() {}
