// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Cache Generator - Generator for the cache structure.

   This builds the cache structure from the abstract package list parser.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/edsplistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/macros.h>
									/*}}}*/

// ListParser::edspListParser - Constructor				/*{{{*/
edspListParser::edspListParser(FileFd *File, std::string const &Arch) : debListParser(File, Arch)
{}
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

   return true;
}
									/*}}}*/
// ListParser::LoadReleaseInfo - Load the release information		/*{{{*/
bool edspListParser::LoadReleaseInfo(pkgCache::PkgFileIterator &FileI,
				    FileFd &File, std::string component)
{
   return true;
}
									/*}}}*/
