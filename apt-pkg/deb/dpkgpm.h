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
#include <vector>
#include <map>
#include <stdio.h>

using std::vector;
using std::map;


class pkgDPkgPM : public pkgPackageManager
{
   private:

   bool stdin_is_dev_null;

   // the buffer we use for the dpkg status-fd reading
   char dpkgbuf[1024];
   int dpkgbuf_pos;
   FILE *term_out;
   
   protected:

   // progress reporting
   struct DpkgState 
   {
      const char *state;     // the dpkg state (e.g. "unpack")
      const char *str;       // the human readable translation of the state
   };

   // the dpkg states that the pkg will run through, the string is 
   // the package, the vector contains the dpkg states that the package
   // will go through
   map<string,vector<struct DpkgState> > PackageOps;
   // the dpkg states that are already done; the string is the package
   // the int is the state that is already done (e.g. a package that is
   // going to be install is already in state "half-installed")
   map<string,unsigned int> PackageOpsDone;
   // progress reporting
   unsigned int PackagesDone;
   unsigned int PackagesTotal;
  
   struct Item
   {
      enum Ops {Install, Configure, Remove, Purge, ConfigurePending} Op;
      string File;
      PkgIterator Pkg;
      Item(Ops Op,PkgIterator Pkg,string File = "") : Op(Op),
            File(File), Pkg(Pkg) {};
      Item() {};
      
   };
   vector<Item> List;

   // Helpers
   bool RunScriptsWithPkgs(const char *Cnf);
   bool SendV2Pkgs(FILE *F);

   // dpkg log
   bool OpenLog();
   bool CloseLog();
   
   // input processing
   void DoStdin(int master);
   void DoTerminalPty(int master);
   void DoDpkgStatusFd(int statusfd, int OutStatusFd);
   void ProcessDpkgStatusLine(int OutStatusFd, char *line);

   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg,bool Purge = false);
   virtual bool Go(int StatusFd=-1);
   virtual void Reset();
   
   public:

   pkgDPkgPM(pkgDepCache *Cache);
   virtual ~pkgDPkgPM();
};

#endif
