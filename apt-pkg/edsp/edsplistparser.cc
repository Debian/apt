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

									/*}}}*/

class edspListParserPrivate						/*{{{*/
{
public:
   FileFd extendedstates;
   FileFd preferences;

   edspListParserPrivate()
   {
      std::string const states = _config->FindFile("Dir::State::extended_states");
      if (states != "/dev/null")
	 unlink(states.c_str());
      extendedstates.Open(states, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive, 0600);
      std::string const prefs = _config->FindFile("Dir::Etc::preferences");
      if (prefs != "/dev/null")
	 unlink(prefs.c_str());
      preferences.Open(prefs, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive, 0600);
   }
};
									/*}}}*/
// ListParser::edspListParser - Constructor				/*{{{*/
edspListParser::edspListParser(FileFd *File) : debListParser(File), d(new edspListParserPrivate())
{
}
									/*}}}*/
// ListParser::NewVersion - Fill in the version structure		/*{{{*/
bool edspListParser::NewVersion(pkgCache::VerIterator &Ver)
{
   Ver->ID = Section.FindI("APT-ID", Ver->ID);
   return debListParser::NewVersion(Ver);
}
									/*}}}*/
// ListParser::Description - Return the description string		/*{{{*/
// ---------------------------------------------------------------------
/* Sorry, no description for the resolvers… */
std::string edspListParser::Description()
{
   return "";
}
std::string edspListParser::DescriptionLanguage()
{
   return "";
}
MD5SumValue edspListParser::Description_md5()
{
   return MD5SumValue("");
}
									/*}}}*/
// ListParser::VersionHash - Compute a unique hash for this version	/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned short edspListParser::VersionHash()
{
   if (Section.Exists("APT-Hash") == true)
      return Section.FindI("APT-Hash");
   else if (Section.Exists("APT-ID") == true)
      return Section.FindI("APT-ID");
   return 0;
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
      if (d->extendedstates.Write(out.c_str(), out.length()) == false)
	 return false;
   }

   signed short const pinvalue = Section.FindI("APT-Pin", 500);
   if (pinvalue != 500)
   {
      std::string out;
      strprintf(out, "Package: %s\nPin: version %s\nPin-Priority: %d\n\n", Pkg.FullName().c_str(), Ver.VerStr(), pinvalue);
      if (d->preferences.Write(out.c_str(), out.length()) == false)
	 return false;
   }

   return true;
}
									/*}}}*/
// ListParser::LoadReleaseInfo - Load the release information		/*{{{*/
APT_CONST bool edspListParser::LoadReleaseInfo(pkgCache::RlsFileIterator & /*FileI*/,
				    FileFd & /*File*/, std::string const &/*component*/)
{
   return true;
}
									/*}}}*/
edspListParser::~edspListParser()					/*{{{*/
{
   delete d;
}
									/*}}}*/
