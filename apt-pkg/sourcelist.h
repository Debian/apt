// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.h,v 1.2 1998/07/09 05:12:31 jgg Exp $
/* ######################################################################

   SourceList - Manage a list of sources
   
   The Source List class provides access to a list of sources. It 
   can read them from a file and generate a list of all the permutations.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_SOURCELIST_H
#define PKGLIB_SOURCELIST_H

#include <string>
#include <vector>
#include <iostream.h>
#include <pkglib/pkgcache.h>

#ifdef __GNUG__
#pragma interface "pkglib/sourcelist.h"
#endif

class pkgAquire;
class pkgSourceList
{
   public:
   
   /* Each item in the source list, each line can have more than one
      item */
   struct Item
   {
      enum {Deb} Type;

      string URI;
      string Dist;
      string Section;
      
      bool SetType(string S);
      bool SetURI(string S);
      string PackagesURI() const;
      string PackagesInfo() const;      
      string SiteOnly(string URI) const;
      string ArchiveInfo(pkgCache::VerIterator Ver) const;
      string ArchiveURI(string File) const;
   };
   typedef vector<Item>::const_iterator const_iterator;
   
   protected:
   
   vector<Item> List;
   
   public:

   bool ReadMainList();
   bool Read(string File);
   string SanitizeURI(string URI);
   const_iterator MatchPkgFile(pkgCache::VerIterator Ver);
   
   // List accessors
   inline const_iterator begin() const {return List.begin();};
   inline const_iterator end() const {return List.end();};
   inline unsigned int size() const {return List.size();};
   inline bool empty() const {return List.empty();};
   
   pkgSourceList();
   pkgSourceList(string File);   
};

ostream &operator <<(ostream &O,pkgSourceList::Item &Itm);

#endif
