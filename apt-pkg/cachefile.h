// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cachefile.h,v 1.1 1999/04/18 06:36:36 jgg Exp $
/* ######################################################################
   
   CacheFile - Simple wrapper class for opening, generating and whatnot
   
   This class implements a simple 2 line mechanism to open various sorts
   of caches. It can operate as root, as not root, show progress and so on,
   it transparently handles everything necessary.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CACHEFILE_H
#define PKGLIB_CACHEFILE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/cachefile.h"
#endif 

#include <apt-pkg/depcache.h>
#include <apt-pkg/dpkginit.h>

class pkgCacheFile
{
   protected:
   
   MMap *Map;
   pkgDepCache *Cache;
   pkgDpkgLock *Lock;
   
   public:
      
   // We look pretty much exactly like a pointer to a dep cache
   inline operator pkgDepCache &() {return *Cache;};
   inline pkgDepCache *operator ->() {return Cache;};
   inline pkgDepCache &operator *() {return *Cache;};

   // Release the dpkg status lock
   inline void ReleaseLock() {Lock->Close();};
   
   bool Open(OpProgress &Progress,bool WithLock = true);
   
   pkgCacheFile();
   ~pkgCacheFile();
};

#endif
