// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version Matching

   This module takes a matching string and a type and locates the version
   record that satisfies the constraint described by the matching string.

     Version: 1.2*
     Release: o=Debian,v=2.1*,c=main
     Release: v=2.1*
     Release: a=testing
     Release: n=squeeze
     Release: *
     Origin: ftp.debian.org

   Release may be a complex type that can specify matches for any of:
      Version (v= with prefix)
      Origin (o=)
      Archive (a=) eg, unstable, testing, stable
      Codename (n=) e.g. etch, lenny, squeeze, sid
      Label (l=)
      Component (c=)
      Binary Architecture (b=)
   If there are no equals signs in the string then it is scanned in short
   form - if it starts with a number it is Version otherwise it is an
   Archive or a Codename.

   Release may be a '*' to match all releases.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_VERSIONMATCH_H
#define PKGLIB_VERSIONMATCH_H

#include <apt-pkg/pkgcache.h>

#include <string>


class APT_PUBLIC pkgVersionMatch
{
   // Version Matching
   std::string VerStr;
   bool VerPrefixMatch;

   // Release Matching
   std::string RelVerStr;
   bool RelVerPrefixMatch;
   std::string RelOrigin;
   std::string RelRelease;
   std::string RelCodename;
   std::string RelArchive;
   std::string RelLabel;
   std::string RelComponent;
   std::string RelArchitecture;
   bool MatchAll;

   // Origin Matching
   std::string OrSite;

   public:

   enum MatchType {None = 0,Version,Release,Origin} Type;

   bool MatchVer(const char *A,std::string B,bool Prefix) APT_PURE;
   static bool ExpressionMatches(const char *pattern, const char *string);
   static bool ExpressionMatches(const std::string& pattern, const char *string);
   bool FileMatch(pkgCache::PkgFileIterator File);
   bool FileMatch(pkgCache::RlsFileIterator const &File);
   pkgCache::VerIterator Find(pkgCache::PkgIterator Pkg);
   bool VersionMatches(pkgCache::VerIterator Ver);

   pkgVersionMatch(std::string Data,MatchType Type);
};

#endif
