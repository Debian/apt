// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Manager - Abstracts the package manager

   Three steps are 
     - Aquiration of archives (stores the list of final file names)
     - Sorting of operations
     - Invocation of package manager
   
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

#include <apt-pkg/edsp.h>
#include <apt-pkg/init.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <set>
#include <string>


class pkgAcquire;
class pkgDepCache;
class pkgSourceList;
class pkgOrderList;
class pkgRecords;
class OpProgress;
class pkgPackageManager;
namespace APT {
   namespace Progress {
      class PackageManager;
   }
}

class APT_PUBLIC pkgPackageManager : protected pkgCache::Namespace
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
       packages later on and a front-end can choose to display a
       notice to inform the user about these disappears.
   */
   std::set<std::string> disappearedPkgs;

   void ImmediateAdd(PkgIterator P, bool UseInstallVer, unsigned const int &Depth = 0);
   virtual OrderResult OrderInstall();
   bool CheckRConflicts(PkgIterator Pkg,DepIterator Dep,const char *Ver);
   bool CheckRBreaks(PkgIterator const &Pkg,DepIterator Dep,const char * const Ver);
   bool CreateOrderList();
   
   // Analysis helpers
   bool DepAlwaysTrue(DepIterator D) APT_PURE;
   
   // Install helpers
   bool ConfigureAll();
   bool SmartConfigure(PkgIterator Pkg, int const Depth) APT_MUSTCHECK;
   bool SmartUnPack(PkgIterator Pkg, bool const Immediate = true, int const Depth = 0) APT_MUSTCHECK;
   bool SmartRemove(PkgIterator Pkg) APT_MUSTCHECK;
   bool EarlyRemove(PkgIterator Pkg, DepIterator const * const Dep) APT_MUSTCHECK;

   // The Actual installation implementation
   virtual bool Install(PkgIterator /*Pkg*/,std::string /*File*/) {return false;};
   virtual bool Configure(PkgIterator /*Pkg*/) {return false;};
   virtual bool Remove(PkgIterator /*Pkg*/,bool /*Purge*/=false) {return false;};
   virtual bool Go(APT::Progress::PackageManager * /*progress*/) {return true;};

   virtual void Reset() {};

   // the result of the operation
   OrderResult Res;

   public:
      
   // Main action members
   bool GetArchives(pkgAcquire *Owner,pkgSourceList *Sources,
		    pkgRecords *Recs);

   // Do the installation
   OrderResult DoInstall(APT::Progress::PackageManager *progress);

   friend bool EIPP::OrderInstall(char const * const planner, pkgPackageManager * const PM,
	 unsigned int const version, OpProgress * const Progress);
   friend bool EIPP::ReadResponse(int const input, pkgPackageManager * const PM,
	 OpProgress * const Progress);

   // stuff that needs to be done before the fork() of a library that
   // uses apt
   OrderResult DoInstallPreFork() {
      Res = OrderInstall();
      return Res;
   };
   // stuff that needs to be done after the fork
   OrderResult DoInstallPostFork(APT::Progress::PackageManager *progress);
   // compat

   // ?
   bool FixMissing();

   /** \brief returns all packages dpkg let disappear */
   inline std::set<std::string> GetDisappearedPackages() { return disappearedPkgs; };

   explicit pkgPackageManager(pkgDepCache *Cache);
   virtual ~pkgPackageManager();

   private:
   void * const d;
   enum APT_HIDDEN SmartAction { UNPACK_IMMEDIATE, UNPACK, CONFIGURE };
   APT_HIDDEN bool NonLoopingSmart(SmartAction const action, pkgCache::PkgIterator &Pkg,
      pkgCache::PkgIterator DepPkg, int const Depth, bool const PkgLoop,
      bool * const Bad, bool * const Changed) APT_MUSTCHECK;
};

#endif
