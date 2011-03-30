// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Cache Generator - Generator for the cache structure.

   This builds the cache structure from the abstract package list parser.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/edsplistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/macros.h>
									/*}}}*/

// ListParser::edspListParser - Constructor				/*{{{*/
edspListParser::edspListParser(FileFd *File, string const &Arch) : debListParser(File, Arch)
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
string edspListParser::Description()
{
   return "";
}
string edspListParser::DescriptionLanguage()
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
   const char *Start;
   const char *Stop;
   if (Section.Find("Status",Start,Stop) == false)
      return true;

   // UsePackage() is responsible for setting the flag in the default case
   bool const static essential = _config->Find("pkgCacheGen::Essential", "") == "installed";
   if (essential == true &&
       Section.FindFlag("Essential",Pkg->Flags,pkgCache::Flag::Essential) == false)
      return false;

   // Isolate the first word
   const char *I = Start;
   for(; I < Stop && *I != ' '; I++);

   // Process the flag field
   WordList StatusList[] = {{"installed",pkgCache::State::Installed},
                            {}};
   if (GrabWord(string(Start,I-Start),StatusList,Pkg->CurrentState) == false)
      return _error->Error("Malformed Status line");

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
	 Pkg->CurrentVer = Ver.Index();
   }
   
   return true;
}
									/*}}}*/
// ListParser::LoadReleaseInfo - Load the release information		/*{{{*/
bool edspListParser::LoadReleaseInfo(pkgCache::PkgFileIterator &FileI,
				    FileFd &File, string component)
{
   return true;
}
									/*}}}*/
