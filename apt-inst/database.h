// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: database.h,v 1.2 2001/02/20 07:03:16 jgg Exp $
/* ######################################################################

   Data Base Abstraction
   
   This class provides a simple interface to an abstract notion of a 
   database directory for storing state information about the system.

   The 'Meta' information for a package is the control information and
   setup scripts stored inside the archive. GetMetaTmp returns the name of
   a directory that is used to store named files containing the control
   information. 
   
   The File Listing is the database of installed files. It is loaded 
   into the memory/persistent cache structure by the ReadFileList method.  
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DATABASE_H
#define PKGLIB_DATABASE_H

#include <apt-pkg/filelist.h>
#include <apt-pkg/pkgcachegen.h>

class pkgDataBase
{
   protected:
   
   pkgCacheGenerator *Cache;
   pkgFLCache *FList;
   string MetaDir;
   virtual bool InitMetaTmp(string &Dir) = 0;
   
   public:

   // Some manipulators for the cache and generator
   inline pkgCache &GetCache() {return Cache->GetCache();};
   inline pkgFLCache &GetFLCache() {return *FList;};
   inline pkgCacheGenerator &GetGenerator() {return *Cache;};
   
   bool GetMetaTmp(string &Dir);
   virtual bool ReadyFileList(OpProgress &Progress) = 0;
   virtual bool ReadyPkgCache(OpProgress &Progress) = 0;
   virtual bool LoadChanges() = 0;

   pkgDataBase() : Cache(0), FList(0) {};
   virtual ~pkgDataBase() {delete Cache; delete FList;};
};

#endif
