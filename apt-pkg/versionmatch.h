// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: versionmatch.h,v 1.4 2001/05/29 03:07:12 jgg Exp $
/* ######################################################################

   Version Matching 
   
   This module takes a matching string and a type and locates the version
   record that satisfies the constraint described by the matching string.

     Version: 1.2*
     Release: o=Debian,v=2.1*,c=main
     Release: v=2.1*
     Release: *
     Origin: ftp.debian.org
   
   Release may be a complex type that can specify matches for any of:
      Version (v= with prefix)
      Origin (o=)
      Archive (a=)
      Label (l=)
      Component (c=)
   If there are no equals signs in the string then it is scanned in short
   form - if it starts with a number it is Version otherwise it is an 
   Archive.
   
   Release may be a '*' to match all releases.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_VERSIONMATCH_H
#define PKGLIB_VERSIONMATCH_H


#include <string>
#include <apt-pkg/pkgcache.h>

using std::string;

class pkgVersionMatch
{   
   // Version Matching
   string VerStr;
   bool VerPrefixMatch;

   // Release Matching
   string RelVerStr;
   bool RelVerPrefixMatch;
   string RelOrigin;
   string RelArchive;
   string RelLabel;
   string RelComponent;
   bool MatchAll;
   
   // Origin Matching
   string OrSite;
   
   public:
   
   enum MatchType {None = 0,Version,Release,Origin} Type;
   
   bool MatchVer(const char *A,string B,bool Prefix);
   bool FileMatch(pkgCache::PkgFileIterator File);
   pkgCache::VerIterator Find(pkgCache::PkgIterator Pkg);
			       
   pkgVersionMatch(string Data,MatchType Type);
};

#endif
