// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-get.cc,v 1.156 2004/08/28 01:05:16 mdz Exp $
/* ######################################################################
   
   apt-get - Cover for dpkg
   
   This is an allout cover for dpkg implementing a safer front end. It is
   based largely on libapt-pkg.

   The syntax is different, 
      apt-get [opt] command [things]
   Where command is:
      update - Resyncronize the package files from their sources
      upgrade - Smart-Download the newest versions of all packages
      dselect-upgrade - Follows dselect's changes to the Status: field
                       and installes new and removes old packages
      dist-upgrade - Powerfull upgrader designed to handle the issues with
                    a new distribution.
      install - Download and install a given package (by name, not by .deb)
      check - Update the package cache and check for broken packages
      clean - Erase the .debs downloaded to /var/cache/apt/archives and
              the partial dir too

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/init.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/clean.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/versionmatch.h>

#include <config.h>
#include <apti18n.h>

#include "acqprogress.h"

#include <set>
#include <locale.h>
#include <langinfo.h>
#include <fstream>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <regex.h>
#include <sys/wait.h>
#include <sstream>

#define statfs statfs64
#define statvfs statvfs64
									/*}}}*/

#define RAMFS_MAGIC     0x858458f6

using namespace std;

ostream c0out(0);
ostream c1out(0);
ostream c2out(0);
ofstream devnull("/dev/null");
unsigned int ScreenWidth = 80 - 1; /* - 1 for the cursor */

// class CacheFile - Cover class for some dependency cache functions	/*{{{*/
// ---------------------------------------------------------------------
/* */
class CacheFile : public pkgCacheFile
{
   static pkgCache *SortCache;
   static int NameComp(const void *a,const void *b);
   
   public:
   pkgCache::Package **List;
   
   void Sort();
   bool CheckDeps(bool AllowBroken = false);
   bool BuildCaches(bool WithLock = true)
   {
      OpTextProgress Prog(*_config);
      if (pkgCacheFile::BuildCaches(&Prog,WithLock) == false)
	 return false;
      return true;
   }
   bool Open(bool WithLock = true) 
   {
      OpTextProgress Prog(*_config);
      if (pkgCacheFile::Open(&Prog,WithLock) == false)
	 return false;
      Sort();
      
      return true;
   };
   bool OpenForInstall()
   {
      if (_config->FindB("APT::Get::Print-URIs") == true)
	 return Open(false);
      else
	 return Open(true);
   }
   CacheFile() : List(0) {};
   ~CacheFile() {
      delete[] List;
   }
};
									/*}}}*/

// YnPrompt - Yes No Prompt.						/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool YnPrompt(bool Default=true)
{
   if (_config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      c1out << _("Y") << endl;
      return true;
   }

   char response[1024] = "";
   cin.getline(response, sizeof(response));

   if (!cin)
      return false;

   if (strlen(response) == 0)
      return Default;

   regex_t Pattern;
   int Res;

   Res = regcomp(&Pattern, nl_langinfo(YESEXPR),
                 REG_EXTENDED|REG_ICASE|REG_NOSUB);

   if (Res != 0) {
      char Error[300];        
      regerror(Res,&Pattern,Error,sizeof(Error));
      return _error->Error(_("Regex compilation error - %s"),Error);
   }
   
   Res = regexec(&Pattern, response, 0, NULL, 0);
   if (Res == 0)
      return true;
   return false;
}
									/*}}}*/
// AnalPrompt - Annoying Yes No Prompt.					/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool AnalPrompt(const char *Text)
{
   char Buf[1024];
   cin.getline(Buf,sizeof(Buf));
   if (strcmp(Buf,Text) == 0)
      return true;
   return false;
}
									/*}}}*/
// ShowList - Show a list						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a string of space separated words with a title and 
   a two space indent line wraped to the current screen width. */
bool ShowList(ostream &out,string Title,string List,string VersionsList)
{
   if (List.empty() == true)
      return true;
   // trim trailing space
   int NonSpace = List.find_last_not_of(' ');
   if (NonSpace != -1)
   {
      List = List.erase(NonSpace + 1);
      if (List.empty() == true)
	 return true;
   }

   // Acount for the leading space
   int ScreenWidth = ::ScreenWidth - 3;
      
   out << Title << endl;
   string::size_type Start = 0;
   string::size_type VersionsStart = 0;
   while (Start < List.size())
   {
      if(_config->FindB("APT::Get::Show-Versions",false) == true &&
         VersionsList.size() > 0) {
         string::size_type End;
         string::size_type VersionsEnd;
         
         End = List.find(' ',Start);
         VersionsEnd = VersionsList.find('\n', VersionsStart);

         out << "   " << string(List,Start,End - Start) << " (" << 
            string(VersionsList,VersionsStart,VersionsEnd - VersionsStart) << 
            ")" << endl;

	 if (End == string::npos || End < Start)
	    End = Start + ScreenWidth;

         Start = End + 1;
         VersionsStart = VersionsEnd + 1;
      } else {
         string::size_type End;

         if (Start + ScreenWidth >= List.size())
            End = List.size();
         else
            End = List.rfind(' ',Start+ScreenWidth);

         if (End == string::npos || End < Start)
            End = Start + ScreenWidth;
         out << "  " << string(List,Start,End - Start) << endl;
         Start = End + 1;
      }
   }   

   return false;
}
									/*}}}*/
// ShowBroken - Debugging aide						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
   with the name of each each broken dependency and a quite version 
   description.
   
   The output looks like:
 The following packages have unmet dependencies:
     exim: Depends: libc6 (>= 2.1.94) but 2.1.3-10 is to be installed
           Depends: libldap2 (>= 2.0.2-2) but it is not going to be installed
           Depends: libsasl7 but it is not going to be installed   
 */
void ShowBroken(ostream &out,CacheFile &Cache,bool Now)
{
   out << _("The following packages have unmet dependencies:") << endl;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      if (Now == true)
      {
	 if (Cache[I].NowBroken() == false)
	    continue;
      }
      else
      {
	 if (Cache[I].InstBroken() == false)
	    continue;
      }
      
      // Print out each package and the failed dependencies
      out << " " << I.FullName(true) << " :";
      unsigned const Indent = I.FullName(true).size() + 3;
      bool First = true;
      pkgCache::VerIterator Ver;
      
      if (Now == true)
	 Ver = I.CurrentVer();
      else
	 Ver = Cache[I].InstVerIter(Cache);
      
      if (Ver.end() == true)
      {
	 out << endl;
	 continue;
      }
      
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false;)
      {
	 // Compute a single dependency element (glob or)
	 pkgCache::DepIterator Start;
	 pkgCache::DepIterator End;
	 D.GlobOr(Start,End); // advances D

	 if (Cache->IsImportantDep(End) == false)
	    continue;
	 
	 if (Now == true)
	 {
	    if ((Cache[End] & pkgDepCache::DepGNow) == pkgDepCache::DepGNow)
	       continue;
	 }
	 else
	 {
	    if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	       continue;
	 }
	 
	 bool FirstOr = true;
	 while (1)
	 {
	    if (First == false)
	       for (unsigned J = 0; J != Indent; J++)
		  out << ' ';
	    First = false;

	    if (FirstOr == false)
	    {
	       for (unsigned J = 0; J != strlen(End.DepType()) + 3; J++)
		  out << ' ';
	    }
	    else
	       out << ' ' << End.DepType() << ": ";
	    FirstOr = false;
	    
	    out << Start.TargetPkg().FullName(true);
	 
	    // Show a quick summary of the version requirements
	    if (Start.TargetVer() != 0)
	       out << " (" << Start.CompType() << " " << Start.TargetVer() << ")";
	    
	    /* Show a summary of the target package if possible. In the case
	       of virtual packages we show nothing */	 
	    pkgCache::PkgIterator Targ = Start.TargetPkg();
	    if (Targ->ProvidesList == 0)
	    {
	       out << ' ';
	       pkgCache::VerIterator Ver = Cache[Targ].InstVerIter(Cache);
	       if (Now == true)
		  Ver = Targ.CurrentVer();
	       	    
	       if (Ver.end() == false)
	       {
		  if (Now == true)
		     ioprintf(out,_("but %s is installed"),Ver.VerStr());
		  else
		     ioprintf(out,_("but %s is to be installed"),Ver.VerStr());
	       }	       
	       else
	       {
		  if (Cache[Targ].CandidateVerIter(Cache).end() == true)
		  {
		     if (Targ->ProvidesList == 0)
			out << _("but it is not installable");
		     else
			out << _("but it is a virtual package");
		  }		  
		  else
		     out << (Now?_("but it is not installed"):_("but it is not going to be installed"));
	       }	       
	    }
	    
	    if (Start != End)
	       out << _(" or");
	    out << endl;
	    
	    if (Start == End)
	       break;
	    Start++;
	 }	 
      }	    
   }   
}
									/*}}}*/
// ShowNew - Show packages to newly install				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowNew(ostream &out,CacheFile &Cache)
{
   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].NewInstall() == true) {
         List += I.FullName(true) + " ";
         VersionsList += string(Cache[I].CandVersion) + "\n";
      }
   }
   
   ShowList(out,_("The following NEW packages will be installed:"),List,VersionsList);
}
									/*}}}*/
// ShowDel - Show packages to delete					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowDel(ostream &out,CacheFile &Cache)
{
   /* Print out a list of packages that are going to be removed extra
      to what the user asked */
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].Delete() == true)
      {
	 if ((Cache[I].iFlags & pkgDepCache::Purge) == pkgDepCache::Purge)
	    List += I.FullName(true) + "* ";
	 else
	    List += I.FullName(true) + " ";
     
     VersionsList += string(Cache[I].CandVersion)+ "\n";
      }
   }
   
   ShowList(out,_("The following packages will be REMOVED:"),List,VersionsList);
}
									/*}}}*/
// ShowKept - Show kept packages					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowKept(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {	 
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == true || Cache[I].Upgradable() == false ||
	  I->CurrentVer == 0 || Cache[I].Delete() == true)
	 continue;
      
      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   ShowList(out,_("The following packages have been kept back:"),List,VersionsList);
}
									/*}}}*/
// ShowUpgraded - Show upgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowUpgraded(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == false || Cache[I].NewInstall() == true)
	 continue;

      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   ShowList(out,_("The following packages will be upgraded:"),List,VersionsList);
}
									/*}}}*/
// ShowDowngraded - Show downgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowDowngraded(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Downgrade() == false || Cache[I].NewInstall() == true)
	 continue;

      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   return ShowList(out,_("The following packages will be DOWNGRADED:"),List,VersionsList);
}
									/*}}}*/
// ShowHold - Show held but changed packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHold(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].InstallVer != (pkgCache::Version *)I.CurrentVer() &&
          I->SelectedState == pkgCache::State::Hold) {
         List += I.FullName(true) + " ";
		 VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
      }
   }

   return ShowList(out,_("The following held packages will be changed:"),List,VersionsList);
}
									/*}}}*/
// ShowEssential - Show an essential package warning			/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a warning message that is not to be ignored. It shows
   all essential packages and their dependents that are to be removed. 
   It is insanely risky to remove the dependents of an essential package! */
bool ShowEssential(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   bool *Added = new bool[Cache->Head().PackageCount];
   for (unsigned int I = 0; I != Cache->Head().PackageCount; I++)
      Added[I] = false;
   
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if ((I->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential &&
	  (I->Flags & pkgCache::Flag::Important) != pkgCache::Flag::Important)
	 continue;
      
      // The essential package is being removed
      if (Cache[I].Delete() == true)
      {
	 if (Added[I->ID] == false)
	 {
	    Added[I->ID] = true;
	    List += I.FullName(true) + " ";
        //VersionsList += string(Cache[I].CurVersion) + "\n"; ???
	 }
      }
      else
	 continue;

      if (I->CurrentVer == 0)
	 continue;

      // Print out any essential package depenendents that are to be removed
      for (pkgCache::DepIterator D = I.CurrentVer().DependsList(); D.end() == false; D++)
      {
	 // Skip everything but depends
	 if (D->Type != pkgCache::Dep::PreDepends &&
	     D->Type != pkgCache::Dep::Depends)
	    continue;
	 
	 pkgCache::PkgIterator P = D.SmartTargetPkg();
	 if (Cache[P].Delete() == true)
	 {
	    if (Added[P->ID] == true)
	       continue;
	    Added[P->ID] = true;
	    
	    char S[300];
	    snprintf(S,sizeof(S),_("%s (due to %s) "),P.FullName(true).c_str(),I.FullName(true).c_str());
	    List += S;
        //VersionsList += "\n"; ???
	 }	 
      }      
   }
   
   delete [] Added;
   return ShowList(out,_("WARNING: The following essential packages will be removed.\n"
			 "This should NOT be done unless you know exactly what you are doing!"),List,VersionsList);
}

									/*}}}*/
// Stats - Show some statistics						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Stats(ostream &out,pkgDepCache &Dep)
{
   unsigned long Upgrade = 0;
   unsigned long Downgrade = 0;
   unsigned long Install = 0;
   unsigned long ReInstall = 0;
   for (pkgCache::PkgIterator I = Dep.PkgBegin(); I.end() == false; I++)
   {
      if (Dep[I].NewInstall() == true)
	 Install++;
      else
      {
	 if (Dep[I].Upgrade() == true)
	    Upgrade++;
	 else
	    if (Dep[I].Downgrade() == true)
	       Downgrade++;
      }
      
      if (Dep[I].Delete() == false && (Dep[I].iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall)
	 ReInstall++;
   }   

   ioprintf(out,_("%lu upgraded, %lu newly installed, "),
	    Upgrade,Install);
   
   if (ReInstall != 0)
      ioprintf(out,_("%lu reinstalled, "),ReInstall);
   if (Downgrade != 0)
      ioprintf(out,_("%lu downgraded, "),Downgrade);

   ioprintf(out,_("%lu to remove and %lu not upgraded.\n"),
	    Dep.DelCount(),Dep.KeepCount());
   
   if (Dep.BadCount() != 0)
      ioprintf(out,_("%lu not fully installed or removed.\n"),
	       Dep.BadCount());
}
									/*}}}*/
// CacheSetHelperAPTGet - responsible for message telling from the CacheSets/*{{{*/
class CacheSetHelperAPTGet : public APT::CacheSetHelper {
	/** \brief stream message should be printed to */
	std::ostream &out;
	/** \brief were things like Task or RegEx used to select packages? */
	bool explicitlyNamed;

	APT::PackageSet virtualPkgs;

public:
	std::list<std::pair<pkgCache::VerIterator, std::string> > selectedByRelease;

	CacheSetHelperAPTGet(std::ostream &out) : APT::CacheSetHelper(true), out(out) {
		explicitlyNamed = true;
	}

	virtual void showTaskSelection(APT::PackageSet const &pkgset, string const &pattern) {
		for (APT::PackageSet::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
			ioprintf(out, _("Note, selecting '%s' for task '%s'\n"),
				 Pkg.FullName(true).c_str(), pattern.c_str());
		explicitlyNamed = false;
	}
	virtual void showRegExSelection(APT::PackageSet const &pkgset, string const &pattern) {
		for (APT::PackageSet::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
			ioprintf(out, _("Note, selecting '%s' for regex '%s'\n"),
				 Pkg.FullName(true).c_str(), pattern.c_str());
		explicitlyNamed = false;
	}
	virtual void showSelectedVersion(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const Ver,
				 string const &ver, bool const &verIsRel) {
		if (ver == Ver.VerStr())
			return;
		selectedByRelease.push_back(make_pair(Ver, ver));
	}

	bool showVirtualPackageErrors(pkgCacheFile &Cache) {
		if (virtualPkgs.empty() == true)
			return true;
		for (APT::PackageSet::const_iterator Pkg = virtualPkgs.begin();
		     Pkg != virtualPkgs.end(); ++Pkg) {
			if (Pkg->ProvidesList != 0) {
				ioprintf(c1out,_("Package %s is a virtual package provided by:\n"),
					 Pkg.FullName(true).c_str());

				pkgCache::PrvIterator I = Pkg.ProvidesList();
				unsigned short provider = 0;
				for (; I.end() == false; ++I) {
					pkgCache::PkgIterator Pkg = I.OwnerPkg();

					if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer()) {
						out << "  " << Pkg.FullName(true) << " " << I.OwnerVer().VerStr();
						if (Cache[Pkg].Install() == true && Cache[Pkg].NewInstall() == false)
							out << _(" [Installed]");
						out << endl;
						++provider;
					}
				}
				// if we found no candidate which provide this package, show non-candidates
				if (provider == 0)
					for (I = Pkg.ProvidesList(); I.end() == false; I++)
						out << "  " << I.OwnerPkg().FullName(true) << " " << I.OwnerVer().VerStr()
						    << _(" [Not candidate version]") << endl;
				else
					out << _("You should explicitly select one to install.") << endl;
			} else {
				ioprintf(out,
					_("Package %s is not available, but is referred to by another package.\n"
					  "This may mean that the package is missing, has been obsoleted, or\n"
					  "is only available from another source\n"),Pkg.FullName(true).c_str());

				string List;
				string VersionsList;
				SPtrArray<bool> Seen = new bool[Cache.GetPkgCache()->Head().PackageCount];
				memset(Seen,0,Cache.GetPkgCache()->Head().PackageCount*sizeof(*Seen));
				for (pkgCache::DepIterator Dep = Pkg.RevDependsList();
				     Dep.end() == false; Dep++) {
					if (Dep->Type != pkgCache::Dep::Replaces)
						continue;
					if (Seen[Dep.ParentPkg()->ID] == true)
						continue;
					Seen[Dep.ParentPkg()->ID] = true;
					List += Dep.ParentPkg().FullName(true) + " ";
					//VersionsList += string(Dep.ParentPkg().CurVersion) + "\n"; ???
				}
				ShowList(out,_("However the following packages replace it:"),List,VersionsList);
			}
			out << std::endl;
		}
		return false;
	}

	virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
		APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, APT::VersionSet::CANDIDATE);
		if (verset.empty() == false)
			return *(verset.begin());
		if (ShowError == true) {
			_error->Error(_("Package '%s' has no installation candidate"),Pkg.FullName(true).c_str());
			virtualPkgs.insert(Pkg);
		}
		return pkgCache::VerIterator(Cache, 0);
	}

	virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
		APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, APT::VersionSet::NEWEST);
		if (verset.empty() == false)
			return *(verset.begin());
		if (ShowError == true)
			ioprintf(out, _("Virtual packages like '%s' can't be removed\n"), Pkg.FullName(true).c_str());
		return pkgCache::VerIterator(Cache, 0);
	}

	APT::VersionSet tryVirtualPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg,
						APT::VersionSet::Version const &select) {
		/* This is a pure virtual package and there is a single available
		   candidate providing it. */
		if (unlikely(Cache[Pkg].CandidateVer != 0) || Pkg->ProvidesList == 0)
			return APT::VersionSet();

		pkgCache::PkgIterator Prov;
		bool found_one = false;
		for (pkgCache::PrvIterator P = Pkg.ProvidesList(); P; ++P) {
			pkgCache::VerIterator const PVer = P.OwnerVer();
			pkgCache::PkgIterator const PPkg = PVer.ParentPkg();

			/* Ignore versions that are not a candidate. */
			if (Cache[PPkg].CandidateVer != PVer)
				continue;

			if (found_one == false) {
				Prov = PPkg;
				found_one = true;
			} else if (PPkg != Prov) {
				found_one = false; // we found at least two
				break;
			}
		}

		if (found_one == true) {
			ioprintf(out, _("Note, selecting '%s' instead of '%s'\n"),
				 Prov.FullName(true).c_str(), Pkg.FullName(true).c_str());
			return APT::VersionSet::FromPackage(Cache, Prov, select, *this);
		}
		return APT::VersionSet();
	}

	inline bool allPkgNamedExplicitly() const { return explicitlyNamed; }

};
									/*}}}*/
// TryToInstall - Mark a package for installation			/*{{{*/
struct TryToInstall {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool FixBroken;
   unsigned long AutoMarkChanged;
   APT::PackageSet doAutoInstallLater;

   TryToInstall(pkgCacheFile &Cache, pkgProblemResolver *PM, bool const &FixBroken) : Cache(&Cache), Fix(PM),
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
   bool FixBroken;
   bool PurgePkgs;
   unsigned long AutoMarkChanged;

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
	 ioprintf(c1out,_("Package %s is not installed, so not removed\n"),Pkg.FullName(true).c_str());
	 // MarkInstall refuses to install packages on hold
	 Pkg->SelectedState = pkgCache::State::Hold;
      }
      else
	 Cache->GetDepCache()->MarkDelete(Pkg, PurgePkgs);
   }
};
									/*}}}*/
// CacheFile::NameComp - QSort compare by name				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache *CacheFile::SortCache = 0;
int CacheFile::NameComp(const void *a,const void *b)
{
   if (*(pkgCache::Package **)a == 0 || *(pkgCache::Package **)b == 0)
      return *(pkgCache::Package **)a - *(pkgCache::Package **)b;
   
   const pkgCache::Package &A = **(pkgCache::Package **)a;
   const pkgCache::Package &B = **(pkgCache::Package **)b;

   return strcmp(SortCache->StrP + A.Name,SortCache->StrP + B.Name);
}
									/*}}}*/
// CacheFile::Sort - Sort by name					/*{{{*/
// ---------------------------------------------------------------------
/* */
void CacheFile::Sort()
{
   delete [] List;
   List = new pkgCache::Package *[Cache->Head().PackageCount];
   memset(List,0,sizeof(*List)*Cache->Head().PackageCount);
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; I++)
      List[I->ID] = I;

   SortCache = *this;
   qsort(List,Cache->Head().PackageCount,sizeof(*List),NameComp);
}
									/*}}}*/
// CacheFile::CheckDeps - Open the cache file				/*{{{*/
// ---------------------------------------------------------------------
/* This routine generates the caches and then opens the dependency cache
   and verifies that the system is OK. */
bool CacheFile::CheckDeps(bool AllowBroken)
{
   bool FixBroken = _config->FindB("APT::Get::Fix-Broken",false);

   if (_error->PendingError() == true)
      return false;

   // Check that the system is OK
   if (DCache->DelCount() != 0 || DCache->InstCount() != 0)
      return _error->Error("Internal error, non-zero counts");
   
   // Apply corrections for half-installed packages
   if (pkgApplyStatus(*DCache) == false)
      return false;
   
   if (_config->FindB("APT::Get::Fix-Policy-Broken",false) == true)
   {
      FixBroken = true;
      if ((DCache->PolicyBrokenCount() > 0))
      {
	 // upgrade all policy-broken packages with ForceImportantDeps=True
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); !I.end(); I++)
	    if ((*DCache)[I].NowPolicyBroken() == true) 
	       DCache->MarkInstall(I,true,0, false, true);
      }
   }

   // Nothing is broken
   if (DCache->BrokenCount() == 0 || AllowBroken == true)
      return true;

   // Attempt to fix broken things
   if (FixBroken == true)
   {
      c1out << _("Correcting dependencies...") << flush;
      if (pkgFixBroken(*DCache) == false || DCache->BrokenCount() != 0)
      {
	 c1out << _(" failed.") << endl;
	 ShowBroken(c1out,*this,true);

	 return _error->Error(_("Unable to correct dependencies"));
      }
      if (pkgMinimizeUpgrade(*DCache) == false)
	 return _error->Error(_("Unable to minimize the upgrade set"));
      
      c1out << _(" Done") << endl;
   }
   else
   {
      c1out << _("You might want to run 'apt-get -f install' to correct these.") << endl;
      ShowBroken(c1out,*this,true);

      return _error->Error(_("Unmet dependencies. Try using -f."));
   }
      
   return true;
}
									/*}}}*/
// CheckAuth - check if each download comes form a trusted source	/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool CheckAuth(pkgAcquire& Fetcher)
{
   string UntrustedList;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd(); ++I)
   {
      if (!(*I)->IsTrusted())
      {
         UntrustedList += string((*I)->ShortDesc()) + " ";
      }
   }

   if (UntrustedList == "")
   {
      return true;
   }
        
   ShowList(c2out,_("WARNING: The following packages cannot be authenticated!"),UntrustedList,"");

   if (_config->FindB("APT::Get::AllowUnauthenticated",false) == true)
   {
      c2out << _("Authentication warning overridden.\n");
      return true;
   }

   if (_config->FindI("quiet",0) < 2
       && _config->FindB("APT::Get::Assume-Yes",false) == false)
   {
      c2out << _("Install these packages without verification [y/N]? ") << flush;
      if (!YnPrompt(false))
         return _error->Error(_("Some packages could not be authenticated"));

      return true;
   }
   else if (_config->FindB("APT::Get::Force-Yes",false) == true)
   {
      return true;
   }

   return _error->Error(_("There are problems and -y was used without --force-yes"));
}
									/*}}}*/
// InstallPackages - Actually download and install the packages		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the informative messages describing what is going to 
   happen and then calls the download routines */
bool InstallPackages(CacheFile &Cache,bool ShwKept,bool Ask = true,
		     bool Safety = true)
{
   if (_config->FindB("APT::Get::Purge",false) == true)
   {
      pkgCache::PkgIterator I = Cache->PkgBegin();
      for (; I.end() == false; I++)
      {
	 if (I.Purge() == false && Cache[I].Mode == pkgDepCache::ModeDelete)
	    Cache->MarkDelete(I,true);
      }
   }
   
   bool Fail = false;
   bool Essential = false;
   
   // Show all the various warning indicators
   ShowDel(c1out,Cache);
   ShowNew(c1out,Cache);
   if (ShwKept == true)
      ShowKept(c1out,Cache);
   Fail |= !ShowHold(c1out,Cache);
   if (_config->FindB("APT::Get::Show-Upgraded",true) == true)
      ShowUpgraded(c1out,Cache);
   Fail |= !ShowDowngraded(c1out,Cache);
   if (_config->FindB("APT::Get::Download-Only",false) == false)
        Essential = !ShowEssential(c1out,Cache);
   Fail |= Essential;
   Stats(c1out,Cache);

   // Sanity check
   if (Cache->BrokenCount() != 0)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, InstallPackages was called with broken packages!"));
   }

   if (Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
       Cache->BadCount() == 0)
      return true;

   // No remove flag
   if (Cache->DelCount() != 0 && _config->FindB("APT::Get::Remove",true) == false)
      return _error->Error(_("Packages need to be removed but remove is disabled."));
       
   // Run the simulator ..
   if (_config->FindB("APT::Get::Simulate") == true)
   {
      pkgSimulate PM(Cache);
      int status_fd = _config->FindI("APT::Status-Fd",-1);
      pkgPackageManager::OrderResult Res = PM.DoInstall(status_fd);
      if (Res == pkgPackageManager::Failed)
	 return false;
      if (Res != pkgPackageManager::Completed)
	 return _error->Error(_("Internal error, Ordering didn't finish"));
      return true;
   }
   
   // Create the text record parser
   pkgRecords Recs(Cache);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   pkgAcquire Fetcher;
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   if (_config->FindB("APT::Get::Print-URIs", false) == true)
   {
      // force a hashsum for compatibility reasons
      _config->CndSet("Acquire::ForceHash", "md5sum");
   }
   else if (Fetcher.Setup(&Stat, _config->FindDir("Dir::Cache::Archives")) == false)
      return false;

   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   
   // Create the package manager and prepare to download
   SPtr<pkgPackageManager> PM= _system->CreatePM(Cache);
   if (PM->GetArchives(&Fetcher,List,&Recs) == false || 
       _error->PendingError() == true)
      return false;

   // Display statistics
   unsigned long long FetchBytes = Fetcher.FetchNeeded();
   unsigned long long FetchPBytes = Fetcher.PartialPresent();
   unsigned long long DebBytes = Fetcher.TotalNeeded();
   if (DebBytes != Cache->DebSize())
   {
      c0out << DebBytes << ',' << Cache->DebSize() << endl;
      c0out << _("How odd.. The sizes didn't match, email apt@packages.debian.org") << endl;
   }
   
   // Number of bytes
   if (DebBytes != FetchBytes)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement strings, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB/%sB of archives.\n"),
	       SizeToStr(FetchBytes).c_str(),SizeToStr(DebBytes).c_str());
   else if (DebBytes != 0)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB of archives.\n"),
	       SizeToStr(DebBytes).c_str());

   // Size delta
   if (Cache->UsrSize() >= 0)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("After this operation, %sB of additional disk space will be used.\n"),
	       SizeToStr(Cache->UsrSize()).c_str());
   else
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("After this operation, %sB disk space will be freed.\n"),
	       SizeToStr(-1*Cache->UsrSize()).c_str());

   if (_error->PendingError() == true)
      return false;

   /* Check for enough free space, but only if we are actually going to
      download */
   if (_config->FindB("APT::Get::Print-URIs") == false &&
       _config->FindB("APT::Get::Download",true) == true)
   {
      struct statvfs Buf;
      string OutputDir = _config->FindDir("Dir::Cache::Archives");
      if (statvfs(OutputDir.c_str(),&Buf) != 0) {
	 if (errno == EOVERFLOW)
	    return _error->WarningE("statvfs",_("Couldn't determine free space in %s"),
				 OutputDir.c_str());
	 else
	    return _error->Errno("statvfs",_("Couldn't determine free space in %s"),
				 OutputDir.c_str());
      } else if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
      {
         struct statfs Stat;
         if (statfs(OutputDir.c_str(),&Stat) != 0
#if HAVE_STRUCT_STATFS_F_TYPE
             || unsigned(Stat.f_type) != RAMFS_MAGIC
#endif
             )
            return _error->Error(_("You don't have enough free space in %s."),
                OutputDir.c_str());
      }
   }
   
   // Fail safe check
   if (_config->FindI("quiet",0) >= 2 ||
       _config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      if (Fail == true && _config->FindB("APT::Get::Force-Yes",false) == false)
	 return _error->Error(_("There are problems and -y was used without --force-yes"));
   }         

   if (Essential == true && Safety == true)
   {
      if (_config->FindB("APT::Get::Trivial-Only",false) == true)
	 return _error->Error(_("Trivial Only specified but this is not a trivial operation."));
      
      const char *Prompt = _("Yes, do as I say!");
      ioprintf(c2out,
	       _("You are about to do something potentially harmful.\n"
		 "To continue type in the phrase '%s'\n"
		 " ?] "),Prompt);
      c2out << flush;
      if (AnalPrompt(Prompt) == false)
      {
	 c2out << _("Abort.") << endl;
	 exit(1);
      }     
   }
   else
   {      
      // Prompt to continue
      if (Ask == true || Fail == true)
      {            
	 if (_config->FindB("APT::Get::Trivial-Only",false) == true)
	    return _error->Error(_("Trivial Only specified but this is not a trivial operation."));
	 
	 if (_config->FindI("quiet",0) < 2 &&
	     _config->FindB("APT::Get::Assume-Yes",false) == false)
	 {
	    c2out << _("Do you want to continue [Y/n]? ") << flush;
	 
	    if (YnPrompt() == false)
	    {
	       c2out << _("Abort.") << endl;
	       exit(1);
	    }     
	 }	 
      }      
   }
   
   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); I++)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      return true;
   }

   if (!CheckAuth(Fetcher))
      return false;

   /* Unlock the dpkg lock if we are not going to be doing an install
      after. */
   if (_config->FindB("APT::Get::Download-Only",false) == true)
      _system->UnLock();
   
   // Run it
   while (1)
   {
      bool Transient = false;
      if (_config->FindB("APT::Get::Download",true) == false)
      {
	 for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd();)
	 {
	    if ((*I)->Local == true)
	    {
	       I++;
	       continue;
	    }

	    // Close the item and check if it was found in cache
	    (*I)->Finished();
	    if ((*I)->Complete == false)
	       Transient = true;
	    
	    // Clear it out of the fetch list
	    delete *I;
	    I = Fetcher.ItemsBegin();
	 }	 
      }
      
      if (Fetcher.Run() == pkgAcquire::Failed)
	 return false;
      
      // Print out errors
      bool Failed = false;
      for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
      {
	 if ((*I)->Status == pkgAcquire::Item::StatDone &&
	     (*I)->Complete == true)
	    continue;
	 
	 if ((*I)->Status == pkgAcquire::Item::StatIdle)
	 {
	    Transient = true;
	    // Failed = true;
	    continue;
	 }

	 fprintf(stderr,_("Failed to fetch %s  %s\n"),(*I)->DescURI().c_str(),
		 (*I)->ErrorText.c_str());
	 Failed = true;
      }

      /* If we are in no download mode and missing files and there were
         'failures' then the user must specify -m. Furthermore, there 
         is no such thing as a transient error in no-download mode! */
      if (Transient == true &&
	  _config->FindB("APT::Get::Download",true) == false)
      {
	 Transient = false;
	 Failed = true;
      }
      
      if (_config->FindB("APT::Get::Download-Only",false) == true)
      {
	 if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
	    return _error->Error(_("Some files failed to download"));
	 c1out << _("Download complete and in download only mode") << endl;
	 return true;
      }
      
      if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
      {
	 return _error->Error(_("Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"));
      }
      
      if (Transient == true && Failed == true)
	 return _error->Error(_("--fix-missing and media swapping is not currently supported"));
      
      // Try to deal with missing package files
      if (Failed == true && PM->FixMissing() == false)
      {
	 cerr << _("Unable to correct missing packages.") << endl;
	 return _error->Error(_("Aborting install."));
      }

      _system->UnLock();
      int status_fd = _config->FindI("APT::Status-Fd",-1);
      pkgPackageManager::OrderResult Res = PM->DoInstall(status_fd);
      if (Res == pkgPackageManager::Failed || _error->PendingError() == true)
	 return false;
      if (Res == pkgPackageManager::Completed)
	 break;
      
      // Reload the fetcher object and loop again for media swapping
      Fetcher.Shutdown();
      if (PM->GetArchives(&Fetcher,List,&Recs) == false)
	 return false;
      
      _system->Lock();
   }

   std::set<std::string> const disappearedPkgs = PM->GetDisappearedPackages();
   if (disappearedPkgs.empty() == true)
      return true;

   string disappear;
   for (std::set<std::string>::const_iterator d = disappearedPkgs.begin();
	d != disappearedPkgs.end(); ++d)
      disappear.append(*d).append(" ");

   ShowList(c1out, P_("The following package disappeared from your system as\n"
	"all files have been overwritten by other packages:",
	"The following packages disappeared from your system as\n"
	"all files have been overwritten by other packages:", disappearedPkgs.size()), disappear, "");
   c0out << _("Note: This is done automatic and on purpose by dpkg.") << std::endl;

   return true;
}
									/*}}}*/
// TryToInstallBuildDep - Try to install a single package		/*{{{*/
// ---------------------------------------------------------------------
/* This used to be inlined in DoInstall, but with the advent of regex package
   name matching it was split out.. */
bool TryToInstallBuildDep(pkgCache::PkgIterator Pkg,pkgCacheFile &Cache,
		  pkgProblemResolver &Fix,bool Remove,bool BrokenFix,
		  bool AllowFail = true)
{
   if (Cache[Pkg].CandidateVer == 0 && Pkg->ProvidesList != 0)
   {
      CacheSetHelperAPTGet helper(c1out);
      helper.showErrors(AllowFail == false);
      pkgCache::VerIterator Ver = helper.canNotFindNewestVer(Cache, Pkg);
      if (Ver.end() == false)
	 Pkg = Ver.ParentPkg();
      else if (helper.showVirtualPackageErrors(Cache) == false)
	 return AllowFail;
   }

   if (Remove == true)
   {
      TryToRemove RemoveAction(Cache, &Fix);
      RemoveAction(Pkg.VersionList());
   } else if (Cache[Pkg].CandidateVer != 0) {
      TryToInstall InstallAction(Cache, &Fix, BrokenFix);
      InstallAction(Cache[Pkg].CandidateVerIter(Cache));
      InstallAction.doAutoInstall();
   } else
      return AllowFail;

   return true;
}
									/*}}}*/
// FindSrc - Find a source record					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSrcRecords::Parser *FindSrc(const char *Name,pkgRecords &Recs,
			       pkgSrcRecords &SrcRecs,string &Src,
			       pkgDepCache &Cache)
{
   string VerTag;
   string DefRel = _config->Find("APT::Default-Release");
   string TmpSrc = Name;

   // extract the version/release from the pkgname
   const size_t found = TmpSrc.find_last_of("/=");
   if (found != string::npos) {
      if (TmpSrc[found] == '/')
	 DefRel = TmpSrc.substr(found+1);
      else
	 VerTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }

   /* Lookup the version of the package we would install if we were to
      install a version and determine the source package name, then look
      in the archive for a source package of the same name. */
   bool MatchSrcOnly = _config->FindB("APT::Get::Only-Source");
   const pkgCache::PkgIterator Pkg = Cache.FindPkg(TmpSrc);
   if (MatchSrcOnly == false && Pkg.end() == false) 
   {
      if(VerTag.empty() == false || DefRel.empty() == false) 
      {
	 bool fuzzy = false;
	 // we have a default release, try to locate the pkg. we do it like
	 // this because GetCandidateVer() will not "downgrade", that means
	 // "apt-get source -t stable apt" won't work on a unstable system
	 for (pkgCache::VerIterator Ver = Pkg.VersionList();; Ver++)
	 {
	    // try first only exact matches, later fuzzy matches
	    if (Ver.end() == true)
	    {
	       if (fuzzy == true)
		  break;
	       fuzzy = true;
	       Ver = Pkg.VersionList();
	       // exit right away from the Pkg.VersionList() loop if we
	       // don't have any versions
	       if (Ver.end() == true)
		  break;
	    }
	    // We match against a concrete version (or a part of this version)
	    if (VerTag.empty() == false &&
		(fuzzy == true || Cache.VS().CmpVersion(VerTag, Ver.VerStr()) != 0) && // exact match
		(fuzzy == false || strncmp(VerTag.c_str(), Ver.VerStr(), VerTag.size()) != 0)) // fuzzy match
	       continue;

	    for (pkgCache::VerFileIterator VF = Ver.FileList();
		 VF.end() == false; VF++) 
	    {
	       /* If this is the status file, and the current version is not the
		  version in the status file (ie it is not installed, or somesuch)
		  then it is not a candidate for installation, ever. This weeds
		  out bogus entries that may be due to config-file states, or
		  other. */
	       if ((VF.File()->Flags & pkgCache::Flag::NotSource) ==
		   pkgCache::Flag::NotSource && Pkg.CurrentVer() != Ver)
		  continue;

	       // or we match against a release
	       if(VerTag.empty() == false ||
		  (VF.File().Archive() != 0 && VF.File().Archive() == DefRel) ||
		  (VF.File().Codename() != 0 && VF.File().Codename() == DefRel)) 
	       {
		  pkgRecords::Parser &Parse = Recs.Lookup(VF);
		  Src = Parse.SourcePkg();
		  // no SourcePkg name, so it is the "binary" name
		  if (Src.empty() == true)
		     Src = TmpSrc;
		  // the Version we have is possibly fuzzy or includes binUploads,
		  // so we use the Version of the SourcePkg (empty if same as package)
		  VerTag = Parse.SourceVer();
		  if (VerTag.empty() == true)
		     VerTag = Ver.VerStr();
		  break;
	       }
	    }
	    if (Src.empty() == false)
	       break;
	 }
	 if (Src.empty() == true) 
	 {
	    // Sources files have no codename information
	    if (VerTag.empty() == true && DefRel.empty() == false) 
	    {
	       _error->Error(_("Ignore unavailable target release '%s' of package '%s'"), DefRel.c_str(), TmpSrc.c_str());
	       return 0;
	    }
	 }
      }
      if (Src.empty() == true)
      {
	 // if we don't have found a fitting package yet so we will
	 // choose a good candidate and proceed with that.
	 // Maybe we will find a source later on with the right VerTag
	 pkgCache::VerIterator Ver = Cache.GetCandidateVer(Pkg);
	 if (Ver.end() == false) 
	 {
	    pkgRecords::Parser &Parse = Recs.Lookup(Ver.FileList());
	    Src = Parse.SourcePkg();
	    if (VerTag.empty() == true)
	       VerTag = Parse.SourceVer();
	 }
      }
   }

   if (Src.empty() == true)
      Src = TmpSrc;
   else 
   {
      /* if we have a source pkg name, make sure to only search
	 for srcpkg names, otherwise apt gets confused if there
	 is a binary package "pkg1" and a source package "pkg1"
	 with the same name but that comes from different packages */
      MatchSrcOnly = true;
      if (Src != TmpSrc) 
      {
	 ioprintf(c1out, _("Picking '%s' as source package instead of '%s'\n"), Src.c_str(), TmpSrc.c_str());
      }
   }

   // The best hit
   pkgSrcRecords::Parser *Last = 0;
   unsigned long Offset = 0;
   string Version;

   /* Iterate over all of the hits, which includes the resulting
      binary packages in the search */
   pkgSrcRecords::Parser *Parse;
   while (true) 
   {
      SrcRecs.Restart();
      while ((Parse = SrcRecs.Find(Src.c_str(), MatchSrcOnly)) != 0) 
      {
	 const string Ver = Parse->Version();

	 // Ignore all versions which doesn't fit
	 if (VerTag.empty() == false &&
	     Cache.VS().CmpVersion(VerTag, Ver) != 0) // exact match
	    continue;

	 // Newer version or an exact match? Save the hit
	 if (Last == 0 || Cache.VS().CmpVersion(Version,Ver) < 0) {
	    Last = Parse;
	    Offset = Parse->Offset();
	    Version = Ver;
	 }

	 // was the version check above an exact match? If so, we don't need to look further
	 if (VerTag.empty() == false && VerTag.size() == Ver.size())
	    break;
      }
      if (Last != 0 || VerTag.empty() == true)
	 break;
      //if (VerTag.empty() == false && Last == 0)
      _error->Error(_("Ignore unavailable version '%s' of package '%s'"), VerTag.c_str(), TmpSrc.c_str());
      return 0;
   }

   if (Last == 0 || Last->Jump(Offset) == false)
      return 0;

   return Last;
}
									/*}}}*/
// DoUpdate - Update the package lists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoUpdate(CommandLine &CmdL)
{
   if (CmdL.FileSize() != 1)
      return _error->Error(_("The update command takes no arguments"));

   CacheFile Cache;

   // Get the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();

   // Create the progress
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));
      
   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      // force a hashsum for compatibility reasons
      _config->CndSet("Acquire::ForceHash", "md5sum");

      // get a fetcher
      pkgAcquire Fetcher;
      if (Fetcher.Setup(&Stat) == false)
	 return false;

      // Populate it with the source selection and get all Indexes 
      // (GetAll=true)
      if (List->GetIndexes(&Fetcher,true) == false)
	 return false;

      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); I++)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      return true;
   }

   // do the work
   if (_config->FindB("APT::Get::Download",true) == true)
       ListUpdate(Stat, *List);

   // Rebuild the cache.   
   if (Cache.BuildCaches() == false)
      return false;
   
   return true;
}
									/*}}}*/
// DoAutomaticRemove - Remove all automatic unused packages		/*{{{*/
// ---------------------------------------------------------------------
/* Remove unused automatic packages */
bool DoAutomaticRemove(CacheFile &Cache)
{
   bool Debug = _config->FindI("Debug::pkgAutoRemove",false);
   bool doAutoRemove = _config->FindB("APT::Get::AutomaticRemove", false);
   bool hideAutoRemove = _config->FindB("APT::Get::HideAutoRemove");

   pkgDepCache::ActionGroup group(*Cache);
   if(Debug)
      std::cout << "DoAutomaticRemove()" << std::endl;

   if (doAutoRemove == true &&
	_config->FindB("APT::Get::Remove",true) == false)
   {
      c1out << _("We are not supposed to delete stuff, can't start "
		 "AutoRemover") << std::endl;
      return false;
   }

   bool purgePkgs = _config->FindB("APT::Get::Purge", false);
   bool smallList = (hideAutoRemove == false &&
		strcasecmp(_config->Find("APT::Get::HideAutoRemove","").c_str(),"small") == 0);

   string autoremovelist, autoremoveversions;
   unsigned long autoRemoveCount = 0;
   APT::PackageSet tooMuch;
   // look over the cache to see what can be removed
   for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); ! Pkg.end(); ++Pkg)
   {
      if (Cache[Pkg].Garbage)
      {
	 if(Pkg.CurrentVer() != 0 || Cache[Pkg].Install())
	    if(Debug)
	       std::cout << "We could delete %s" <<  Pkg.FullName(true).c_str() << std::endl;

	 if (doAutoRemove)
	 {
	    if(Pkg.CurrentVer() != 0 && 
	       Pkg->CurrentState != pkgCache::State::ConfigFiles)
	       Cache->MarkDelete(Pkg, purgePkgs);
	    else
	       Cache->MarkKeep(Pkg, false, false);
	 }
	 else
	 {
	    // if the package is a new install and already garbage we don't need to
	    // install it in the first place, so nuke it instead of show it
	    if (Cache[Pkg].Install() == true && Pkg.CurrentVer() == 0)
	    {
	       Cache->MarkDelete(Pkg, false);
	       tooMuch.insert(Pkg);
	    }
	    // only show stuff in the list that is not yet marked for removal
	    else if(hideAutoRemove == false && Cache[Pkg].Delete() == false) 
	    {
	       ++autoRemoveCount;
	       // we don't need to fill the strings if we don't need them
	       if (smallList == false)
	       {
		 autoremovelist += Pkg.FullName(true) + " ";
		 autoremoveversions += string(Cache[Pkg].CandVersion) + "\n";
	       }
	    }
	 }
      }
   }

   // we could have removed a new dependency of a garbage package,
   // so check if a reverse depends is broken and if so install it again.
   if (tooMuch.empty() == false && Cache->BrokenCount() != 0)
   {
      bool Changed;
      do {
	 Changed = false;
	 for (APT::PackageSet::const_iterator P = tooMuch.begin();
	      P != tooMuch.end() && Changed == false; ++P)
	 {
	    for (pkgCache::DepIterator R = P.RevDependsList();
		 R.end() == false; ++R)
	    {
	       if (R->Type != pkgCache::Dep::Depends &&
		   R->Type != pkgCache::Dep::PreDepends)
		  continue;
	       pkgCache::PkgIterator N = R.ParentPkg();
	       if (N.end() == true || (N->CurrentVer == 0 && (*Cache)[N].Install() == false))
		  continue;
	       if (Debug == true)
		  std::clog << "Save " << P << " as another installed garbage package depends on it" << std::endl;
	       Cache->MarkInstall(P, false);
	       if(hideAutoRemove == false)
	       {
		  ++autoRemoveCount;
		  if (smallList == false)
		  {
		     autoremovelist += P.FullName(true) + " ";
		     autoremoveversions += string(Cache[P].CandVersion) + "\n";
		  }
	       }
	       tooMuch.erase(P);
	       Changed = true;
	       break;
	    }
	 }
      } while (Changed == true);
   }

   // Now see if we had destroyed anything (if we had done anything)
   if (Cache->BrokenCount() != 0)
   {
      c1out << _("Hmm, seems like the AutoRemover destroyed something which really\n"
	         "shouldn't happen. Please file a bug report against apt.") << endl;
      c1out << endl;
      c1out << _("The following information may help to resolve the situation:") << endl;
      c1out << endl;
      ShowBroken(c1out,Cache,false);

      return _error->Error(_("Internal Error, AutoRemover broke stuff"));
   }

   // if we don't remove them, we should show them!
   if (doAutoRemove == false && (autoremovelist.empty() == false || autoRemoveCount != 0))
   {
      if (smallList == false)
	 ShowList(c1out, P_("The following package was automatically installed and is no longer required:",
	          "The following packages were automatically installed and are no longer required:",
	          autoRemoveCount), autoremovelist, autoremoveversions);
      else
	 ioprintf(c1out, P_("%lu package was automatically installed and is no longer required.\n",
	          "%lu packages were automatically installed and are no longer required.\n", autoRemoveCount), autoRemoveCount);
      c1out << _("Use 'apt-get autoremove' to remove them.") << std::endl;
   }
   return true;
}
									/*}}}*/
// DoUpgrade - Upgrade all packages					/*{{{*/
// ---------------------------------------------------------------------
/* Upgrade all packages without installing new packages or erasing old
   packages */
bool DoUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   // Do the upgrade
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, AllUpgrade broke stuff"));
   }
   
   return InstallPackages(Cache,true);
}
									/*}}}*/
// DoInstall - Install packages from the command line			/*{{{*/
// ---------------------------------------------------------------------
/* Install named packages */
bool DoInstall(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || 
       Cache.CheckDeps(CmdL.FileSize() != 1) == false)
      return false;
   
   // Enter the special broken fixing mode if the user specified arguments
   bool BrokenFix = false;
   if (Cache->BrokenCount() != 0)
      BrokenFix = true;

   pkgProblemResolver* Fix = NULL;
   if (_config->FindB("APT::Get::CallResolver", true) == true)
      Fix = new pkgProblemResolver(Cache);

   static const unsigned short MOD_REMOVE = 1;
   static const unsigned short MOD_INSTALL = 2;

   unsigned short fallback = MOD_INSTALL;
   if (strcasecmp(CmdL.FileList[0],"remove") == 0)
      fallback = MOD_REMOVE;
   else if (strcasecmp(CmdL.FileList[0], "purge") == 0)
   {
      _config->Set("APT::Get::Purge", true);
      fallback = MOD_REMOVE;
   }
   else if (strcasecmp(CmdL.FileList[0], "autoremove") == 0)
   {
      _config->Set("APT::Get::AutomaticRemove", "true");
      fallback = MOD_REMOVE;
   }

   std::list<APT::VersionSet::Modifier> mods;
   mods.push_back(APT::VersionSet::Modifier(MOD_INSTALL, "+",
		APT::VersionSet::Modifier::POSTFIX, APT::VersionSet::CANDIDATE));
   mods.push_back(APT::VersionSet::Modifier(MOD_REMOVE, "-",
		APT::VersionSet::Modifier::POSTFIX, APT::VersionSet::NEWEST));
   CacheSetHelperAPTGet helper(c0out);
   std::map<unsigned short, APT::VersionSet> verset = APT::VersionSet::GroupedFromCommandLine(Cache,
		CmdL.FileList + 1, mods, fallback, helper);

   if (_error->PendingError() == true)
   {
      helper.showVirtualPackageErrors(Cache);
      if (Fix != NULL)
	 delete Fix;
      return false;
   }

   unsigned short const order[] = { MOD_REMOVE, MOD_INSTALL, 0 };

  TryToInstall InstallAction(Cache, Fix, BrokenFix);
  TryToRemove RemoveAction(Cache, Fix);

   // new scope for the ActionGroup
   {
      pkgDepCache::ActionGroup group(Cache);

      for (unsigned short i = 0; order[i] != 0; ++i)
      {
	 if (order[i] == MOD_INSTALL)
	    InstallAction = std::for_each(verset[MOD_INSTALL].begin(), verset[MOD_INSTALL].end(), InstallAction);
	 else if (order[i] == MOD_REMOVE)
	    RemoveAction = std::for_each(verset[MOD_REMOVE].begin(), verset[MOD_REMOVE].end(), RemoveAction);
      }

      if (Fix != NULL && _config->FindB("APT::Get::AutoSolving", true) == true)
      {
         for (unsigned short i = 0; order[i] != 0; ++i)
         {
	    if (order[i] != MOD_INSTALL)
	       continue;
	    InstallAction.propergateReleaseCandiateSwitching(helper.selectedByRelease, c0out);
	    InstallAction.doAutoInstall();
	 }
      }

      if (_error->PendingError() == true)
      {
	 if (Fix != NULL)
	    delete Fix;
	 return false;
      }

      /* If we are in the Broken fixing mode we do not attempt to fix the
	 problems. This is if the user invoked install without -f and gave
	 packages */
      if (BrokenFix == true && Cache->BrokenCount() != 0)
      {
	 c1out << _("You might want to run 'apt-get -f install' to correct these:") << endl;
	 ShowBroken(c1out,Cache,false);
	 if (Fix != NULL)
	    delete Fix;
	 return _error->Error(_("Unmet dependencies. Try 'apt-get -f install' with no packages (or specify a solution)."));
      }

      if (Fix != NULL)
      {
	 // Call the scored problem resolver
	 Fix->InstallProtect();
	 if (Fix->Resolve(true) == false)
	    _error->Discard();
	 delete Fix;
      }

      // Now we check the state of the packages,
      if (Cache->BrokenCount() != 0)
      {
	 c1out << 
	    _("Some packages could not be installed. This may mean that you have\n" 
	      "requested an impossible situation or if you are using the unstable\n" 
	      "distribution that some required packages have not yet been created\n"
	      "or been moved out of Incoming.") << endl;
	 /*
	 if (Packages == 1)
	 {
	    c1out << endl;
	    c1out << 
	       _("Since you only requested a single operation it is extremely likely that\n"
		 "the package is simply not installable and a bug report against\n" 
		 "that package should be filed.") << endl;
	 }
	 */

	 c1out << _("The following information may help to resolve the situation:") << endl;
	 c1out << endl;
	 ShowBroken(c1out,Cache,false);
	 return _error->Error(_("Broken packages"));
      }   
   }
   if (!DoAutomaticRemove(Cache)) 
      return false;

   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   if (Cache->InstCount() != verset[MOD_INSTALL].size())
   {
      string List;
      string VersionsList;
      for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
      {
	 pkgCache::PkgIterator I(Cache,Cache.List[J]);
	 if ((*Cache)[I].Install() == false)
	    continue;
	 pkgCache::VerIterator Cand = Cache[I].CandidateVerIter(Cache);

	 if (verset[MOD_INSTALL].find(Cand) != verset[MOD_INSTALL].end())
	    continue;

	 List += I.FullName(true) + " ";
	 VersionsList += string(Cache[I].CandVersion) + "\n";
      }
      
      ShowList(c1out,_("The following extra packages will be installed:"),List,VersionsList);
   }

   /* Print out a list of suggested and recommended packages */
   {
      string SuggestsList, RecommendsList, List;
      string SuggestsVersions, RecommendsVersions;
      for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
      {
	 pkgCache::PkgIterator Pkg(Cache,Cache.List[J]);

	 /* Just look at the ones we want to install */
	 if ((*Cache)[Pkg].Install() == false)
	   continue;

	 // get the recommends/suggests for the candidate ver
	 pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);
	 for (pkgCache::DepIterator D = CV.DependsList(); D.end() == false; )
	 {
	    pkgCache::DepIterator Start;
	    pkgCache::DepIterator End;
	    D.GlobOr(Start,End); // advances D

	    // FIXME: we really should display a or-group as a or-group to the user
	    //        the problem is that ShowList is incapable of doing this
	    string RecommendsOrList,RecommendsOrVersions;
	    string SuggestsOrList,SuggestsOrVersions;
	    bool foundInstalledInOrGroup = false;
	    for(;;)
	    {
	       /* Skip if package is  installed already, or is about to be */
	       string target = Start.TargetPkg().FullName(true) + " ";
	       pkgCache::PkgIterator const TarPkg = Start.TargetPkg();
	       if (TarPkg->SelectedState == pkgCache::State::Install ||
		   TarPkg->SelectedState == pkgCache::State::Hold ||
		   Cache[Start.TargetPkg()].Install())
	       {
		  foundInstalledInOrGroup=true;
		  break;
	       }

	       /* Skip if we already saw it */
	       if (int(SuggestsList.find(target)) != -1 || int(RecommendsList.find(target)) != -1)
	       {
		  foundInstalledInOrGroup=true;
		  break; 
	       }

	       // this is a dep on a virtual pkg, check if any package that provides it
	       // should be installed
	       if(Start.TargetPkg().ProvidesList() != 0)
	       {
		  pkgCache::PrvIterator I = Start.TargetPkg().ProvidesList();
		  for (; I.end() == false; I++)
		  {
		     pkgCache::PkgIterator Pkg = I.OwnerPkg();
		     if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer() && 
			 Pkg.CurrentVer() != 0)
			foundInstalledInOrGroup=true;
		  }
	       }

	       if (Start->Type == pkgCache::Dep::Suggests) 
	       {
		  SuggestsOrList += target;
		  SuggestsOrVersions += string(Cache[Start.TargetPkg()].CandVersion) + "\n";
	       }
	       
	       if (Start->Type == pkgCache::Dep::Recommends) 
	       {
		  RecommendsOrList += target;
		  RecommendsOrVersions += string(Cache[Start.TargetPkg()].CandVersion) + "\n";
	       }

	       if (Start >= End)
		  break;
	       Start++;
	    }
	    
	    if(foundInstalledInOrGroup == false)
	    {
	       RecommendsList += RecommendsOrList;
	       RecommendsVersions += RecommendsOrVersions;
	       SuggestsList += SuggestsOrList;
	       SuggestsVersions += SuggestsOrVersions;
	    }
	       
	 }
      }

      ShowList(c1out,_("Suggested packages:"),SuggestsList,SuggestsVersions);
      ShowList(c1out,_("Recommended packages:"),RecommendsList,RecommendsVersions);

   }

   // if nothing changed in the cache, but only the automark information
   // we write the StateFile here, otherwise it will be written in 
   // cache.commit()
   if (InstallAction.AutoMarkChanged > 0 &&
       Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
       Cache->BadCount() == 0 &&
       _config->FindB("APT::Get::Simulate",false) == false)
      Cache->writeStateFile(NULL);

   // See if we need to prompt
   // FIXME: check if really the packages in the set are going to be installed
   if (Cache->InstCount() == verset[MOD_INSTALL].size() && Cache->DelCount() == 0)
      return InstallPackages(Cache,false,false);

   return InstallPackages(Cache,false);   
}

/* mark packages as automatically/manually installed. */
bool DoMarkAuto(CommandLine &CmdL)
{
   bool Action = true;
   int AutoMarkChanged = 0;
   OpTextProgress progress;
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;

   if (strcasecmp(CmdL.FileList[0],"markauto") == 0)
      Action = true;
   else if (strcasecmp(CmdL.FileList[0],"unmarkauto") == 0)
      Action = false;

   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      const char *S = *I;
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
      if (Pkg.end() == true) {
         return _error->Error(_("Couldn't find package %s"),S);
      }
      else
      {
         if (!Action)
            ioprintf(c1out,_("%s set to manually installed.\n"), Pkg.Name());
         else
            ioprintf(c1out,_("%s set to automatically installed.\n"),
                      Pkg.Name());

         Cache->MarkAuto(Pkg,Action);
         AutoMarkChanged++;
      }
   }
   if (AutoMarkChanged && ! _config->FindB("APT::Get::Simulate",false))
      return Cache->writeStateFile(NULL);
   return false;
}
									/*}}}*/
// DoDistUpgrade - Automatic smart upgrader				/*{{{*/
// ---------------------------------------------------------------------
/* Intelligent upgrader that will install and remove packages at will */
bool DoDistUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   c0out << _("Calculating upgrade... ") << flush;
   if (pkgDistUpgrade(*Cache) == false)
   {
      c0out << _("Failed") << endl;
      ShowBroken(c1out,Cache,false);
      return false;
   }
   
   c0out << _("Done") << endl;
   
   return InstallPackages(Cache,true);
}
									/*}}}*/
// DoDSelectUpgrade - Do an upgrade by following dselects selections	/*{{{*/
// ---------------------------------------------------------------------
/* Follows dselect's selections */
bool DoDSelectUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;
   
   pkgDepCache::ActionGroup group(Cache);

   // Install everything with the install flag set
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; I++)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,false);
   }

   /* Now install their deps too, if we do this above then order of
      the status file is significant for | groups */
   for (I = Cache->PkgBegin();I.end() != true; I++)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,true);
   }
   
   // Apply erasures now, they override everything else.
   for (I = Cache->PkgBegin();I.end() != true; I++)
   {
      // Remove packages 
      if (I->SelectedState == pkgCache::State::DeInstall ||
	  I->SelectedState == pkgCache::State::Purge)
	 Cache->MarkDelete(I,I->SelectedState == pkgCache::State::Purge);
   }

   /* Resolve any problems that dselect created, allupgrade cannot handle
      such things. We do so quite agressively too.. */
   if (Cache->BrokenCount() != 0)
   {      
      pkgProblemResolver Fix(Cache);

      // Hold back held packages.
      if (_config->FindB("APT::Ignore-Hold",false) == false)
      {
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); I.end() == false; I++)
	 {
	    if (I->SelectedState == pkgCache::State::Hold)
	    {
	       Fix.Protect(I);
	       Cache->MarkKeep(I);
	    }
	 }
      }
   
      if (Fix.Resolve() == false)
      {
	 ShowBroken(c1out,Cache,false);
	 return _error->Error(_("Internal error, problem resolver broke stuff"));
      }
   }

   // Now upgrade everything
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, problem resolver broke stuff"));
   }
   
   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoClean - Remove download archives					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoClean(CommandLine &CmdL)
{
   if (_config->FindB("APT::Get::Simulate") == true)
   {
      cout << "Del " << _config->FindDir("Dir::Cache::archives") << "* " <<
	 _config->FindDir("Dir::Cache::archives") << "partial/*" << endl;
      return true;
   }
   
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
      if (_error->PendingError() == true)
	 return _error->Error(_("Unable to lock the download directory"));
   }
   
   pkgAcquire Fetcher;
   Fetcher.Clean(_config->FindDir("Dir::Cache::archives"));
   Fetcher.Clean(_config->FindDir("Dir::Cache::archives") + "partial/");
   return true;
}
									/*}}}*/
// DoAutoClean - Smartly remove downloaded archives			/*{{{*/
// ---------------------------------------------------------------------
/* This is similar to clean but it only purges things that cannot be 
   downloaded, that is old versions of cached packages. */
class LogCleaner : public pkgArchiveCleaner
{
   protected:
   virtual void Erase(const char *File,string Pkg,string Ver,struct stat &St) 
   {
      c1out << "Del " << Pkg << " " << Ver << " [" << SizeToStr(St.st_size) << "B]" << endl;
      
      if (_config->FindB("APT::Get::Simulate") == false)
	 unlink(File);      
   };
};

bool DoAutoClean(CommandLine &CmdL)
{
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
      if (_error->PendingError() == true)
	 return _error->Error(_("Unable to lock the download directory"));
   }
   
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;
   
   LogCleaner Cleaner;
   
   return Cleaner.Go(_config->FindDir("Dir::Cache::archives"),*Cache) &&
      Cleaner.Go(_config->FindDir("Dir::Cache::archives") + "partial/",*Cache);
}
									/*}}}*/
// DoDownload - download a binary					/*{{{*/
// ---------------------------------------------------------------------
bool DoDownload(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;
   
   APT::CacheSetHelper helper(c0out);
   APT::VersionSet verset = APT::VersionSet::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::VersionSet::CANDIDATE, helper);

   if (verset.empty() == true)
      return false;

   pkgAcquire Fetcher;
   AcqTextStatus Stat(ScreenWidth, _config->FindI("quiet",0));
   if (_config->FindB("APT::Get::Print-URIs") == true)
      Fetcher.Setup(&Stat);

   pkgRecords Recs(Cache);
   pkgSourceList *SrcList = Cache.GetSourceList();
   for (APT::VersionSet::const_iterator Ver = verset.begin(); 
        Ver != verset.end(); 
        ++Ver) 
   {
      string descr;
      // get the right version
      pkgCache::PkgIterator Pkg = Ver.ParentPkg();
      pkgRecords::Parser &rec=Recs.Lookup(Ver.FileList());
      pkgCache::VerFileIterator Vf = Ver.FileList();
      if (Vf.end() == true)
         return _error->Error("Can not find VerFile");
      pkgCache::PkgFileIterator F = Vf.File();
      pkgIndexFile *index;
      if(SrcList->FindIndex(F, index) == false)
         return _error->Error("FindIndex failed");
      string uri = index->ArchiveURI(rec.FileName());
      strprintf(descr, _("Downloading %s %s"), Pkg.Name(), Ver.VerStr());
      // get the most appropriate hash
      HashString hash;
      if (rec.SHA256Hash() != "")
         hash = HashString("sha256", rec.SHA256Hash());
      else if (rec.SHA1Hash() != "")
         hash = HashString("sha1", rec.SHA1Hash());
      else if (rec.MD5Hash() != "")
         hash = HashString("md5", rec.MD5Hash());
      // get the file
      new pkgAcqFile(&Fetcher, uri, hash.toStr(), (*Ver)->Size, descr, Pkg.Name(), ".");
   }

   // Just print out the uris and exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); I++)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      return true;
   }

   return (Fetcher.Run() == pkgAcquire::Continue);
}
									/*}}}*/
// DoCheck - Perform the check operation				/*{{{*/
// ---------------------------------------------------------------------
/* Opening automatically checks the system, this command is mostly used
   for debugging */
bool DoCheck(CommandLine &CmdL)
{
   CacheFile Cache;
   Cache.Open();
   Cache.CheckDeps();
   
   return true;
}
									/*}}}*/
// DoSource - Fetch a source archive					/*{{{*/
// ---------------------------------------------------------------------
/* Fetch souce packages */
struct DscFile
{
   string Package;
   string Version;
   string Dsc;
};

bool DoSource(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open(false) == false)
      return false;

   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to fetch source for"));
   
   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   
   // Create the text record parsers
   pkgRecords Recs(Cache);
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher;
   if (Fetcher.Setup(&Stat) == false)
      return false;

   DscFile *Dsc = new DscFile[CmdL.FileSize()];
   
   // insert all downloaded uris into this set to avoid downloading them
   // twice
   set<string> queued;

   // Diff only mode only fetches .diff files
   bool const diffOnly = _config->FindB("APT::Get::Diff-Only", false);
   // Tar only mode only fetches .tar files
   bool const tarOnly = _config->FindB("APT::Get::Tar-Only", false);
   // Dsc only mode only fetches .dsc files
   bool const dscOnly = _config->FindB("APT::Get::Dsc-Only", false);

   // Load the requestd sources into the fetcher
   unsigned J = 0;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      string Src;
      pkgSrcRecords::Parser *Last = FindSrc(*I,Recs,SrcRecs,Src,*Cache);
      
      if (Last == 0)
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());
      
      string srec = Last->AsStr();
      string::size_type pos = srec.find("\nVcs-");
      while (pos != string::npos)
      {
	 pos += strlen("\nVcs-");
	 string vcs = srec.substr(pos,srec.find(":",pos)-pos);
	 if(vcs == "Browser") 
	 {
	    pos = srec.find("\nVcs-", pos);
	    continue;
	 }
	 pos += vcs.length()+2;
	 string::size_type epos = srec.find("\n", pos);
	 string uri = srec.substr(pos,epos-pos).c_str();
	 ioprintf(c1out, _("NOTICE: '%s' packaging is maintained in "
			   "the '%s' version control system at:\n"
			   "%s\n"),
		  Src.c_str(), vcs.c_str(), uri.c_str());
	 if(vcs == "Bzr") 
	    ioprintf(c1out,_("Please use:\n"
			     "bzr get %s\n"
			     "to retrieve the latest (possibly unreleased) "
			     "updates to the package.\n"),
		     uri.c_str());
	 break;
      }

      // Back track
      vector<pkgSrcRecords::File> Lst;
      if (Last->Files(Lst) == false)
	 return false;

      // Load them into the fetcher
      for (vector<pkgSrcRecords::File>::const_iterator I = Lst.begin();
	   I != Lst.end(); I++)
      {
	 // Try to guess what sort of file it is we are getting.
	 if (I->Type == "dsc")
	 {
	    Dsc[J].Package = Last->Package();
	    Dsc[J].Version = Last->Version();
	    Dsc[J].Dsc = flNotDir(I->Path);
	 }

	 // Handle the only options so that multiple can be used at once
	 if (diffOnly == true || tarOnly == true || dscOnly == true)
	 {
	    if ((diffOnly == true && I->Type == "diff") ||
	        (tarOnly == true && I->Type == "tar") ||
	        (dscOnly == true && I->Type == "dsc"))
		; // Fine, we want this file downloaded
	    else
	       continue;
	 }

	 // don't download the same uri twice (should this be moved to
	 // the fetcher interface itself?)
	 if(queued.find(Last->Index().ArchiveURI(I->Path)) != queued.end())
	    continue;
	 queued.insert(Last->Index().ArchiveURI(I->Path));
	    
	 // check if we have a file with that md5 sum already localy
	 if(!I->MD5Hash.empty() && FileExists(flNotDir(I->Path)))  
	 {
	    FileFd Fd(flNotDir(I->Path), FileFd::ReadOnly);
	    MD5Summation sum;
	    sum.AddFD(Fd.Fd(), Fd.Size());
	    Fd.Close();
	    if((string)sum.Result() == I->MD5Hash) 
	    {
	       ioprintf(c1out,_("Skipping already downloaded file '%s'\n"),
			flNotDir(I->Path).c_str());
	       continue;
	    }
	 }

	 new pkgAcqFile(&Fetcher,Last->Index().ArchiveURI(I->Path),
			I->MD5Hash,I->Size,
			Last->Index().SourceInfo(*Last,*I),Src);
      }
   }
   
   // Display statistics
   unsigned long long FetchBytes = Fetcher.FetchNeeded();
   unsigned long long FetchPBytes = Fetcher.PartialPresent();
   unsigned long long DebBytes = Fetcher.TotalNeeded();

   // Check for enough free space
   struct statvfs Buf;
   string OutputDir = ".";
   if (statvfs(OutputDir.c_str(),&Buf) != 0) {
      if (errno == EOVERFLOW)
	 return _error->WarningE("statvfs",_("Couldn't determine free space in %s"),
				OutputDir.c_str());
      else
	 return _error->Errno("statvfs",_("Couldn't determine free space in %s"),
				OutputDir.c_str());
   } else if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
     {
       struct statfs Stat;
       if (statfs(OutputDir.c_str(),&Stat) != 0
#if HAVE_STRUCT_STATFS_F_TYPE
           || unsigned(Stat.f_type) != RAMFS_MAGIC
#endif
           ) 
          return _error->Error(_("You don't have enough free space in %s"),
              OutputDir.c_str());
      }
   
   // Number of bytes
   if (DebBytes != FetchBytes)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement strings, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB/%sB of source archives.\n"),
	       SizeToStr(FetchBytes).c_str(),SizeToStr(DebBytes).c_str());
   else
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB of source archives.\n"),
	       SizeToStr(DebBytes).c_str());
   
   if (_config->FindB("APT::Get::Simulate",false) == true)
   {
      for (unsigned I = 0; I != J; I++)
	 ioprintf(cout,_("Fetch source %s\n"),Dsc[I].Package.c_str());
      delete[] Dsc;
      return true;
   }
   
   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); I++)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      delete[] Dsc;
      return true;
   }
   
   // Run it
   if (Fetcher.Run() == pkgAcquire::Failed)
      return false;

   // Print error messages
   bool Failed = false;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone &&
	  (*I)->Complete == true)
	 continue;
      
      fprintf(stderr,_("Failed to fetch %s  %s\n"),(*I)->DescURI().c_str(),
	      (*I)->ErrorText.c_str());
      Failed = true;
   }
   if (Failed == true)
      return _error->Error(_("Failed to fetch some archives."));
   
   if (_config->FindB("APT::Get::Download-only",false) == true)
   {
      c1out << _("Download complete and in download only mode") << endl;
      delete[] Dsc;
      return true;
   }

   // Unpack the sources
   pid_t Process = ExecFork();
   
   if (Process == 0)
   {
      bool const fixBroken = _config->FindB("APT::Get::Fix-Broken", false);
      for (unsigned I = 0; I != J; I++)
      {
	 string Dir = Dsc[I].Package + '-' + Cache->VS().UpstreamVersion(Dsc[I].Version.c_str());
	 
	 // Diff only mode only fetches .diff files
	 if (_config->FindB("APT::Get::Diff-Only",false) == true ||
	     _config->FindB("APT::Get::Tar-Only",false) == true ||
	     Dsc[I].Dsc.empty() == true)
	    continue;

	 // See if the package is already unpacked
	 struct stat Stat;
	 if (fixBroken == false && stat(Dir.c_str(),&Stat) == 0 &&
	     S_ISDIR(Stat.st_mode) != 0)
	 {
	    ioprintf(c0out ,_("Skipping unpack of already unpacked source in %s\n"),
			      Dir.c_str());
	 }
	 else
	 {
	    // Call dpkg-source
	    char S[500];
	    snprintf(S,sizeof(S),"%s -x %s",
		     _config->Find("Dir::Bin::dpkg-source","dpkg-source").c_str(),
		     Dsc[I].Dsc.c_str());
	    if (system(S) != 0)
	    {
	       fprintf(stderr,_("Unpack command '%s' failed.\n"),S);
	       fprintf(stderr,_("Check if the 'dpkg-dev' package is installed.\n"));
	       _exit(1);
	    }	    
	 }
	 
	 // Try to compile it with dpkg-buildpackage
	 if (_config->FindB("APT::Get::Compile",false) == true)
	 {
	    // Call dpkg-buildpackage
	    char S[500];
	    snprintf(S,sizeof(S),"cd %s && %s %s",
		     Dir.c_str(),
		     _config->Find("Dir::Bin::dpkg-buildpackage","dpkg-buildpackage").c_str(),
		     _config->Find("DPkg::Build-Options","-b -uc").c_str());
	    
	    if (system(S) != 0)
	    {
	       fprintf(stderr,_("Build command '%s' failed.\n"),S);
	       _exit(1);
	    }	    
	 }      
      }
      
      _exit(0);
   }
   delete[] Dsc;

   // Wait for the subprocess
   int Status = 0;
   while (waitpid(Process,&Status,0) != Process)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }

   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      return _error->Error(_("Child process failed"));
   
   return true;
}
									/*}}}*/
// DoBuildDep - Install/removes packages to satisfy build dependencies  /*{{{*/
// ---------------------------------------------------------------------
/* This function will look at the build depends list of the given source 
   package and install the necessary packages to make it true, or fail. */
bool DoBuildDep(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open(true) == false)
      return false;

   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to check builddeps for"));
   
   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   
   // Create the text record parsers
   pkgRecords Recs(Cache);
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher;
   if (Fetcher.Setup(&Stat) == false)
      return false;

   unsigned J = 0;
   bool const StripMultiArch = APT::Configuration::getArchitectures().size() <= 1;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      string Src;
      pkgSrcRecords::Parser *Last = FindSrc(*I,Recs,SrcRecs,Src,*Cache);
      if (Last == 0)
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());
            
      // Process the build-dependencies
      vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;
      if (Last->BuildDepends(BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch) == false)
      	return _error->Error(_("Unable to get build-dependency information for %s"),Src.c_str());
   
      // Also ensure that build-essential packages are present
      Configuration::Item const *Opts = _config->Tree("APT::Build-Essential");
      if (Opts) 
	 Opts = Opts->Child;
      for (; Opts; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;

         pkgSrcRecords::Parser::BuildDepRec rec;
	 rec.Package = Opts->Value;
	 rec.Type = pkgSrcRecords::Parser::BuildDependIndep;
	 rec.Op = 0;
	 BuildDeps.push_back(rec);
      }

      if (BuildDeps.size() == 0)
      {
	 ioprintf(c1out,_("%s has no build depends.\n"),Src.c_str());
	 continue;
      }
      
      // Install the requested packages
      vector <pkgSrcRecords::Parser::BuildDepRec>::iterator D;
      pkgProblemResolver Fix(Cache);
      bool skipAlternatives = false; // skip remaining alternatives in an or group
      for (D = BuildDeps.begin(); D != BuildDeps.end(); D++)
      {
         bool hasAlternatives = (((*D).Op & pkgCache::Dep::Or) == pkgCache::Dep::Or);

         if (skipAlternatives == true)
         {
            if (!hasAlternatives)
               skipAlternatives = false; // end of or group
            continue;
         }

         if ((*D).Type == pkgSrcRecords::Parser::BuildConflict ||
	     (*D).Type == pkgSrcRecords::Parser::BuildConflictIndep)
         {
            pkgCache::PkgIterator Pkg = Cache->FindPkg((*D).Package);
            // Build-conflicts on unknown packages are silently ignored
            if (Pkg.end() == true)
               continue;

            pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);

            /* 
             * Remove if we have an installed version that satisfies the 
             * version criteria
             */
            if (IV.end() == false && 
                Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
               TryToInstallBuildDep(Pkg,Cache,Fix,true,false);
         }
	 else // BuildDep || BuildDepIndep
         {
	    pkgCache::PkgIterator Pkg = Cache->FindPkg((*D).Package);
            if (_config->FindB("Debug::BuildDeps",false) == true)
                 cout << "Looking for " << (*D).Package << "...\n";

	    if (Pkg.end() == true)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                    cout << " (not found)" << (*D).Package << endl;

               if (hasAlternatives)
                  continue;

               return _error->Error(_("%s dependency for %s cannot be satisfied "
                                      "because the package %s cannot be found"),
                                    Last->BuildDepType((*D).Type),Src.c_str(),
                                    (*D).Package.c_str());
            }

            /*
             * if there are alternatives, we've already picked one, so skip
             * the rest
             *
             * TODO: this means that if there's a build-dep on A|B and B is
             * installed, we'll still try to install A; more importantly,
             * if A is currently broken, we cannot go back and try B. To fix 
             * this would require we do a Resolve cycle for each package we 
             * add to the install list. Ugh
             */
                       
	    /* 
	     * If this is a virtual package, we need to check the list of
	     * packages that provide it and see if any of those are
	     * installed
	     */
            pkgCache::PrvIterator Prv = Pkg.ProvidesList();
            for (; Prv.end() != true; Prv++)
	    {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                    cout << "  Checking provider " << Prv.OwnerPkg().FullName() << endl;

	       if ((*Cache)[Prv.OwnerPkg()].InstVerIter(*Cache).end() == false)
	          break;
            }
            
            // Get installed version and version we are going to install
	    pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);

            if ((*D).Version[0] != '\0') {
                 // Versioned dependency

                 pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);

                 for (; CV.end() != true; CV++)
                 {
                      if (Cache->VS().CheckDep(CV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
                           break;
                 }
                 if (CV.end() == true)
		 {
		   if (hasAlternatives)
		   {
		      continue;
		   }
		   else
		   {
                      return _error->Error(_("%s dependency for %s cannot be satisfied "
                                             "because no available versions of package %s "
                                             "can satisfy version requirements"),
                                           Last->BuildDepType((*D).Type),Src.c_str(),
                                           (*D).Package.c_str());
		   }
		 }
            }
            else
            {
               // Only consider virtual packages if there is no versioned dependency
               if (Prv.end() == false)
               {
                  if (_config->FindB("Debug::BuildDeps",false) == true)
                     cout << "  Is provided by installed package " << Prv.OwnerPkg().FullName() << endl;
                  skipAlternatives = hasAlternatives;
                  continue;
               }
            }

            if (IV.end() == false)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cout << "  Is installed\n";

               if (Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
               {
                  skipAlternatives = hasAlternatives;
                  continue;
               }

               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cout << "    ...but the installed version doesn't meet the version requirement\n";

               if (((*D).Op & pkgCache::Dep::LessEq) == pkgCache::Dep::LessEq)
               {
                  return _error->Error(_("Failed to satisfy %s dependency for %s: Installed package %s is too new"),
                                       Last->BuildDepType((*D).Type),
                                       Src.c_str(),
                                       Pkg.FullName(true).c_str());
               }
            }


            if (_config->FindB("Debug::BuildDeps",false) == true)
               cout << "  Trying to install " << (*D).Package << endl;

            if (TryToInstallBuildDep(Pkg,Cache,Fix,false,false) == true)
            {
               // We successfully installed something; skip remaining alternatives
               skipAlternatives = hasAlternatives;
	       if(_config->FindB("APT::Get::Build-Dep-Automatic", false) == true)
		  Cache->MarkAuto(Pkg, true);
               continue;
            }
            else if (hasAlternatives)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cout << "  Unsatisfiable, trying alternatives\n";
               continue;
            }
            else
            {
               return _error->Error(_("Failed to satisfy %s dependency for %s: %s"),
                                    Last->BuildDepType((*D).Type),
                                    Src.c_str(),
                                    (*D).Package.c_str());
            }
	 }	       
      }
      
      Fix.InstallProtect();
      if (Fix.Resolve(true) == false)
	 _error->Discard();
      
      // Now we check the state of the packages,
      if (Cache->BrokenCount() != 0)
      {
	 ShowBroken(cout, Cache, false);
	 return _error->Error(_("Build-dependencies for %s could not be satisfied."),*I);
      }
   }
  
   if (InstallPackages(Cache, false, true) == false)
      return _error->Error(_("Failed to process build dependencies"));
   return true;
}
									/*}}}*/
// GetChangelogPath - return a path pointing to a changelog file or dir /*{{{*/
// ---------------------------------------------------------------------
/* This returns a "path" string for the changelog url construction.
 * Please note that its not complete, it either needs a "/changelog"
 * appended (for the packages.debian.org/changelogs site) or a
 * ".changelog" (for third party sites that store the changelog in the
 * pool/ next to the deb itself)
 * Example return: "pool/main/a/apt/apt_0.8.8ubuntu3" 
 */
string GetChangelogPath(CacheFile &Cache, 
                        pkgCache::PkgIterator Pkg,
                        pkgCache::VerIterator Ver)
{
   string path;

   pkgRecords Recs(Cache);
   pkgRecords::Parser &rec=Recs.Lookup(Ver.FileList());
   string srcpkg = rec.SourcePkg().empty() ? Pkg.Name() : rec.SourcePkg();
   string ver = Ver.VerStr();
   // if there is a source version it always wins
   if (rec.SourceVer() != "")
      ver = rec.SourceVer();
   path = flNotFile(rec.FileName());
   path += srcpkg + "_" + StripEpoch(ver);
   return path;
}
									/*}}}*/
// GuessThirdPartyChangelogUri - return url 			        /*{{{*/
// ---------------------------------------------------------------------
/* Contruct a changelog file path for third party sites that do not use
 * packages.debian.org/changelogs
 * This simply uses the ArchiveURI() of the source pkg and looks for
 * a .changelog file there, Example for "mediabuntu":
 * apt-get changelog mplayer-doc:
 *  http://packages.medibuntu.org/pool/non-free/m/mplayer/mplayer_1.0~rc4~try1.dsfg1-1ubuntu1+medibuntu1.changelog
 */
bool GuessThirdPartyChangelogUri(CacheFile &Cache, 
                                 pkgCache::PkgIterator Pkg,
                                 pkgCache::VerIterator Ver,
                                 string &out_uri)
{
   // get the binary deb server path
   pkgCache::VerFileIterator Vf = Ver.FileList();
   if (Vf.end() == true)
      return false;
   pkgCache::PkgFileIterator F = Vf.File();
   pkgIndexFile *index;
   pkgSourceList *SrcList = Cache.GetSourceList();
   if(SrcList->FindIndex(F, index) == false)
      return false;

   // get archive uri for the binary deb
   string path_without_dot_changelog = GetChangelogPath(Cache, Pkg, Ver);
   out_uri = index->ArchiveURI(path_without_dot_changelog + ".changelog");

   // now strip away the filename and add srcpkg_srcver.changelog
   return true;
}
									/*}}}*/
// DownloadChangelog - Download the changelog 			        /*{{{*/
// ---------------------------------------------------------------------
bool DownloadChangelog(CacheFile &CacheFile, pkgAcquire &Fetcher, 
                       pkgCache::VerIterator Ver, string targetfile)
/* Download a changelog file for the given package version to
 * targetfile. This will first try the server from Apt::Changelogs::Server
 * (http://packages.debian.org/changelogs by default) and if that gives
 * a 404 tries to get it from the archive directly (see 
 * GuessThirdPartyChangelogUri for details how)
 */
{
   string path;
   string descr;
   string server;
   string changelog_uri;

   // data structures we need
   pkgCache::PkgIterator Pkg = Ver.ParentPkg();

   // make the server root configurable
   server = _config->Find("Apt::Changelogs::Server",
                          "http://packages.debian.org/changelogs");
   path = GetChangelogPath(CacheFile, Pkg, Ver);
   strprintf(changelog_uri, "%s/%s/changelog", server.c_str(), path.c_str());
   if (_config->FindB("APT::Get::Print-URIs", false) == true)
   {
      std::cout << '\'' << changelog_uri << '\'' << std::endl;
      return true;
   }

   strprintf(descr, _("Changelog for %s (%s)"), Pkg.Name(), changelog_uri.c_str());
   // queue it
   new pkgAcqFile(&Fetcher, changelog_uri, "", 0, descr, Pkg.Name(), "ignored", targetfile);

   // try downloading it, if that fails, try third-party-changelogs location
   // FIXME: Fetcher.Run() is "Continue" even if I get a 404?!?
   Fetcher.Run();
   if (!FileExists(targetfile))
   {
      string third_party_uri;
      if (GuessThirdPartyChangelogUri(CacheFile, Pkg, Ver, third_party_uri))
      {
         strprintf(descr, _("Changelog for %s (%s)"), Pkg.Name(), third_party_uri.c_str());
         new pkgAcqFile(&Fetcher, third_party_uri, "", 0, descr, Pkg.Name(), "ignored", targetfile);
         Fetcher.Run();
      }
   }

   if (FileExists(targetfile))
      return true;

   // error
   pkgRecords Recs(CacheFile);
   pkgRecords::Parser &rec=Recs.Lookup(Ver.FileList());
   string srcpkg = rec.SourcePkg().empty() ? Pkg.Name() : rec.SourcePkg();
   return _error->Error("changelog for this version is not (yet) available; try https://launchpad.net/ubuntu/+source/%s/+changelog", srcpkg.c_str());
}
									/*}}}*/
// DisplayFileInPager - Display File with pager        			/*{{{*/
void DisplayFileInPager(string filename)
{
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char *Args[3];
      Args[0] = "/usr/bin/sensible-pager";
      Args[1] = filename.c_str();
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      exit(100);
   }
         
   // Wait for the subprocess
   ExecWait(Process, "sensible-pager", false);
}
									/*}}}*/
// DoChangelog - Get changelog from the command line			/*{{{*/
// ---------------------------------------------------------------------
bool DoChangelog(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;
   
   APT::CacheSetHelper helper(c0out);
   APT::VersionSet verset = APT::VersionSet::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::VersionSet::CANDIDATE, helper);
   if (verset.empty() == true)
      return false;
   pkgAcquire Fetcher;

   if (_config->FindB("APT::Get::Print-URIs", false) == true)
      for (APT::VersionSet::const_iterator Ver = verset.begin();
	   Ver != verset.end(); ++Ver)
	 return DownloadChangelog(Cache, Fetcher, Ver, "");

   AcqTextStatus Stat(ScreenWidth, _config->FindI("quiet",0));
   Fetcher.Setup(&Stat);

   bool const downOnly = _config->FindB("APT::Get::Download-Only", false);

   char tmpname[100];
   char* tmpdir = NULL;
   if (downOnly == false)
   {
      const char* const tmpDir = getenv("TMPDIR");
      if (tmpDir != NULL && *tmpDir != '\0')
	 snprintf(tmpname, sizeof(tmpname), "%s/apt-changelog-XXXXXX", tmpDir);
      else
	 strncpy(tmpname, "/tmp/apt-changelog-XXXXXX", sizeof(tmpname));
      tmpdir = mkdtemp(tmpname);
      if (tmpdir == NULL)
	 return _error->Errno("mkdtemp", "mkdtemp failed");
   }

   for (APT::VersionSet::const_iterator Ver = verset.begin(); 
        Ver != verset.end(); 
        ++Ver) 
   {
      string changelogfile;
      if (downOnly == false)
	 changelogfile.append(tmpname).append("changelog");
      else
	 changelogfile.append(Ver.ParentPkg().Name()).append(".changelog");
      if (DownloadChangelog(Cache, Fetcher, Ver, changelogfile) && downOnly == false)
      {
         DisplayFileInPager(changelogfile);
         // cleanup temp file
         unlink(changelogfile.c_str());
      }
   }
   // clenaup tmp dir
   if (tmpdir != NULL)
      rmdir(tmpdir);
   return true;
}
									/*}}}*/
// DoMoo - Never Ask, Never Tell					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoMoo(CommandLine &CmdL)
{
   cout << 
      "         (__) \n"
      "         (oo) \n"
      "   /------\\/ \n"
      "  / |    ||   \n" 
      " *  /\\---/\\ \n"
      "    ~~   ~~   \n"
      "....\"Have you mooed today?\"...\n";
			    
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
	    
   if (_config->FindB("version") == true)
   {
      cout << _("Supported modules:") << endl;
      
      for (unsigned I = 0; I != pkgVersioningSystem::GlobalListLen; I++)
      {
	 pkgVersioningSystem *VS = pkgVersioningSystem::GlobalList[I];
	 if (_system != 0 && _system->VS == VS)
	    cout << '*';
	 else
	    cout << ' ';
	 cout << "Ver: " << VS->Label << endl;
	 
	 /* Print out all the packaging systems that will work with 
	    this VS */
	 for (unsigned J = 0; J != pkgSystem::GlobalListLen; J++)
	 {
	    pkgSystem *Sys = pkgSystem::GlobalList[J];
	    if (_system == Sys)
	       cout << '*';
	    else
	       cout << ' ';
	    if (Sys->VS->TestCompatibility(*VS) == true)
	       cout << "Pkg:  " << Sys->Label << " (Priority " << Sys->Score(*_config) << ")" << endl;
	 }
      }
      
      for (unsigned I = 0; I != pkgSourceList::Type::GlobalListLen; I++)
      {
	 pkgSourceList::Type *Type = pkgSourceList::Type::GlobalList[I];
	 cout << " S.L: '" << Type->Name << "' " << Type->Label << endl;
      }      
      
      for (unsigned I = 0; I != pkgIndexFile::Type::GlobalListLen; I++)
      {
	 pkgIndexFile::Type *Type = pkgIndexFile::Type::GlobalList[I];
	 cout << " Idx: " << Type->Label << endl;
      }      
      
      return true;
   }
   
   cout << 
    _("Usage: apt-get [options] command\n"
      "       apt-get [options] install|remove pkg1 [pkg2 ...]\n"
      "       apt-get [options] source pkg1 [pkg2 ...]\n"
      "\n"
      "apt-get is a simple command line interface for downloading and\n"
      "installing packages. The most frequently used commands are update\n"
      "and install.\n"   
      "\n"
      "Commands:\n"
      "   update - Retrieve new lists of packages\n"
      "   upgrade - Perform an upgrade\n"
      "   install - Install new packages (pkg is libc6 not libc6.deb)\n"
      "   remove - Remove packages\n"
      "   autoremove - Remove automatically all unused packages\n"
      "   purge - Remove packages and config files\n"
      "   source - Download source archives\n"
      "   build-dep - Configure build-dependencies for source packages\n"
      "   dist-upgrade - Distribution upgrade, see apt-get(8)\n"
      "   dselect-upgrade - Follow dselect selections\n"
      "   clean - Erase downloaded archive files\n"
      "   autoclean - Erase old downloaded archive files\n"
      "   check - Verify that there are no broken dependencies\n"
      "   markauto - Mark the given packages as automatically installed\n"
      "   unmarkauto - Mark the given packages as manually installed\n"
      "   changelog - Download and display the changelog for the given package\n"
      "   download - Download the binary package into the current directory\n"
      "\n"
      "Options:\n"
      "  -h  This help text.\n"
      "  -q  Loggable output - no progress indicator\n"
      "  -qq No output except for errors\n"
      "  -d  Download only - do NOT install or unpack archives\n"
      "  -s  No-act. Perform ordering simulation\n"
      "  -y  Assume Yes to all queries and do not prompt\n"
      "  -f  Attempt to correct a system with broken dependencies in place\n"
      "  -m  Attempt to continue if archives are unlocatable\n"
      "  -u  Show a list of upgraded packages as well\n"
      "  -b  Build the source package after fetching it\n"
      "  -V  Show verbose version numbers\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See the apt-get(8), sources.list(5) and apt.conf(5) manual\n"
      "pages for more information and options.\n"
      "                       This APT has Super Cow Powers.\n");
   return true;
}
									/*}}}*/
// SigWinch - Window size change signal handler				/*{{{*/
// ---------------------------------------------------------------------
/* */
void SigWinch(int)
{
   // Riped from GNU ls
#ifdef TIOCGWINSZ
   struct winsize ws;
  
   if (ioctl(1, TIOCGWINSZ, &ws) != -1 && ws.ws_col >= 5)
      ScreenWidth = ws.ws_col - 1;
#endif
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'V',"verbose-versions","APT::Get::Show-Versions",0},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'q',"silent","quiet",CommandLine::IntLevel},
      {'d',"download-only","APT::Get::Download-Only",0},
      {'b',"compile","APT::Get::Compile",0},
      {'b',"build","APT::Get::Compile",0},
      {'s',"simulate","APT::Get::Simulate",0},
      {'s',"just-print","APT::Get::Simulate",0},
      {'s',"recon","APT::Get::Simulate",0},
      {'s',"dry-run","APT::Get::Simulate",0},
      {'s',"no-act","APT::Get::Simulate",0},
      {'y',"yes","APT::Get::Assume-Yes",0},
      {'y',"assume-yes","APT::Get::Assume-Yes",0},      
      {'f',"fix-broken","APT::Get::Fix-Broken",0},
      {'u',"show-upgraded","APT::Get::Show-Upgraded",0},
      {'m',"ignore-missing","APT::Get::Fix-Missing",0},
      {'t',"target-release","APT::Default-Release",CommandLine::HasArg},
      {'t',"default-release","APT::Default-Release",CommandLine::HasArg},
      {0,"download","APT::Get::Download",0},
      {0,"fix-missing","APT::Get::Fix-Missing",0},
      {0,"ignore-hold","APT::Ignore-Hold",0},      
      {0,"upgrade","APT::Get::upgrade",0},
      {0,"only-upgrade","APT::Get::Only-Upgrade",0},
      {0,"force-yes","APT::Get::force-yes",0},
      {0,"print-uris","APT::Get::Print-URIs",0},
      {0,"diff-only","APT::Get::Diff-Only",0},
      {0,"debian-only","APT::Get::Diff-Only",0},
      {0,"tar-only","APT::Get::Tar-Only",0},
      {0,"dsc-only","APT::Get::Dsc-Only",0},
      {0,"purge","APT::Get::Purge",0},
      {0,"list-cleanup","APT::Get::List-Cleanup",0},
      {0,"reinstall","APT::Get::ReInstall",0},
      {0,"trivial-only","APT::Get::Trivial-Only",0},
      {0,"remove","APT::Get::Remove",0},
      {0,"only-source","APT::Get::Only-Source",0},
      {0,"arch-only","APT::Get::Arch-Only",0},
      {0,"auto-remove","APT::Get::AutomaticRemove",0},
      {0,"allow-unauthenticated","APT::Get::AllowUnauthenticated",0},
      {0,"install-recommends","APT::Install-Recommends",CommandLine::Boolean},
      {0,"install-suggests","APT::Install-Suggests",CommandLine::Boolean},
      {0,"fix-policy","APT::Get::Fix-Policy-Broken",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {{"update",&DoUpdate},
                                   {"upgrade",&DoUpgrade},
                                   {"install",&DoInstall},
                                   {"remove",&DoInstall},
                                   {"purge",&DoInstall},
				   {"autoremove",&DoInstall},
				   {"markauto",&DoMarkAuto},
				   {"unmarkauto",&DoMarkAuto},
                                   {"dist-upgrade",&DoDistUpgrade},
                                   {"dselect-upgrade",&DoDSelectUpgrade},
				   {"build-dep",&DoBuildDep},
                                   {"clean",&DoClean},
                                   {"autoclean",&DoAutoClean},
                                   {"check",&DoCheck},
				   {"source",&DoSource},
                                   {"download",&DoDownload},
                                   {"changelog",&DoChangelog},
				   {"moo",&DoMoo},
				   {"help",&ShowHelp},
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

   // simulate user-friendly if apt-get has no root privileges
   if (getuid() != 0 && _config->FindB("APT::Get::Simulate") == true &&
	(CmdL.FileSize() == 0 ||
	 (strcmp(CmdL.FileList[0], "source") != 0 && strcmp(CmdL.FileList[0], "download") != 0 &&
	  strcmp(CmdL.FileList[0], "changelog") != 0)))
   {
      if (_config->FindB("APT::Get::Show-User-Simulation-Note",true) == true)
	 cout << _("NOTE: This is only a simulation!\n"
	    "      apt-get needs root privileges for real execution.\n"
	    "      Keep also in mind that locking is deactivated,\n"
	    "      so don't depend on the relevance to the real current situation!"
	 ) << std::endl;
      _config->Set("Debug::NoLocking",true);
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

   // Setup the signals
   signal(SIGPIPE,SIG_IGN);
   signal(SIGWINCH,SigWinch);
   SigWinch(0);

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
