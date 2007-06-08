// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgdb.h,v 1.2 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   DPKGv1 Data Base Implemenation
   
   The DPKGv1 database is typically stored in /var/lib/dpkg/. For 
   DPKGv1 the 'meta' information is the contents of the .deb control.tar.gz
   member prepended by the package name. The meta information is unpacked
   in its temporary directory and then migrated into the main list dir
   at a checkpoint.
   
   Journaling is providing by syncronized file writes to the updates sub
   directory.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DPKGDB_H
#define PKGLIB_DPKGDB_H


#include <apt-pkg/database.h>

class debDpkgDB : public pkgDataBase
{
   protected:
   
   string AdminDir;
   DynamicMMap *CacheMap;
   DynamicMMap *FileMap;
   unsigned long DiverInode;
   signed long DiverTime;
   
   virtual bool InitMetaTmp(string &Dir);
   bool ReadFList(OpProgress &Progress);
   bool ReadDiversions();
   bool ReadConfFiles();
      
   public:

   virtual bool ReadyFileList(OpProgress &Progress);
   virtual bool ReadyPkgCache(OpProgress &Progress);
   virtual bool LoadChanges();
   
   debDpkgDB();
   virtual ~debDpkgDB();
};

#endif
