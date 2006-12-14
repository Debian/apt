// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.h,v 1.12.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   SourceList - Manage a list of sources
   
   The Source List class provides access to a list of sources. It 
   can read them from a file and generate a list of all the distinct
   sources.
   
   All sources have a type associated with them that defines the layout
   of the archive. The exact format of the file is documented in
   files.sgml.

   The types are mapped through a list of type definitions which handle
   the actual construction of the back end type. After loading a source 
   list all you have is a list of package index files that have the ability
   to be Acquired.
   
   The vendor machanism is similar, except the vendor types are hard 
   wired. Before loading the source list the vendor list is loaded.
   This doesn't load key data, just the checks to perform.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SOURCELIST_H
#define PKGLIB_SOURCELIST_H

#include <string>
#include <vector>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/metaindex.h>

using std::string;
using std::vector;
    

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
      virtual bool ParseLine(vector<metaIndex *> &List,
			     const char *Buffer,
			     unsigned long CurLine,string File) const;
      virtual bool CreateItem(vector<metaIndex *> &List,string URI,
			      string Dist,string Section) const = 0;
      Type();
      virtual ~Type() {};
   };
   
   typedef vector<metaIndex *>::const_iterator const_iterator;
   
   protected:

   vector<metaIndex *> SrcList;
   
   public:

   bool ReadMainList();
   bool Read(string File);

   // CNC:2003-03-03
   void Reset();
   bool ReadAppend(string File);
   bool ReadSourceDir(string Dir);
   
   // List accessors
   inline const_iterator begin() const {return SrcList.begin();};
   inline const_iterator end() const {return SrcList.end();};
   inline unsigned int size() const {return SrcList.size();};
   inline bool empty() const {return SrcList.empty();};

   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const;
   bool GetIndexes(pkgAcquire *Owner, bool GetAll=false) const;
   
   pkgSourceList();
   pkgSourceList(string File);
   ~pkgSourceList();      
};

#endif
