// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version Matching 
   
   This module takes a matching string and a type and locates the version
   record that satisfies the constraint described by the matching string.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/versionmatch.h>

#include <string>
#include <ctype.h>
#include <fnmatch.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
									/*}}}*/

using std::string;

// VersionMatch::pkgVersionMatch - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Break up the data string according to the selected type */
pkgVersionMatch::pkgVersionMatch(string Data,MatchType Type) : Type(Type)
{
   MatchAll = false;
   VerPrefixMatch = false;
   RelVerPrefixMatch = false;
   
   if (Type == None || Data.length() < 1)
      return;
   
   // Cut up the version representation
   if (Type == Version)
   {
      if (Data.end()[-1] == '*')
      {
	 VerPrefixMatch = true;
	 VerStr = string(Data,0,Data.length()-1);
      }
      else
	 VerStr = Data;
      return;
   }   
   
   if (Type == Release)
   {
      // All empty = match all
      if (Data == "*")
      {
	 MatchAll = true;
	 return;
      }
      
      // Are we a simple specification?
      string::const_iterator I = Data.begin();
      for (; I != Data.end() && *I != '='; ++I);
      if (I == Data.end())
      {
	 // Temporary
	 if (isdigit(Data[0]))
	    RelVerStr = Data;
	 else
	    RelRelease = Data;

	 if (RelVerStr.length() > 0 && RelVerStr.end()[-1] == '*')
	 {
	    RelVerPrefixMatch = true;
	    RelVerStr = string(RelVerStr.begin(),RelVerStr.end()-1);
	 }	 
	 return;
      }
            
      char Spec[300];
      char *Fragments[20];
      snprintf(Spec,sizeof(Spec),"%s",Data.c_str());
      if (TokSplitString(',',Spec,Fragments,
			 sizeof(Fragments)/sizeof(Fragments[0])) == false)
      {
	 Type = None;
	 return;
      }
      
      for (unsigned J = 0; Fragments[J] != 0; J++)
      {
	 if (strlen(Fragments[J]) < 3)
	    continue;

	 if (stringcasecmp(Fragments[J],Fragments[J]+2,"v=") == 0)
	    RelVerStr = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"o=") == 0)
	    RelOrigin = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"a=") == 0)
	    RelArchive = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"n=") == 0)
	    RelCodename = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"l=") == 0)
	    RelLabel = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"c=") == 0)
	    RelComponent = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"b=") == 0)
	    RelArchitecture = Fragments[J]+2;
      }

      if (RelVerStr.end()[-1] == '*')
      {
	 RelVerPrefixMatch = true;
	 RelVerStr = string(RelVerStr.begin(),RelVerStr.end()-1);
      }	 
      return;
   }
   
   if (Type == Origin)
   {
      if (Data[0] == '"' && Data.length() >= 2 && Data.end()[-1] == '"')
	 OrSite = Data.substr(1, Data.length() - 2);
      else
	 OrSite = Data;
      return;
   }   
}
									/*}}}*/
// VersionMatch::MatchVer - Match a version string with prefixing	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgVersionMatch::MatchVer(const char *A,string B,bool Prefix)
{
   if (A == NULL)
      return false;

   const char *Ab = A;
   const char *Ae = Ab + strlen(A);
   
   // Strings are not a compatible size.
   if (((unsigned)(Ae - Ab) != B.length() && Prefix == false) ||
       (unsigned)(Ae - Ab) < B.length())
      return false;
   
   // Match (leading?)
   if (stringcasecmp(B,Ab,Ab + B.length()) == 0)
      return true;
   
   return false;
}
									/*}}}*/
// VersionMatch::Find - Locate the best match for the select type	/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::VerIterator pkgVersionMatch::Find(pkgCache::PkgIterator Pkg)
{
   for (auto Ver = Pkg.VersionList(); not Ver.end(); ++Ver)
      if (VersionMatches(Ver))
	 return Ver;
   // check if the package provides itself in a matching version
   for (auto Prov = Pkg.ProvidesList(); not Prov.end(); ++Prov)
      if (Prov->ProvideVersion != 0 && Prov.OwnerPkg() == Prov.ParentPkg())
	 if (MatchVer(Prov.ProvideVersion(), VerStr, VerPrefixMatch) ||
	     ExpressionMatches(VerStr, Prov.ProvideVersion()))
	    return Prov.OwnerVer();
   return pkgCache::VerIterator{};
}
									/*}}}*/

// VersionMatch::Find - Locate the best match for the select type	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgVersionMatch::VersionMatches(pkgCache::VerIterator Ver)
{
   if (Type == Version)
   {
      if (MatchVer(Ver.VerStr(),VerStr,VerPrefixMatch) == true)
	 return true;
      if (ExpressionMatches(VerStr, Ver.VerStr()) == true)
	 return true;
      return false;
   }

   for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; ++VF)
      if (FileMatch(VF.File()) == true)
	 return true;

   return false;
}
									/*}}}*/

#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif

bool pkgVersionMatch::ExpressionMatches(const char *pattern, const char *string)/*{{{*/
{
   if (pattern == NULL || string == NULL)
      return false;
   if (pattern[0] == '/') {
      size_t length = strlen(pattern);
      if (pattern[length - 1] == '/') {
	 bool res = false;
	 regex_t preg;
	 char *regex = strdup(pattern + 1);
	 regex[length - 2] = '\0';
	 if (regcomp(&preg, regex, REG_EXTENDED | REG_ICASE) != 0) {
	    _error->Warning("Invalid regular expression: %s", regex);
	 } else if (regexec(&preg, string, 0, NULL, 0) == 0) {
	    res = true;
	 }
	 free(regex);
	 regfree(&preg);
	 return res;
      }
   }
   return fnmatch(pattern, string, FNM_CASEFOLD) == 0;
}
bool pkgVersionMatch::ExpressionMatches(const std::string& pattern, const char *string)
{
    return ExpressionMatches(pattern.c_str(), string);
}
									/*}}}*/
// VersionMatch::FileMatch - Match against an index file		/*{{{*/
// ---------------------------------------------------------------------
/* This matcher checks against the release file and the origin location
   to see if the constraints are met. */
bool pkgVersionMatch::FileMatch(pkgCache::RlsFileIterator const &File)
{
   if (Type == Release)
   {
      if (MatchAll)
	 return true;

      if (RelVerStr.empty() && RelOrigin.empty() &&
	  RelArchive.empty() && RelLabel.empty() &&
	  RelRelease.empty() && RelCodename.empty() &&
	  RelComponent.empty() && RelArchitecture.empty())
	 return false;

      if (not RelVerStr.empty() && not MatchVer(File.Version(), RelVerStr, RelVerPrefixMatch) &&
	  not ExpressionMatches(RelVerStr, File.Version()))
	 return false;
      if (not RelOrigin.empty() && not ExpressionMatches(RelOrigin, File.Origin()))
	 return false;
      if (not RelArchive.empty() && not ExpressionMatches(RelArchive, File.Archive()))
	 return false;
      if (not RelCodename.empty() && not ExpressionMatches(RelCodename, File.Codename()))
	 return false;
      if (not RelRelease.empty() && not ExpressionMatches(RelRelease, File.Archive()) &&
	  not ExpressionMatches(RelRelease, File.Codename()))
	 return false;
      if (not RelLabel.empty() && not ExpressionMatches(RelLabel, File.Label()))
	 return false;
      return true;
   }

   if (Type == Origin)
   {
      if (not OrSite.empty() && File.Site() == nullptr)
	 return false;
      return ExpressionMatches(OrSite, File.Site()); /* both strings match */
   }

   return false;
}
bool pkgVersionMatch::FileMatch(pkgCache::PkgFileIterator File)
{
   if (auto const RlsFile = File.ReleaseFile(); not RlsFile.end())
   {
      if (not FileMatch(RlsFile))
	 return false;
   }
   else if (Type == Release)
   {
      // only 'bad' files like dpkg.status file have no release file
      // those reuse the Component of te PkgFile to store the Archive "now".
      if (not RelArchive.empty() && not ExpressionMatches(RelArchive, File.Component()))
	 return false;
      if (not RelRelease.empty() && not ExpressionMatches(RelRelease, File.Component()))
	 return false;
      if (not RelOrigin.empty() || not RelLabel.empty() ||
	  not RelVerStr.empty() || not RelCodename.empty())
	 return false;
   }
   else
      return false;

   if (Type == Release)
   {
      if (MatchAll)
	 return true;

      if (not RelComponent.empty() && not ExpressionMatches(RelComponent, File.Component()))
	 return false;
      if (not RelArchitecture.empty() && not ExpressionMatches(RelArchitecture, File.Architecture()))
	 return false;
   }

   return true;
}
									/*}}}*/
