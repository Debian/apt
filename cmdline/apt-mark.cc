// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################
   apt-mark - show and change auto-installed bit information
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/pkgsystem.h>

#include <algorithm>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/
using namespace std;

ostream c0out(0);
ostream c1out(0);
ostream c2out(0);
ofstream devnull("/dev/null");
/* DoAuto - mark packages as automatically/manually installed		{{{*/
bool DoAuto(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   if (unlikely(Cache == NULL || DepCache == NULL))
      return false;

   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1);
   if (pkgset.empty() == true)
      return _error->Error(_("No packages found"));

   bool MarkAuto = strcasecmp(CmdL.FileList[0],"auto") == 0;
   int AutoMarkChanged = 0;

   for (APT::PackageList::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      if (Pkg->CurrentVer == 0)
      {
	 ioprintf(c1out,_("%s can not be marked as it is not installed.\n"), Pkg.FullName(true).c_str());
	 continue;
      }
      else if ((((*DepCache)[Pkg].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto) == MarkAuto)
      {
	 if (MarkAuto == false)
	    ioprintf(c1out,_("%s was already set to manually installed.\n"), Pkg.FullName(true).c_str());
	 else
	    ioprintf(c1out,_("%s was already set to automatically installed.\n"), Pkg.FullName(true).c_str());
	 continue;
      }

      if (MarkAuto == false)
	 ioprintf(c1out,_("%s set to manually installed.\n"), Pkg.FullName(true).c_str());
      else
	 ioprintf(c1out,_("%s set to automatically installed.\n"), Pkg.FullName(true).c_str());

      DepCache->MarkAuto(Pkg, MarkAuto);
      ++AutoMarkChanged;
   }
   if (AutoMarkChanged > 0 && _config->FindB("APT::Mark::Simulate", false) == false)
      return DepCache->writeStateFile(NULL);
   return true;
}
									/*}}}*/
/* DoMarkAuto - mark packages as automatically/manually installed	{{{*/
/* Does the same as DoAuto but tries to do it exactly the same why as
   the python implementation did it so it can be a drop-in replacement */
bool DoMarkAuto(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   if (unlikely(Cache == NULL || DepCache == NULL))
      return false;

   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1);
   if (pkgset.empty() == true)
      return _error->Error(_("No packages found"));

   bool const MarkAuto = strcasecmp(CmdL.FileList[0],"markauto") == 0;
   bool const Verbose = _config->FindB("APT::MarkAuto::Verbose", false);
   int AutoMarkChanged = 0;

   for (APT::PackageList::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      if (Pkg->CurrentVer == 0 ||
	  (((*DepCache)[Pkg].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto) == MarkAuto)
	 continue;

      if (Verbose == true)
	 ioprintf(c1out, "changing %s to %d\n", Pkg.Name(), (MarkAuto == false) ? 0 : 1);

      DepCache->MarkAuto(Pkg, MarkAuto);
      ++AutoMarkChanged;
   }
   if (AutoMarkChanged > 0 && _config->FindB("APT::Mark::Simulate", false) == false)
      return DepCache->writeStateFile(NULL);

   _error->Notice(_("This command is deprecated. Please use 'apt-mark auto' and 'apt-mark manual' instead."));

   return true;
}
									/*}}}*/
/* ShowAuto - show automatically installed packages (sorted)		{{{*/
bool ShowAuto(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   if (unlikely(Cache == NULL || DepCache == NULL))
      return false;

   std::vector<string> packages;

   bool const ShowAuto = strcasecmp(CmdL.FileList[0],"showauto") == 0;

   if (CmdL.FileList[1] == 0)
   {
      packages.reserve(Cache->HeaderP->PackageCount / 3);
      for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
	 if (P->CurrentVer != 0 &&
	     (((*DepCache)[P].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto) == ShowAuto)
	    packages.push_back(P.FullName(true));
   }
   else
   {
      APT::CacheSetHelper helper(false); // do not show errors
      APT::PackageSet pkgset = APT::PackageSet::FromCommandLine(CacheFile, CmdL.FileList + 1, helper);
      packages.reserve(pkgset.size());
      for (APT::PackageSet::const_iterator P = pkgset.begin(); P != pkgset.end(); ++P)
	 if (P->CurrentVer != 0 &&
	     (((*DepCache)[P].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto) == ShowAuto)
	    packages.push_back(P.FullName(true));
   }

   std::sort(packages.begin(), packages.end());

   for (vector<string>::const_iterator I = packages.begin(); I != packages.end(); ++I)
      std::cout << *I << std::endl;

   return true;
}
									/*}}}*/
/* DoHold - mark packages as hold by dpkg				{{{*/
bool DoHold(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1);
   if (pkgset.empty() == true)
      return _error->Error(_("No packages found"));

   bool const MarkHold = strcasecmp(CmdL.FileList[0],"hold") == 0;

   for (APT::PackageList::iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      if ((Pkg->SelectedState == pkgCache::State::Hold) == MarkHold)
      {
	 if (MarkHold == true)
	    ioprintf(c1out,_("%s was already set on hold.\n"), Pkg.FullName(true).c_str());
	 else
	    ioprintf(c1out,_("%s was already not hold.\n"), Pkg.FullName(true).c_str());
	 pkgset.erase(Pkg);
	 continue;
      }
   }

   if (pkgset.empty() == true)
      return true;

   if (_config->FindB("APT::Mark::Simulate", false) == true)
   {
      for (APT::PackageList::iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
      {
	 if (MarkHold == false)
	    ioprintf(c1out,_("%s set on hold.\n"), Pkg.FullName(true).c_str());
	 else
	    ioprintf(c1out,_("Canceled hold on %s.\n"), Pkg.FullName(true).c_str());
      }
      return true;
   }

   string dpkgcall = _config->Find("Dir::Bin::dpkg", "dpkg");
   std::vector<string> const dpkgoptions = _config->FindVector("DPkg::options");
   for (std::vector<string>::const_iterator o = dpkgoptions.begin();
	o != dpkgoptions.end(); ++o)
      dpkgcall.append(" ").append(*o);
   dpkgcall.append(" --set-selections");
   FILE *dpkg = popen(dpkgcall.c_str(), "w");
   if (dpkg == NULL)
      return _error->Errno("DoHold", "fdopen on dpkg stdin failed");

   for (APT::PackageList::iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      if (MarkHold == true)
      {
	 fprintf(dpkg, "%s hold\n", Pkg.FullName(true).c_str());
	 ioprintf(c1out,_("%s set on hold.\n"), Pkg.FullName(true).c_str());
      }
      else
      {
	 fprintf(dpkg, "%s install\n", Pkg.FullName(true).c_str());
	 ioprintf(c1out,_("Canceled hold on %s.\n"), Pkg.FullName(true).c_str());
      }
   }

   int const status = pclose(dpkg);
   if (status == -1)
      return _error->Errno("DoHold", "dpkg execution failed in the end");
   if (WIFEXITED(status) == false || WEXITSTATUS(status) != 0)
      return _error->Error(_("Executing dpkg failed. Are you root?"));
   return true;
}
									/*}}}*/
/* ShowHold - show packages set on hold in dpkg status			{{{*/
bool ShowHold(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

   std::vector<string> packages;

   if (CmdL.FileList[1] == 0)
   {
      packages.reserve(50); // how many holds are realistic? I hope just a few…
      for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
	 if (P->SelectedState == pkgCache::State::Hold)
	    packages.push_back(P.FullName(true));
   }
   else
   {
      APT::CacheSetHelper helper(false); // do not show errors
      APT::PackageSet pkgset = APT::PackageSet::FromCommandLine(CacheFile, CmdL.FileList + 1, helper);
      packages.reserve(pkgset.size());
      for (APT::PackageSet::const_iterator P = pkgset.begin(); P != pkgset.end(); ++P)
	 if (P->SelectedState == pkgCache::State::Hold)
	    packages.push_back(P.FullName(true));
   }

   std::sort(packages.begin(), packages.end());

   for (vector<string>::const_iterator I = packages.begin(); I != packages.end(); ++I)
      std::cout << *I << std::endl;

   return true;
}
									/*}}}*/
// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp(CommandLine &CmdL)
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);

   cout <<
    _("Usage: apt-mark [options] {auto|manual} pkg1 [pkg2 ...]\n"
      "\n"
      "apt-mark is a simple command line interface for marking packages\n"
      "as manual or automatical installed. It can also list marks.\n"
      "\n"
      "Commands:\n"
      "   auto - Mark the given packages as automatically installed\n"
      "   manual - Mark the given packages as manually installed\n"
      "\n"
      "Options:\n"
      "  -h  This help text.\n"
      "  -q  Loggable output - no progress indicator\n"
      "  -qq No output except for errors\n"
      "  -s  No-act. Just prints what would be done.\n"
      "  -f  read/write auto/manual marking in the given file\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See the apt-mark(8) and apt.conf(5) manual pages for more information.")
      << std::endl;
   return true;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {0,"version","version",0},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'q',"silent","quiet",CommandLine::IntLevel},
      {'v',"verbose","APT::MarkAuto::Verbose",0},
      {'s',"simulate","APT::Mark::Simulate",0},
      {'s',"just-print","APT::Mark::Simulate",0},
      {'s',"recon","APT::Mark::Simulate",0},
      {'s',"dry-run","APT::Mark::Simulate",0},
      {'s',"no-act","APT::Mark::Simulate",0},
      {'f',"file","Dir::State::extended_states",CommandLine::HasArg},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {{"help",&ShowHelp},
				   {"auto",&DoAuto},
				   {"manual",&DoAuto},
				   {"hold",&DoHold},
				   {"unhold",&DoHold},
				   {"showauto",&ShowAuto},
				   {"showmanual",&ShowAuto},
				   {"showhold",&ShowHold},
				   // be nice and forgive the typo
				   {"showholds",&ShowHold},
				   // be nice and forgive it as it is technical right
				   {"install",&DoHold},
				   // obsolete commands for compatibility
				   {"markauto", &DoMarkAuto},
				   {"unmarkauto", &DoMarkAuto},
                                   {0,0}};

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      if (_config->FindB("version") == true)
	 ShowHelp(CmdL);
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
   {
      ShowHelp(CmdL);
      return 0;
   }

   // Deal with stdout not being a tty
   if (!isatty(STDOUT_FILENO) && _config->FindI("quiet", -1) == -1)
      _config->Set("quiet","1");

   // Setup the output streams
   c0out.rdbuf(cout.rdbuf());
   c1out.rdbuf(cout.rdbuf());
   c2out.rdbuf(cout.rdbuf());
   if (_config->FindI("quiet",0) > 0)
      c0out.rdbuf(devnull.rdbuf());
   if (_config->FindI("quiet",0) > 1)
      c1out.rdbuf(devnull.rdbuf());

   // Match the operation
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   return Errors == true ? 100 : 0;
}
									/*}}}*/
