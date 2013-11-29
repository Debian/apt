#ifndef APT_PRIVATE_INSTALL_H
#define APT_PRIVATE_INSTALL_H

#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>

#include "private-cachefile.h"
#include "private-output.h"

#include <apti18n.h>

#define RAMFS_MAGIC     0x858458f6

bool DoInstall(CommandLine &Cmd);

bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, CacheFile &Cache,
                                        std::map<unsigned short, APT::VersionSet> &verset);
bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, CacheFile &Cache);

bool InstallPackages(CacheFile &Cache,bool ShwKept,bool Ask = true,
                        bool Safety = true);


// TryToInstall - Mark a package for installation			/*{{{*/
struct TryToInstall {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool FixBroken;
   unsigned long AutoMarkChanged;
   APT::PackageSet doAutoInstallLater;

   TryToInstall(pkgCacheFile &Cache, pkgProblemResolver *PM, bool const FixBroken) : Cache(&Cache), Fix(PM),
			FixBroken(FixBroken), AutoMarkChanged(0) {};

   void operator() (pkgCache::VerIterator const &Ver) {
      pkgCache::PkgIterator Pkg = Ver.ParentPkg();

      Cache->GetDepCache()->SetCandidateVersion(Ver);
      pkgDepCache::StateCache &State = (*Cache)[Pkg];

      // Handle the no-upgrade case
      if (_config->FindB("APT::Get::upgrade",true) == false && Pkg->CurrentVer != 0)
	 ioprintf(c1out,_("Skipping %s, it is already installed and upgrade is not set.\n"),
		  Pkg.FullName(true).c_str());
      // Ignore request for install if package would be new
      else if (_config->FindB("APT::Get::Only-Upgrade", false) == true && Pkg->CurrentVer == 0)
	 ioprintf(c1out,_("Skipping %s, it is not installed and only upgrades are requested.\n"),
		  Pkg.FullName(true).c_str());
      else {
	 if (Fix != NULL) {
	    Fix->Clear(Pkg);
	    Fix->Protect(Pkg);
	 }
	 Cache->GetDepCache()->MarkInstall(Pkg,false);

	 if (State.Install() == false) {
	    if (_config->FindB("APT::Get::ReInstall",false) == true) {
	       if (Pkg->CurrentVer == 0 || Pkg.CurrentVer().Downloadable() == false)
		  ioprintf(c1out,_("Reinstallation of %s is not possible, it cannot be downloaded.\n"),
			   Pkg.FullName(true).c_str());
	       else
		  Cache->GetDepCache()->SetReInstall(Pkg, true);
	    } else
	       ioprintf(c1out,_("%s is already the newest version.\n"),
			Pkg.FullName(true).c_str());
	 }

	 // Install it with autoinstalling enabled (if we not respect the minial
	 // required deps or the policy)
	 if (FixBroken == false)
	    doAutoInstallLater.insert(Pkg);
      }

      // see if we need to fix the auto-mark flag
      // e.g. apt-get install foo
      // where foo is marked automatic
      if (State.Install() == false &&
	  (State.Flags & pkgCache::Flag::Auto) &&
	  _config->FindB("APT::Get::ReInstall",false) == false &&
	  _config->FindB("APT::Get::Only-Upgrade",false) == false &&
	  _config->FindB("APT::Get::Download-Only",false) == false)
      {
	 ioprintf(c1out,_("%s set to manually installed.\n"),
		  Pkg.FullName(true).c_str());
	 Cache->GetDepCache()->MarkAuto(Pkg,false);
	 AutoMarkChanged++;
      }
   }

   bool propergateReleaseCandiateSwitching(std::list<std::pair<pkgCache::VerIterator, std::string> > start, std::ostream &out)
   {
      for (std::list<std::pair<pkgCache::VerIterator, std::string> >::const_iterator s = start.begin();
		s != start.end(); ++s)
	 Cache->GetDepCache()->SetCandidateVersion(s->first);

      bool Success = true;
      // the Changed list contains:
      //   first: "new version" 
      //   second: "what-caused the change" 
      std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > Changed;
      for (std::list<std::pair<pkgCache::VerIterator, std::string> >::const_iterator s = start.begin();
		s != start.end(); ++s)
      {
	 Changed.push_back(std::make_pair(s->first, pkgCache::VerIterator(*Cache)));
	 // We continue here even if it failed to enhance the ShowBroken output
	 Success &= Cache->GetDepCache()->SetCandidateRelease(s->first, s->second, Changed);
      }
      for (std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> >::const_iterator c = Changed.begin();
	   c != Changed.end(); ++c)
      {
	 if (c->second.end() == true)
	    ioprintf(out, _("Selected version '%s' (%s) for '%s'\n"),
		     c->first.VerStr(), c->first.RelStr().c_str(), c->first.ParentPkg().FullName(true).c_str());
	 else if (c->first.ParentPkg()->Group != c->second.ParentPkg()->Group)
	 {
	    pkgCache::VerIterator V = (*Cache)[c->first.ParentPkg()].CandidateVerIter(*Cache);
	    ioprintf(out, _("Selected version '%s' (%s) for '%s' because of '%s'\n"), V.VerStr(),
		     V.RelStr().c_str(), V.ParentPkg().FullName(true).c_str(), c->second.ParentPkg().FullName(true).c_str());
	 }
      }
      return Success;
   }

   void doAutoInstall() {
      for (APT::PackageSet::const_iterator P = doAutoInstallLater.begin();
	   P != doAutoInstallLater.end(); ++P) {
	 pkgDepCache::StateCache &State = (*Cache)[P];
	 if (State.InstBroken() == false && State.InstPolicyBroken() == false)
	    continue;
	 Cache->GetDepCache()->MarkInstall(P, true);
      }
      doAutoInstallLater.clear();
   }
};
									/*}}}*/
// TryToRemove - Mark a package for removal				/*{{{*/
struct TryToRemove {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool PurgePkgs;

   TryToRemove(pkgCacheFile &Cache, pkgProblemResolver *PM) : Cache(&Cache), Fix(PM),
				PurgePkgs(_config->FindB("APT::Get::Purge", false)) {};

   void operator() (pkgCache::VerIterator const &Ver)
   {
      pkgCache::PkgIterator Pkg = Ver.ParentPkg();

      if (Fix != NULL)
      {
	 Fix->Clear(Pkg);
	 Fix->Protect(Pkg);
	 Fix->Remove(Pkg);
      }

      if ((Pkg->CurrentVer == 0 && PurgePkgs == false) ||
	  (PurgePkgs == true && Pkg->CurrentState == pkgCache::State::NotInstalled))
      {
	 pkgCache::GrpIterator Grp = Pkg.Group();
	 pkgCache::PkgIterator P = Grp.PackageList();
	 for (; P.end() != true; P = Grp.NextPkg(P))
	 {
	    if (P == Pkg)
	       continue;
	    if (P->CurrentVer != 0 || (PurgePkgs == true && P->CurrentState != pkgCache::State::NotInstalled))
	    {
	       // TRANSLATORS: Note, this is not an interactive question
	       ioprintf(c1out,_("Package '%s' is not installed, so not removed. Did you mean '%s'?\n"),
			Pkg.FullName(true).c_str(), P.FullName(true).c_str());
	       break;
	    }
	 }
	 if (P.end() == true)
	    ioprintf(c1out,_("Package '%s' is not installed, so not removed\n"),Pkg.FullName(true).c_str());

	 // MarkInstall refuses to install packages on hold
	 Pkg->SelectedState = pkgCache::State::Hold;
      }
      else
	 Cache->GetDepCache()->MarkDelete(Pkg, PurgePkgs);
   }
};
									/*}}}*/


#endif
