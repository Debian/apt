// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.h,v 1.12 2002/07/01 21:41:11 jgg Exp $
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
   This doesn't load key data, just the checks to preform.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SOURCELIST_H
#define PKGLIB_SOURCELIST_H

#include <string>
#include <vector>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/indexfile.h>

using std::string;
using std::vector;
    
#ifdef __GNUG__
#pragma interface "apt-pkg/sourcelist.h"
#endif

class pkgAquire;
class pkgSourceList
{
   public:
   
   // An available vendor
   struct Vendor
   {
      string VendorID;
      string FingerPrint;
      string Description;

      /* Lets revisit these..
      bool MatchFingerPrint(string FingerPrint);
      string FingerPrintDescr();*/
   };
   
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
			     Vendor const *Vendor,
			     const char *Buffer,
			     unsigned long CurLine,string File) const;
      virtual bool CreateItem(vector<pkgIndexFile *> &List,string URI,
			      string Dist,string Section,
			      Vendor const *Vendor) const = 0;

      Type();
      virtual ~Type() {};
   };
   
   typedef vector<pkgIndexFile *>::const_iterator const_iterator;
   
   protected:

   vector<pkgIndexFile *> SrcList;
   vector<Vendor const *> VendorList;
   
   public:

   bool ReadMainList();
   bool Read(string File);
   bool ReadVendors();
   
   // List accessors
   inline const_iterator begin() const {return SrcList.begin();};
   inline const_iterator end() const {return SrcList.end();};
   inline unsigned int size() const {return SrcList.size();};
   inline bool empty() const {return SrcList.empty();};

   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const;
   bool GetIndexes(pkgAcquire *Owner) const;
   
   pkgSourceList();
   pkgSourceList(string File);
   ~pkgSourceList();      
};

#endif
