// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: deblistparser.cc,v 1.3 1998/07/05 05:34:00 jgg Exp $
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
// ListParser::FindTagI - Find the tag and return an int		/*{{{*/
// ---------------------------------------------------------------------
/* */
signed long debListParser::FindTagI(const char *Tag,signed long Default)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Tag,Start,Stop) == false)
      return Default;
   
   // Copy it into a temp buffer so we can use strtol
   char S[300];
   if ((unsigned)(Stop - Start) >= sizeof(S))
      return Default;
   strncpy(S,Start,Stop-Start);
   S[Stop - Start] = 0;
   
   char *End;
   signed long Result = strtol(S,&End,10);
   if (S == End)
      return Default;
   return Result;
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
// ListParser::NewVersion - Fill in the version structure		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::NewVersion(pkgCache::VerIterator Ver)
{
   // Parse the section
   if ((Ver->Section = UniqFindTagWrite("Section")) == 0)
      return _error->Warning("Missing Section tag");
   
   // Archive Size
   if ((Ver->Size = (unsigned)FindTagI("Size")) == 0)
      return _error->Error("Unparsable Size field");
   
   // Unpacked Size (in K)
   if ((Ver->InstalledSize = (unsigned)FindTagI("Installed-Size")) == 0)
      return _error->Error("Unparsable Installed-Size field");
   Ver->InstalledSize *= 1024;

   // Priority
   const char *Start;
   const char *Stop;
   if (Section.Find("Priority",Start,Stop) == true)
   {
      WordList PrioList[] = {{"important",pkgCache::Important},
	                     {"required",pkgCache::Required},
	                     {"standard",pkgCache::Standard},
                             {"optional",pkgCache::Optional},
                             {"extra",pkgCache::Extra}};
      if (GrabWord(string(Start,Stop-Start),PrioList,
		   _count(PrioList),Ver->Priority) == false)
	 return _error->Error("Malformed Priority line");
   }

   if (ParseDepends(Ver,"Depends",pkgCache::Depends) == false)
      return false;
   if (ParseDepends(Ver,"PreDepends",pkgCache::PreDepends) == false)
      return false;
   if (ParseDepends(Ver,"Suggests",pkgCache::Suggests) == false)
      return false;
   if (ParseDepends(Ver,"Recommends",pkgCache::Recommends) == false)
      return false;
   if (ParseDepends(Ver,"Conflicts",pkgCache::Conflicts) == false)
      return false;
   if (ParseDepends(Ver,"Replaces",pkgCache::Depends) == false)
      return false;

   if (ParseProvides(Ver) == false)
      return false;
   
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
// ListParser::ParseStatus - Parse the status field			/*{{{*/
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
// ListParser::ParseDepends - Parse a dependency element		/*{{{*/
// ---------------------------------------------------------------------
/* This parses the dependency elements out of a standard string in place,
   bit by bit. */
const char *debListParser::ParseDepends(const char *Start,const char *Stop,
					string &Package,string &Ver,
					unsigned int &Op)
{
   // Strip off leading space
   for (;Start != Stop && isspace(*Start) != 0; Start++);
   
   // Parse off the package name
   const char *I = Start;
   for (;I != Stop && isspace(*I) == 0 && *I != '(' && *I != ')' &&
	*I != ',' && *I != '|'; I++);
   
   // Malformed, no '('
   if (I != Stop && *I == ')')
      return 0;

   if (I == Start)
      return 0;
   
   // Stash the package name
   Package.assign(Start,I - Start);
   
   // Skip white space to the '('
   for (;I != Stop && isspace(*I) != 0 ; I++);
   
   // Parse a version
   if (I != Stop && *I == '(')
   {
      // Skip the '('
      for (I++; I != Stop && isspace(*I) != 0 ; I++);
      if (I + 3 >= Stop)
	 return 0;
      
      // Determine the operator
      switch (*I)
      {
	 case '<':
	 I++;
	 if (*I == '=')
	 {
	    I++;
	    Op = pkgCache::LessEq;
	    break;
	 }
	 
	 if (*I == '<')
	 {
	    I++;
	    Op = pkgCache::Less;
	    break;
	 }
	 
	 // < is the same as <= and << is really Cs < for some reason
	 Op = pkgCache::LessEq;
	 break;
	 
	 case '>':
	 I++;
	 if (*I == '=')
	 {
	    I++;
	    Op = pkgCache::GreaterEq;
	    break;
	 }
	 
	 if (*I == '>')
	 {
	    I++;
	    Op = pkgCache::Greater;
	    break;
	 }
	 
	 // > is the same as >= and >> is really Cs > for some reason
	 Op = pkgCache::GreaterEq;
	 break;
	 
	 case '=':
	 Op = pkgCache::Equals;
	 I++;
	 break;
	 
	 // HACK around bad package definitions
	 default:
	 Op = pkgCache::Equals;
	 break;
      }
      
      // Skip whitespace
      for (;I != Stop && isspace(*I) != 0; I++);
      Start = I;
      for (;I != Stop && *I != ')'; I++);
      if (I == Stop || Start == I)
	 return 0;     
      
      Ver = string(Start,I-Start);
      I++;
   }
   else
   {
      Ver = string();
      Op = pkgCache::NoOp;
   }
   
   // Skip whitespace
   for (;I != Stop && isspace(*I) != 0; I++);
   if (I != Stop && *I == '|')
      Op |= pkgCache::Or;
   
   if (I == Stop || *I == ',' || *I == '|')
   {
      if (I != Stop)
	 for (I++; I != Stop && isspace(*I) != 0; I++);
      return I;
   }
   
   return 0;
}
									/*}}}*/
// ListParser::ParseDepends - Parse a dependency list			/*{{{*/
// ---------------------------------------------------------------------
/* This is the higher level depends parser. It takes a tag and generates
   a complete depends tree for the given version. */
bool debListParser::ParseDepends(pkgCache::VerIterator Ver,
				 const char *Tag,unsigned int Type)
{
   const char *Start;
   const char *Stop;
   if (Section.Find(Tag,Start,Stop) == false)
      return true;
   
   string Package;
   string Version;
   unsigned int Op;

   while ((Start = ParseDepends(Start,Stop,Package,Version,Op)) != Stop)
   {
      if (Start == 0)
	 return _error->Error("Problem parsing dependency %s",Tag);

      if (NewDepends(Ver,Package,Version,Op,Type) == false)
	 return false;
   }
   return true;
}
									/*}}}*/
// ListParser::ParseProvides - Parse the provides list			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debListParser::ParseProvides(pkgCache::VerIterator Ver)
{
   const char *Start;
   const char *Stop;
   if (Section.Find("Provides",Start,Stop) == false)
      return true;
   
   string Package;
   string Version;
   unsigned int Op;

   while (1)
   {
      Start = ParseDepends(Start,Stop,Package,Version,Op);
      if (Start == 0)
	 return _error->Error("Problem parsing Provides line");
      if (Op != pkgCache::NoOp)
	 return _error->Error("Malformed provides line");

      if (NewProvides(Ver,Package,Version) == false)
	 return false;

      if (Start == Stop)
	 break;
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
/* This has to be carefull to only process the correct architecture */
bool debListParser::Step()
{
   iOffset = Tags.Offset();
   while (Tags.Step(Section) == true)
   {
      /* See if this is the correct Architecture, if it isnt then we
         drop the whole section */
      const char *Start;
      const char *Stop;
      if (Section.Find("Architecture",Start,Stop) == false)
	 return true;
            
      if (strncmp(Start,"i386",Stop - Start) == 0 &&
	  strlen("i386") == (unsigned)(Stop - Start))
	 return true;

      if (strncmp(Start,"all",Stop - Start) == 0 &&
	  3 == (unsigned)(Stop - Start))
	 return true;

      iOffset = Tags.Offset();
   }   
   return false;
}
									/*}}}*/
