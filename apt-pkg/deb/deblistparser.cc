// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: deblistparser.cc,v 1.1 1998/07/04 05:58:08 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <pkglib/deblistparser.h>
#include <pkglib/error.h>
#include <system.h>
									/*}}}*/

// ListParser::debListParser - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debListParser::debListParser(File &File) : Tags(File)
{
   Step();
}
									/*}}}*/
// ListParser::FindTag - Find the tag and return a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debListParser::FindTag(const char *Tag)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Tag,Start,Stop) == false)
      return string();
   return string(Start,Stop - Start);
}
									/*}}}*/
// ListParser::UniqFindTagWrite - Find the tag and write a unq string	/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long debListParser::UniqFindTagWrite(const char *Tag)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Tag,Start,Stop) == false)
      return 0;
   return WriteUniqString(Start,Stop - Start);
}
									/*}}}*/
// ListParser::HandleFlag - Sets a flag variable based on a tag		/*{{{*/
// ---------------------------------------------------------------------
/* This checks the tag for true/false yes/no etc */
bool debListParser::HandleFlag(const char *Tag,unsigned long &Flags,
			       unsigned long Flag)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Tag,Start,Stop) == false)
      return true;
   
   int Set = 2;
   if (strncasecmp(Start,"yes",Stop - Start) == 0)
      Set = 1;
   if (strncasecmp(Start,"true",Stop - Start) == 0)
      Set = 1;
   if (strncasecmp(Start,"no",Stop - Start) == 0)
      Set = 0;
   if (strncasecmp(Start,"false",Stop - Start) == 0)
      Set = 0;
   if (Set == 2)
   {
      _error->Warning("Unknown flag value");
      return true;
   }
   
   if (Set == 0)
      Flags &= ~Flag;
   if (Set == 1)
      Flags |= Flag;
   return true;
}
									/*}}}*/
// ListParser::Package - Return the package name			/*{{{*/
// ---------------------------------------------------------------------
/* This is to return the name of the package this section describes */
string debListParser::Package()
{
   string Result = FindTag("Package");
   if (Result.empty() == true)
      _error->Error("Encoutered a section with no Package: header");
   return Result;
}
									/*}}}*/
// ListParser::Version - Return the version string			/*{{{*/
// ---------------------------------------------------------------------
/* This is to return the string describing the version in debian form,
   epoch:upstream-release. If this returns the blank string then the 
   entry is assumed to only describe package properties */
string debListParser::Version()
{
   return FindTag("Version");
}
									/*}}}*/
// ListParser::NewPackage - Fill in the package structure		/*{{{*/
// ---------------------------------------------------------------------
/* This is called when a new package structure is created. It must fill
   in the static package information. */
bool debListParser::NewPackage(pkgCache::PkgIterator Pkg)
{
   // Debian doesnt have anything, everything is condionally megered
   return true;
}
									/*}}}*/
// ListParser::NewVersion - Fill in the version structure		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::NewVersion(pkgCache::VerIterator Ver)
{   
   return true;
}
									/*}}}*/
// ListParser::UsePackage - Update a package structure			/*{{{*/
// ---------------------------------------------------------------------
/* This is called to update the package with any new information 
   that might be found in the section */
bool debListParser::UsePackage(pkgCache::PkgIterator Pkg,
			       pkgCache::VerIterator Ver)
{
   if (Pkg->Section == 0)
      if ((Pkg->Section = UniqFindTagWrite("Section")) == 0)
	 return false;
   if (HandleFlag("Essential",Pkg->Flags,pkgCache::Essential) == false)
      return false;
   if (HandleFlag("Immediate-Configure",Pkg->Flags,pkgCache::ImmediateConf) == false)
      return false;
   if (ParseStatus(Pkg,Ver) == false)
      return false;
   return true;
}
									/*}}}*/
// ListParser::ParseStatus - Parse the status feild			/*{{{*/
// ---------------------------------------------------------------------
/* Status lines are of the form,
     Status: want flag status
   want = unknown, install, hold, deinstall, purge
   flag = ok, reinstreq, hold, hold-reinstreq
   status = not-installed, unpacked, half-configured, uninstalled,
            half-installed, config-files, post-inst-failed, 
            removal-failed, installed
   
   Some of the above are obsolete (I think?) flag = hold-* and 
   status = post-inst-failed, removal-failed at least.
 */
bool debListParser::ParseStatus(pkgCache::PkgIterator Pkg,
				pkgCache::VerIterator Ver)
{
   const char *Start;
   const char *Stop;
   if (Section.Find("Status",Start,Stop) == false)
      return true;
   
   // Isolate the first word
   const char *I = Start;
   for(; I < Stop && *I != ' '; I++);
   if (I >= Stop || *I != ' ')
      return _error->Error("Malformed Status line");

   // Process the want field
   WordList WantList[] = {{"unknown",pkgCache::Unknown},
                          {"install",pkgCache::Install},
                          {"hold",pkgCache::Hold},
                          {"deinstall",pkgCache::DeInstall},
                          {"purge",pkgCache::Purge}};
   if (GrabWord(string(Start,I-Start),WantList,
		_count(WantList),Pkg->SelectedState) == false)
      return _error->Error("Malformed 1st word in the Status line");

   // Isloate the next word
   I++;
   Start = I;
   for(; I < Stop && *I != ' '; I++);
   if (I >= Stop || *I != ' ')
      return _error->Error("Malformed status line, no 2nd word");

   // Process the flag field
   WordList FlagList[] = {{"ok",pkgCache::Ok},
                          {"reinstreq",pkgCache::ReInstReq},
                          {"hold",pkgCache::HoldInst},
                          {"hold-reinstreq",pkgCache::HoldReInstReq}};
   if (GrabWord(string(Start,I-Start),FlagList,
		_count(FlagList),Pkg->InstState) == false)
      return _error->Error("Malformed 2nd word in the Status line");

   // Isloate the last word
   I++;
   Start = I;
   for(; I < Stop && *I != ' '; I++);
   if (I != Stop)
      return _error->Error("Malformed Status line, no 3rd word");

   // Process the flag field
   WordList StatusList[] = {{"not-installed",pkgCache::NotInstalled},
                            {"unpacked",pkgCache::UnPacked},
                            {"half-configured",pkgCache::HalfConfigured},
                            {"installed",pkgCache::Installed},
                            {"uninstalled",pkgCache::UnInstalled},
                            {"half-installed",pkgCache::HalfInstalled},
                            {"config-files",pkgCache::ConfigFiles},
                            {"post-inst-failed",pkgCache::HalfConfigured},
                            {"removal-failed",pkgCache::HalfInstalled}};
   if (GrabWord(string(Start,I-Start),StatusList,
		_count(StatusList),Pkg->CurrentState) == false)
      return _error->Error("Malformed 3rd word in the Status line");

   /* A Status line marks the package as indicating the current
      version as well. Only if it is actually installed.. Otherwise
      the interesting dpkg handling of the status file creates bogus 
      entries. */
   if (!(Pkg->CurrentState == pkgCache::NotInstalled ||
	 Pkg->CurrentState == pkgCache::ConfigFiles))
   {
      if (Ver.end() == true)
	 _error->Warning("Encountered status field in a non-version description");
      else
	 Pkg->CurrentVer = Ver.Index();
   }
   
   return true;
}
									/*}}}*/
// ListParser::GrabWord - Matches a word and returns			/*{{{*/
// ---------------------------------------------------------------------
/* Looks for a word in a list of words - for ParseStatus */
bool debListParser::GrabWord(string Word,WordList *List,int Count,
			     unsigned char &Out)
{
   for (int C = 0; C != Count; C++)
   {
      if (strcasecmp(Word.c_str(),List[C].Str) == 0)
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
/* */
bool debListParser::Step()
{
   return Tags.Step(Section);
}
									/*}}}*/
