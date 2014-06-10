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

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/init.h>
#include <apt-pkg/macros.h>

#include <string>
#include <set>

#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/install-progress.h>
#include <iostream>
#endif
#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/depcache.h>
using std::string;
#endif

class pkgAcquire;
class pkgDepCache;
class pkgSourceList;
class pkgOrderList;
class pkgRecords;


class pkgPackageManager : protected pkgCache::Namespace
{
   public:
   
   enum OrderResult {Completed,Failed,Incomplete};
   static bool SigINTStop;
   
   protected:
   std::string *FileNames;
   pkgDepCache &Cache;
   pkgOrderList *List;
   bool Debug;
   bool NoImmConfigure;
   bool ImmConfigureAll;

   /** \brief saves packages dpkg let disappear

       This way APT can retreat from trying to configure these
       packages later on and a frontend can choose to display a
       notice to inform the user about these disappears.
   */
   std::set<std::string> disappearedPkgs;

   void ImmediateAdd(PkgIterator P, bool UseInstallVer, unsigned const int &Depth = 0);
   virtual OrderResult OrderInstall();
   bool CheckRConflicts(PkgIterator Pkg,DepIterator Dep,const char *Ver);
   bool CreateOrderList();
   
   // Analysis helpers
   bool DepAlwaysTrue(DepIterator D) APT_PURE;
   
   // Install helpers
   bool ConfigureAll();
   bool SmartConfigure(PkgIterator Pkg, int const Depth) APT_MUSTCHECK;
   //FIXME: merge on abi break
   bool SmartUnPack(PkgIterator Pkg) APT_MUSTCHECK;
   bool SmartUnPack(PkgIterator Pkg, bool const Immediate, int const Depth) APT_MUSTCHECK;
   bool SmartRemove(PkgIterator Pkg) APT_MUSTCHECK;
   bool EarlyRemove(PkgIterator Pkg, DepIterator const * const Dep) APT_MUSTCHECK;
   APT_DEPRECATED bool EarlyRemove(PkgIterator Pkg) APT_MUSTCHECK;

   // The Actual installation implementation
   virtual bool Install(PkgIterator /*Pkg*/,std::string /*File*/) {return false;};
   virtual bool Configure(PkgIterator /*Pkg*/) {return false;};
   virtual bool Remove(PkgIterator /*Pkg*/,bool /*Purge*/=false) {return false;};
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   virtual bool Go(APT::Progress::PackageManager * /*progress*/) {return true;};
#else
   virtual bool Go(int /*statusFd*/=-1) {return true;};
#endif

   virtual void Reset() {};

   // the result of the operation
   OrderResult Res;

   public:
      
   // Main action members
   bool GetArchives(pkgAcquire *Owner,pkgSourceList *Sources,
		    pkgRecords *Recs);

   // Do the installation 
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   OrderResult DoInstall(APT::Progress::PackageManager *progress);
   // compat
   APT_DEPRECATED OrderResult DoInstall(int statusFd=-1);
#else
   OrderResult DoInstall(int statusFd=-1);
#endif

   // stuff that needs to be done before the fork() of a library that
   // uses apt
   OrderResult DoInstallPreFork() {
      Res = OrderInstall();
      return Res;
   };
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   // stuff that needs to be done after the fork
   OrderResult DoInstallPostFork(APT::Progress::PackageManager *progress);
   // compat
   APT_DEPRECATED OrderResult DoInstallPostFork(int statusFd=-1);
#else
   OrderResult DoInstallPostFork(int statusFd=-1);
#endif

   // ?
   bool FixMissing();

   /** \brief returns all packages dpkg let disappear */
   inline std::set<std::string> GetDisappearedPackages() { return disappearedPkgs; };

   pkgPackageManager(pkgDepCache *Cache);
   virtual ~pkgPackageManager();

   private:
   enum APT_HIDDEN SmartAction { UNPACK_IMMEDIATE, UNPACK, CONFIGURE };
   APT_HIDDEN bool NonLoopingSmart(SmartAction const action, pkgCache::PkgIterator &Pkg,
      pkgCache::PkgIterator DepPkg, int const Depth, bool const PkgLoop,
      bool * const Bad, bool * const Changed) APT_MUSTCHECK;
};

#endif
