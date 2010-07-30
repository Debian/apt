// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: packagemanager.h,v 1.14 2001/05/07 04:24:08 jgg Exp $
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


#include <string>
#include <iostream>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/depcache.h>
#include <set>

using std::string;

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

   /** \brief saves packages dpkg let disappear

       This way APT can retreat from trying to configure these
       packages later on and a frontend can choose to display a
       notice to inform the user about these disappears.
   */
   std::set<std::string> disappearedPkgs;

   bool DepAdd(pkgOrderList &Order,PkgIterator P,int Depth = 0);
   void ImmediateAdd(PkgIterator P, bool UseInstallVer, unsigned const int &Depth = 0);
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
   virtual bool Go(int statusFd=-1) {return true;};
   virtual void Reset() {};

   // the result of the operation
   OrderResult Res;

   public:
      
   // Main action members
   bool GetArchives(pkgAcquire *Owner,pkgSourceList *Sources,
		    pkgRecords *Recs);

   // Do the installation 
   OrderResult DoInstall(int statusFd=-1);

   // stuff that needs to be done before the fork() of a library that
   // uses apt
   OrderResult DoInstallPreFork() {
      Res = OrderInstall();
      return Res;
   };

   // stuff that needs to be done after the fork
   OrderResult DoInstallPostFork(int statusFd=-1);
   bool FixMissing();

   /** \brief returns all packages dpkg let disappear */
   inline std::set<std::string> GetDisappearedPackages() { return disappearedPkgs; };

   pkgPackageManager(pkgDepCache *Cache);
   virtual ~pkgPackageManager();
};

#endif
