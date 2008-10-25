// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: versionmatch.cc,v 1.9 2003/05/19 17:58:26 doogie Exp $
/* ######################################################################

   Version Matching 
   
   This module takes a matching string and a type and locates the version
   record that satisfies the constraint described by the matching string.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/versionmatch.h>

#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <stdio.h>
#include <ctype.h>
									/*}}}*/

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
      for (; I != Data.end() && *I != '='; I++);
      if (I == Data.end())
      {
	 // Temporary
	 if (isdigit(Data[0]))
	    RelVerStr = Data;
	 else
	    RelArchive = Data;
	 
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
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"l=") == 0)
	    RelLabel = Fragments[J]+2;
	 else if (stringcasecmp(Fragments[J],Fragments[J]+2,"c=") == 0)
	    RelComponent = Fragments[J]+2;
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
   pkgCache::VerIterator Ver = Pkg.VersionList();
   for (; Ver.end() == false; Ver++)
   {
      if (Type == Version)
      {
	 if (MatchVer(Ver.VerStr(),VerStr,VerPrefixMatch) == true)
	    return Ver;
	 continue;
      }
      
      for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; VF++)
	 if (FileMatch(VF.File()) == true)
	    return Ver;
   }
      
   // This will be Ended by now.
   return Ver;
}
									/*}}}*/
// VersionMatch::FileMatch - Match against an index file		/*{{{*/
// ---------------------------------------------------------------------
/* This matcher checks against the release file and the origin location 
   to see if the constraints are met. */
bool pkgVersionMatch::FileMatch(pkgCache::PkgFileIterator File)
{
   if (Type == Release)
   {
      if (MatchAll == true)
	 return true;
      
/*      cout << RelVerStr << ',' << RelOrigin << ',' << RelArchive << ',' << RelLabel << endl;
      cout << File.Version() << ',' << File.Origin() << ',' << File.Archive() << ',' << File.Label() << endl;*/
      
      if (RelVerStr.empty() == true && RelOrigin.empty() == true &&
	  RelArchive.empty() == true && RelLabel.empty() == true &&
	  RelComponent.empty() == true)
	 return false;
      
      if (RelVerStr.empty() == false)
	 if (File->Version == 0 ||
	     MatchVer(File.Version(),RelVerStr,RelVerPrefixMatch) == false)
	    return false;
      if (RelOrigin.empty() == false)
	 if (File->Origin == 0 ||
	     stringcasecmp(RelOrigin,File.Origin()) != 0)
	    return false;
      if (RelArchive.empty() == false)
      {
	 if (File->Archive == 0 || 
	     stringcasecmp(RelArchive,File.Archive()) != 0)
	    return false;
      }      
      if (RelLabel.empty() == false)
	 if (File->Label == 0 ||
	     stringcasecmp(RelLabel,File.Label()) != 0)
	    return false;
      if (RelComponent.empty() == false)
	 if (File->Component == 0 ||
	     stringcasecmp(RelComponent,File.Component()) != 0)
	    return false;
      return true;
   }
   
   if (Type == Origin)
   {
      if (OrSite.empty() == false) {
	 if (File->Site == 0 || OrSite != File.Site())
	    return false;
      } else // so we are talking about file:// or status file
	 if (strcmp(File.Site(),"") == 0 && File->Archive != 0) // skip the status file
	    return false;
      return (OrSite == File.Site());		/* both strings match */
   }
   
   return false;
}
									/*}}}*/
