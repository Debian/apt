// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.h,v 1.9 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   SourceList - Manage a list of sources
   
   The Source List class provides access to a list of sources. It 
   can read them from a file and generate a list of all the distinct
   sources.
   
   All sources have a type associated with them that defines the layout
   of the archive. The exact format of the file is documented in
   files.sgml.

   The types are mapped through a list of type definitions which handle
   the actual construction of the type. After loading a source list all
   you have is a list of package index files that have the ability
   to be Acquired.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SOURCELIST_H
#define PKGLIB_SOURCELIST_H

#include <string>
#include <vector>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/indexfile.h>
    
#ifdef __GNUG__
#pragma interface "apt-pkg/sourcelist.h"
#endif

class pkgAquire;
class pkgSourceList
{
   public:
   
   // List of supported source list types
   class Type
   {
      public:
      
      // Global list of Items supported
      static Type **GlobalList;
      static unsigned long GlobalListLen;
      static Type *GetType(const char *Type);

      const char *Name;
      const char *Label;

      bool FixupURI(string &URI) const;
      virtual bool ParseLine(vector<pkgIndexFile *> &List,
			     const char *Buffer,
			     unsigned long CurLine,string File) const;
      virtual bool CreateItem(vector<pkgIndexFile *> &List,string URI,
			      string Dist,string Section) const = 0;
			      
      Type();
      virtual ~Type() {};
   };
   
   typedef vector<pkgIndexFile *>::const_iterator const_iterator;
   
   protected:
   
   vector<pkgIndexFile *> List;
   
   public:

   bool ReadMainList();
   bool Read(string File);
   
   // List accessors
   inline const_iterator begin() const {return List.begin();};
   inline const_iterator end() const {return List.end();};
   inline unsigned int size() const {return List.size();};
   inline bool empty() const {return List.empty();};

   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const;
   bool GetIndexes(pkgAcquire *Owner) const;
   
   pkgSourceList();
   pkgSourceList(string File);   
};

#endif
