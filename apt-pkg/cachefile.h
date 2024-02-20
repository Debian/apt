// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   CacheFile - Simple wrapper class for opening, generating and whatnot
   
   This class implements a simple 2 line mechanism to open various sorts
   of caches. It can operate as root, as not root, show progress and so on,
   it transparently handles everything necessary.
   
   This means it can rebuild caches from the source list and instantiates
   and prepares the standard policy mechanism.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CACHEFILE_H
#define PKGLIB_CACHEFILE_H

#include <cstddef>

#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>


class MMap;
class pkgPolicy;
class pkgSourceList;
class pkgIndexFile;
class OpProgress;

class APT_PUBLIC pkgCacheFile
{
   struct Private;
   /** \brief dpointer placeholder (for later in case we need it) */
   Private *const d;
   bool ExternOwner;

   protected:
   MMap *Map;
   pkgCache *Cache;
   pkgDepCache *DCache;
   pkgSourceList *SrcList;

   public:
   pkgPolicy *Policy;

   // We look pretty much exactly like a pointer to a dep cache
   inline operator pkgCache &() const {return *Cache;};
   inline operator pkgCache *() const {return Cache;};
   inline operator pkgDepCache &() const {return *DCache;};
   inline operator pkgDepCache *() const {return DCache;};
   inline operator pkgPolicy &() const {return *Policy;};
   inline operator pkgPolicy *() const {return Policy;};
   inline operator pkgSourceList &() const {return *SrcList;};
   inline operator pkgSourceList *() const {return SrcList;};
   inline pkgDepCache *operator ->() const {return DCache;};
   inline pkgDepCache &operator *() const {return *DCache;};
   inline pkgDepCache::StateCache &operator [](pkgCache::PkgIterator const &I) const {return (*DCache)[I];};
   inline unsigned char &operator [](pkgCache::DepIterator const &I) const {return (*DCache)[I];};

   bool BuildCaches(OpProgress *Progress = NULL,bool WithLock = true);
   bool BuildSourceList(OpProgress *Progress = NULL);
   bool BuildPolicy(OpProgress *Progress = NULL);
   bool BuildDepCache(OpProgress *Progress = NULL);
   bool Open(OpProgress *Progress = NULL, bool WithLock = true);
   inline bool ReadOnlyOpen(OpProgress *Progress = NULL) { return Open(Progress, false); };
   static void RemoveCaches();
   void Close();

   bool AddIndexFile(pkgIndexFile * const File);
   // Starts DepCache with a claim of one ActionGroup already active
   void InhibitActionGroups(bool yes);

   inline pkgCache* GetPkgCache() { BuildCaches(NULL, false); return Cache; };
   inline pkgDepCache* GetDepCache() { BuildDepCache(); return DCache; };
   inline pkgPolicy* GetPolicy() { BuildPolicy(); return Policy; };
   inline pkgSourceList* GetSourceList() { BuildSourceList(); return SrcList; };

   inline bool IsPkgCacheBuilt() const { return (Cache != NULL); };
   inline bool IsDepCacheBuilt() const { return (DCache != NULL); };
   inline bool IsPolicyBuilt() const { return (Policy != NULL); };
   inline bool IsSrcListBuilt() const { return (SrcList != NULL); };

   pkgCacheFile();
   explicit pkgCacheFile(pkgDepCache * const Owner);
   virtual ~pkgCacheFile();
};

#endif
