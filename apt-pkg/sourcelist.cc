// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.cc,v 1.1 1998/07/07 04:17:06 jgg Exp $
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "pkglib/sourcelist.h"
#endif

#include <pkglib/sourcelist.h>
#include <pkglib/error.h>
#include <pkglib/fileutl.h>
#include <strutl.h>
#include <options.h>

#include <fstream.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
									/*}}}*/

// SourceList::pkgSourceList - Constructors				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::pkgSourceList()
{
}

pkgSourceList::pkgSourceList(string File)
{
   Read(File);
}
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   return Read(PKG_DEB_CF_SOURCELIST);
}
									/*}}}*/
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string File)
{
   // Open the stream for reading
   ifstream F(File.c_str(),ios::in | ios::nocreate);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream","Opening %s",File.c_str());
   
   List.erase(List.begin(),List.end());
   char Buffer[300];

   int CurLine = 0;
   while (F.eof() == false)
   {
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      _strtabexpand(Buffer,sizeof(Buffer));
      _strstrip(Buffer);
      
      // Comment or blank
      if (Buffer[0] == '#' || Buffer[0] == 0)
	 continue;
      
      // Grok it
      string Type;
      string URI;
      Item Itm;
      char *C = Buffer;
      if (ParseQuoteWord(C,Type) == false)
	 return _error->Error("Malformed line %u in source list %s (type)",CurLine,File.c_str());
      if (ParseQuoteWord(C,URI) == false)
	 return _error->Error("Malformed line %u in source list %s (URI)",CurLine,File.c_str());
      if (ParseQuoteWord(C,Itm.Dist) == false)
	 return _error->Error("Malformed line %u in source list %s (dist)",CurLine,File.c_str());
      if (Itm.SetType(Type) == false)
	 return _error->Error("Malformed line %u in source list %s (type parse)",CurLine,File.c_str());
      if (Itm.SetURI(URI) == false)
	 return _error->Error("Malformed line %u in source list %s (URI parse)",CurLine,File.c_str());

      // Check for an absolute dists specification.
      if (Itm.Dist.empty() == false && Itm.Dist[Itm.Dist.size() - 1] == '/')
      {
	 if (ParseQuoteWord(C,Itm.Section) == true)
	    return _error->Error("Malformed line %u in source list %s (Absolute dist)",CurLine,File.c_str());
	 Itm.Dist = SubstVar(Itm.Dist,"$(ARCH)",PKG_DEB_ARCH);
	 List.push_back(Itm);
	 continue;
      }

      // Grab the rest of the dists
      if (ParseQuoteWord(C,Itm.Section) == false)
	    return _error->Error("Malformed line %u in source list %s (dist parse)",CurLine,File.c_str());
      
      do
      {
	 List.push_back(Itm);
      }
      while (ParseQuoteWord(C,Itm.Section) == true);
   }
   return true;
}
									/*}}}*/
// SourceList::SanitizeURI - Hash the uri				/*{{{*/
// ---------------------------------------------------------------------
/* This converts a URI into a safe filename. It quotes all unsafe characters
   and converts / to _ and removes the scheme identifier. */
string pkgSourceList::SanitizeURI(string URI)
{
   string::const_iterator I = URI.begin() + URI.find(':') + 1;
   for (; I < URI.end() && *I == '/'; I++);

   // "\x00-\x20{}|\\\\^\\[\\]<>\"\x7F-\xFF";
   URI = QuoteString(string(I,URI.end() - I),"\\|{}[]<>\"^~_=!@#$%^&*");
   string::iterator J = URI.begin();
   for (; J != URI.end(); J++)
      if (*J == '/') 
	 *J = '_';
   return URI;
}
									/*}}}*/
// SourceList::MatchPkgFile - Find the package file that has the ver	/*{{{*/
// ---------------------------------------------------------------------
/* This will return List.end() if it could not find the matching 
   file */
pkgSourceList::const_iterator pkgSourceList::MatchPkgFile(pkgCache::VerIterator Ver)
{
   string Base = PKG_DEB_ST_LIST;
   for (const_iterator I = List.begin(); I != List.end(); I++)
   {
      string URI = I->PackagesURI();
      switch (I->Type)
      {
	 case Item::Deb:
	 if (Base + SanitizeURI(URI) == Ver.File().FileName())
	    return I;
	 break;
      };      
   }
   return List.end();
}
									/*}}}*/
// SourceList::Item << - Writes the item to a stream			/*{{{*/
// ---------------------------------------------------------------------
/* This is not suitable for rebuilding the sourcelist file but it good for
   debugging. */
ostream &operator <<(ostream &O,pkgSourceList::Item &Itm)
{
   O << Itm.Type << ' ' << Itm.URI << ' ' << Itm.Dist << ' ' << Itm.Section;
   return O;
}
									/*}}}*/
// SourceList::Item::SetType - Sets the distribution type		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Item::SetType(string S)
{
   if (S == "deb")
   {
      Type = Deb;
      return true;
   }

   return true;
}
									/*}}}*/
// SourceList::Item::SetURI - Set the URI				/*{{{*/
// ---------------------------------------------------------------------
/* For simplicity we strip the scheme off the uri */
bool pkgSourceList::Item::SetURI(string S)
{
   if (S.empty() == true)
      return false;

   if (S.find(':') == string::npos)
      return false;

   S = SubstVar(S,"$(ARCH)",PKG_DEB_ARCH);
   
   // Make sure that the URN is / postfixed
   URI = S;
   if (URI[URI.size() - 1] != '/')
      URI += '/';
   
   return true;
}
									/*}}}*/
// SourceList::Item::PackagesURI - Returns a URI to the packages file	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::PackagesURI() const
{
   string Res;
   switch (Type)
   {
      case Deb:
      if (Dist[Dist.size() - 1] == '/')
	 Res = URI + Dist;
      else
	 Res = URI + "dists/" + Dist + '/' + Section + 
	 "/binary-" + PKG_DEB_ARCH + '/';
      
      Res += "Packages";
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::PackagesInfo - Shorter version of the URI		/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string pkgSourceList::Item::PackagesInfo() const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res += SiteOnly(URI) + ' ';
      if (Dist[Dist.size() - 1] == '/')
	 Res += Dist;
      else
	 Res += Dist + '/' + Section;
      
      Res += " Packages";
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::ArchiveInfo - Shorter version of the archive spec	/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string pkgSourceList::Item::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res += SiteOnly(URI) + ' ';
      if (Dist[Dist.size() - 1] == '/')
	 Res += Dist;
      else
	 Res += Dist + '/' + Section;
      
      Res += " ";
      Res += Ver.ParentPkg().Name();
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::ArchiveURI - Returns a URI to the given archive	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::ArchiveURI(string File) const
{
   string Res;
   switch (Type)
   {
      case Deb:
      Res = URI + File;
      break;
   };
   return Res;
}
									/*}}}*/
// SourceList::Item::SiteOnly - Strip off the path part of a URI	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgSourceList::Item::SiteOnly(string URI) const
{
   unsigned int Pos = URI.find(':');
   if (Pos == string::npos || Pos + 3 > URI.length())
      return URI;
   if (URI[Pos + 1] != '/' || URI[Pos + 2] != '/')
      return URI;

   Pos = URI.find('/',Pos + 3);
   if (Pos == string::npos)
      return URI;
   return string(URI,0,Pos);
}
									/*}}}*/

// UpdateMeta - Update the meta information				/*{{{*/
// ---------------------------------------------------------------------
/* The meta information is package files, revision information and mirror
   lists. */
bool pkgUpdateMeta(pkgSourceList &List,pkgAquire &Engine)
{
   if (Engine.OutputDir(PKG_DEB_ST_LIST) == false)
      return false;
   
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      string URI = I->PackagesURI();
      string GetInfo = I->PackagesInfo();
      switch (I->Type)
      {
	 case pkgSourceList::Item::Deb:
	    if (Engine.Get(URI + ".gz",List.SanitizeURI(URI),GetInfo) == false)
	       return false;
	 break;
      };      
   }
   
   return true;
}
									/*}}}*/
// MakeSrcCache - Generate a cache file of all the package files	/*{{{*/
// ---------------------------------------------------------------------
/* This goes over the source list and builds a cache of all the package
   files. */
bool pkgMakeSrcCache(pkgSourceList &List)
{
   // First we date check the cache
   bool Bad = false;
   while (Bad == false)
   {
      if (FileExists(PKG_DEB_CA_SRCCACHE) == false)
	  break;
	  
      pkgCache Cache(PKG_DEB_CA_SRCCACHE,true,true);
      if (_error->PendingError() == true)
      {
	 _error->Discard();
	 break;
      }
      
      // They are certianly out of sync
      if (Cache.Head().PackageFileCount != List.size())
	  break;
      
      for (pkgCache::PkgFileIterator F(Cache); F.end() == false; F++)
      {
	 // Search for a match in the source list
	 Bad = true;
	 for (pkgSourceList::const_iterator I = List.begin(); 
	      I != List.end(); I++)
	 {
	    string File = string(PKG_DEB_ST_LIST) + 
	       List.SanitizeURI(I->PackagesURI());
	    if (F.FileName() == File)
	    {
	       Bad = false;
	       break;
	    }
	 }
	 
	 // Check if the file matches what was cached
	 Bad |= !F.IsOk();
	 if (Bad == true)
	    break;
      }      

      if (Bad == false)
	 return true;
   }
   
   unlink(PKG_DEB_CA_SRCCACHE);
   pkgCache::MergeState Merge(PKG_DEB_CA_SRCCACHE);
   if (_error->PendingError() == true)
            return false;
   
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      string File = string(PKG_DEB_ST_LIST) + List.SanitizeURI(I->PackagesURI());
      if (Merge.MergePackageFile(File,"??","??") == false)
	 return false;
   }
          
   return true;
}
									/*}}}*/
// MakeStatusCache - Generates a cache that includes the status files	/*{{{*/
// ---------------------------------------------------------------------
/* This copies the package source cache and then merges the status and 
   xstatus files into it. */
bool pkgMakeStatusCache()
{
   // Quickly check if the existing package cache is ok
   bool Bad = false;
   while (Bad == false)
   {
      if (FileExists(PKG_DEB_CA_PKGCACHE) == false)
	  break;
      
      /* We check the dates of the two caches. This takes care of most things
         quickly and easially */
      struct stat Src;
      struct stat Pkg;
      if (stat(PKG_DEB_CA_PKGCACHE,&Pkg) != 0 || 
	  stat(PKG_DEB_CA_SRCCACHE,&Src) != 0)
	 break;
      if (difftime(Src.st_mtime,Pkg.st_mtime) > 0)
	 break;

      pkgCache Cache(PKG_DEB_CA_PKGCACHE,true,true);
      if (_error->PendingError() == true)
      {
	 _error->Discard();
	 break;
      }
      
      for (pkgCache::PkgFileIterator F(Cache); F.end() == false; F++)
      {
	 if (F.IsOk() == false)
	 {
	    Bad = true;
	    break;
	 }
      }
      
      if (Bad == false)
	 return true;
   }   

   // Check the integrity of the source cache.
   {
      pkgCache Cache(PKG_DEB_CA_SRCCACHE,true,true);
      if (_error->PendingError() == true)
	 return false;
   }
   
   // Sub scope so that merge destructs before we rename the file...
   string Cache = PKG_DEB_CA_PKGCACHE ".new";
   {
      if (CopyFile(PKG_DEB_CA_SRCCACHE,Cache) == false)
	 return false;

      pkgCache::MergeState Merge(Cache);
      if (_error->PendingError() == true)
	 return false;
      
      // Merge in the user status file
      if (FileExists(PKG_DEB_ST_USERSTATUS) == true)
	 if (Merge.MergePackageFile(PKG_DEB_ST_USERSTATUS,"status","0",
				    pkgFLAG_NotSource) == false)
	    return false;
      
      // Merge in the extra status file
      if (FileExists(PKG_DEB_ST_XSTATUS) == true)
	 if (Merge.MergePackageFile(PKG_DEB_ST_XSTATUS,"status","0",
				    pkgFLAG_NotSource) == false)
	    return false;
      
      // Merge in the status file
      if (Merge.MergePackageFile("/var/lib/dpkg/status","status","0",
				 pkgFLAG_NotSource) == false)
	 return false;
   }
   
   if (rename(Cache.c_str(),PKG_DEB_CA_PKGCACHE) != 0)
      return false;
   
   return true;
}
									/*}}}*/
