// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.h,v 1.8 2001/05/07 05:05:13 jgg Exp $
/* ######################################################################

   DPKG Package Manager - Provide an interface to dpkg
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DPKGPM_H
#define PKGLIB_DPKGPM_H

#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>

#include <vector>
#include <map>
#include <stdio.h>
#include <string>

#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/init.h>
#endif

class pkgDepCache;
namespace APT { namespace Progress { class PackageManager; } }

#ifndef APT_8_CLEANER_HEADERS
using std::vector;
using std::map;
#endif

class pkgDPkgPMPrivate;


class pkgDPkgPM : public pkgPackageManager
{
   private:
   pkgDPkgPMPrivate *d;

   /** \brief record the disappear action and handle accordingly

      dpkg let packages disappear then they have no files any longer and
      nothing depends on them. We need to collect this as dpkg as well as
      APT doesn't know beforehand that the package will disappear, so the
      only possible option is to tell the user afterwards about it.
      To enhance the experience we also try to forward the auto-install
      flag so the disappear-causer(s) are not autoremoved next time -
      for the transfer to happen the disappeared version needs to depend
      on the package the flag should be forwarded to and this package
      needs to declare a Replaces on the disappeared package.
      \param pkgname Name of the package that disappeared
   */
   APT_HIDDEN void handleDisappearAction(std::string const &pkgname);

   protected:
   int pkgFailures;

   // progress reporting
   struct DpkgState 
   {
      const char *state;     // the dpkg state (e.g. "unpack")
      const char *str;       // the human readable translation of the state
   };

   // the dpkg states that the pkg will run through, the string is 
   // the package, the vector contains the dpkg states that the package
   // will go through
   std::map<std::string,std::vector<struct DpkgState> > PackageOps;
   // the dpkg states that are already done; the string is the package
   // the int is the state that is already done (e.g. a package that is
   // going to be install is already in state "half-installed")
   std::map<std::string,unsigned int> PackageOpsDone;

   // progress reporting
   unsigned int PackagesDone;
   unsigned int PackagesTotal;
  
   struct Item
   {
      enum Ops {Install, Configure, Remove, Purge, ConfigurePending, TriggersPending} Op;
      std::string File;
      PkgIterator Pkg;
      Item(Ops Op,PkgIterator Pkg,std::string File = "") : Op(Op),
            File(File), Pkg(Pkg) {};
      Item() {};
      
   };
   std::vector<Item> List;

   // Helpers
   bool RunScriptsWithPkgs(const char *Cnf);
   APT_DEPRECATED bool SendV2Pkgs(FILE *F);
   bool SendPkgsInfo(FILE * const F, unsigned int const &Version);
   void WriteHistoryTag(std::string const &tag, std::string value);
   std::string ExpandShortPackageName(pkgDepCache &Cache,
                                      const std::string &short_pkgname);

   // Terminal progress 
   void SendTerminalProgress(float percentage);

   // apport integration
   void WriteApportReport(const char *pkgpath, const char *errormsg);

   // dpkg log
   bool OpenLog();
   bool CloseLog();

   // helper
   void BuildPackagesProgressMap();
   void StartPtyMagic();
   void SetupSlavePtyMagic();
   void StopPtyMagic();
   
   // input processing
   void DoStdin(int master);
   void DoTerminalPty(int master);
   void DoDpkgStatusFd(int statusfd);
   void ProcessDpkgStatusLine(char *line);

   // The Actual installation implementation
   virtual bool Install(PkgIterator Pkg,std::string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg,bool Purge = false);

   virtual bool Go(APT::Progress::PackageManager *progress);
   virtual bool Go(int StatusFd=-1);

   virtual void Reset();
   
   public:

   pkgDPkgPM(pkgDepCache *Cache);
   virtual ~pkgDPkgPM();
};

void SigINT(int sig);

#endif
