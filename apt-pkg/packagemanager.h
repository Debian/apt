// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: packagemanager.h,v 1.13 2001/04/06 05:31:01 jgg Exp $
/* ######################################################################

   Package Manager - Abstacts the package manager

   Three steps are 
     - Aquiration of archives (stores the list of final file names)
     - Sorting of operations
     - Invokation of package manager
   
   This is the final stage when the package cache entities get converted
   into file names and the state stored in a DepCache is transformed
   into a series of operations.

   In the final scheme of things this may serve as a director class to
   access the actual install methods based on the file type being
   installed.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PACKAGEMANAGER_H
#define PKGLIB_PACKAGEMANAGER_H

#ifdef __GNUG__
#pragma interface "apt-pkg/packagemanager.h"
#endif

#include <string>
#include <apt-pkg/pkgcache.h>

class pkgAcquire;
class pkgDepCache;
class pkgSourceList;
class pkgOrderList;
class pkgRecords;
class pkgPackageManager : protected pkgCache::Namespace
{
   public:
   
   enum OrderResult {Completed,Failed,Incomplete};
   
   protected:
   string *FileNames;
   pkgDepCache &Cache;
   pkgOrderList *List;
   bool Debug;
         
   bool DepAdd(pkgOrderList &Order,PkgIterator P,int Depth = 0);
   virtual OrderResult OrderInstall();
   bool CheckRConflicts(PkgIterator Pkg,DepIterator Dep,const char *Ver);
   bool CreateOrderList();
   
   // Analysis helpers
   bool DepAlwaysTrue(DepIterator D);
   
   // Install helpers
   bool ConfigureAll();
   bool SmartConfigure(PkgIterator Pkg);
   bool SmartUnPack(PkgIterator Pkg);
   bool SmartRemove(PkgIterator Pkg);
   bool EarlyRemove(PkgIterator Pkg);   
   
   // The Actual installation implementation
   virtual bool Install(PkgIterator /*Pkg*/,string /*File*/) {return false;};
   virtual bool Configure(PkgIterator /*Pkg*/) {return false;};
   virtual bool Remove(PkgIterator /*Pkg*/,bool /*Purge*/=false) {return false;};
   virtual bool Go() {return true;};
   virtual void Reset() {};
   
   public:
      
   // Main action members
   bool GetArchives(pkgAcquire *Owner,pkgSourceList *Sources,
		    pkgRecords *Recs);
   OrderResult DoInstall();
   bool FixMissing();
   
   pkgPackageManager(pkgDepCache *Cache);
   virtual ~pkgPackageManager();
};

#endif
