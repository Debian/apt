// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-get.cc,v 1.5 1998/10/24 04:58:08 jgg Exp $
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
#include <apt-pkg/error.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/init.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/algorithms.h>

#include <config.h>

#include <fstream.h>
									/*}}}*/

ostream c0out;
ostream c1out;
ostream c2out;
ofstream devnull("/dev/null");
unsigned int ScreenWidth = 80;

// ShowList - Show a list						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a string of space seperated words with a title and 
   a two space indent line wraped to the current screen width. */
void ShowList(ostream &out,string Title,string List)
{
   if (List.empty() == true)
      return;

   // Acount for the leading space
   int ScreenWidth = ::ScreenWidth - 3;
      
   out << Title << endl;
   string::size_type Start = 0;
   while (Start < List.size())
   {
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
									/*}}}*/
// ShowBroken - Debugging aide						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
   with the name of each each broken dependency and a quite version 
   description. */
void ShowBroken(ostream &out,pkgDepCache &Cache)
{
   out << "Sorry, but the following packages are broken - this means they have unmet" << endl;
   out << "dependencies:" << endl;
   pkgCache::PkgIterator I = Cache.PkgBegin();
   for (;I.end() != true; I++)
   {
      if (Cache[I].InstBroken() == false)
	  continue;
	  
      // Print out each package and the failed dependencies
      out <<"  " <<  I.Name() << ":";
      int Indent = strlen(I.Name()) + 3;
      bool First = true;
      if (Cache[I].InstVerIter(Cache).end() == true)
      {
	 cout << endl;
	 continue;
      }
      
      for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList(); D.end() == false; D++)
      {
	 if (Cache.IsImportantDep(D) == false || (Cache[D] &
						  pkgDepCache::DepInstall) != 0)
	    continue;
	 
	 if (First == false)
	    for (int J = 0; J != Indent; J++)
	       out << ' ';
	 First = false;
	 
	 if (D->Type == pkgCache::Dep::Conflicts)
	    out << " Conflicts:" << D.TargetPkg().Name();
	 else
	    out << " Depends:" << D.TargetPkg().Name();
	 
	 // Show a quick summary of the version requirements
	 if (D.TargetVer() != 0)
	    out << " (" << D.CompType() << " " << D.TargetVer() << 
	    ")";
	 
	 /* Show a summary of the target package if possible. In the case
	  of virtual packages we show nothing */
	 
	 pkgCache::PkgIterator Targ = D.TargetPkg();
	 if (Targ->ProvidesList == 0)
	 {
	    out << " but ";
	    pkgCache::VerIterator Ver = Cache[Targ].InstVerIter(Cache);
	    if (Ver.end() == false)
	       out << Ver.VerStr() << " is installed";
	    else
	    {
	       if (Cache[Targ].CandidateVerIter(Cache).end() == true)
	       {
		  if (Targ->ProvidesList == 0)
		     out << "it is not installable";
		  else
		     out << "it is a virtual package";
	       }		  
	       else
		  out << "it is not installed";
	    }	       
	 }
	 
	 out << endl;
      }	    
   }   
}
									/*}}}*/
// ShowNew - Show packages to newly install				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowNew(ostream &out,pkgDepCache &Dep)
{
   /* Print out a list of packages that are going to be removed extra
      to what the user asked */
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   for (;I.end() != true; I++)
      if (Dep[I].NewInstall() == true)
	 List += string(I.Name()) + " ";
   ShowList(out,"The following NEW packages will be installed:",List);
}
									/*}}}*/
// ShowDel - Show packages to delete					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowDel(ostream &out,pkgDepCache &Dep)
{
   /* Print out a list of packages that are going to be removed extra
      to what the user asked */
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   for (;I.end() != true; I++)
      if (Dep[I].Delete() == true)
	 List += string(I.Name()) + " ";
   ShowList(out,"The following packages will be REMOVED:",List);
}
									/*}}}*/
// ShowKept - Show kept packages					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowKept(ostream &out,pkgDepCache &Dep)
{
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   for (;I.end() != true; I++)
   {	 
      // Not interesting
      if (Dep[I].Upgrade() == true || Dep[I].Upgradable() == false ||
	  I->CurrentVer == 0 || Dep[I].Delete() == true)
	 continue;
      
      List += string(I.Name()) + " ";
   }
   ShowList(out,"The following packages have been kept back",List);
}
									/*}}}*/
// ShowUpgraded - Show upgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowUpgraded(ostream &out,pkgDepCache &Dep)
{
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   for (;I.end() != true; I++)
   {
      // Not interesting
      if (Dep[I].Upgrade() == false || Dep[I].NewInstall() == true)
	 continue;
      
      List += string(I.Name()) + " ";
   }
   ShowList(out,"The following packages will be upgraded",List);
}
									/*}}}*/
// ShowHold - Show held but changed packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowHold(ostream &out,pkgDepCache &Dep)
{
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   for (;I.end() != true; I++)
   {
      if (Dep[I].InstallVer != (pkgCache::Version *)I.CurrentVer() &&
	  I->SelectedState == pkgCache::State::Hold)
	 List += string(I.Name()) + " ";
   }

   ShowList(out,"The following held packages will be changed:",List);
}
									/*}}}*/
// ShowEssential - Show an essential package warning			/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a warning message that is not to be ignored. It shows
   all essential packages and their dependents that are to be removed. 
   It is insanely risky to remove the dependents of an essential package! */
void ShowEssential(ostream &out,pkgDepCache &Dep)
{
   pkgCache::PkgIterator I = Dep.PkgBegin();
   string List;
   bool *Added = new bool[Dep.HeaderP->PackageCount];
   for (unsigned int I = 0; I != Dep.HeaderP->PackageCount; I++)
      Added[I] = false;
   
   for (;I.end() != true; I++)
   {
      if ((I->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential)
	 continue;
      
      // The essential package is being removed
      if (Dep[I].Delete() == true)
      {
	 if (Added[I->ID] == false)
	 {
	    Added[I->ID] = true;
	    List += string(I.Name()) + " ";
	 }
      }
      
      if (I->CurrentVer == 0)
	 continue;

      // Print out any essential package depenendents that are to be removed
      for (pkgDepCache::DepIterator D = I.CurrentVer().DependsList(); D.end() == false; D++)
      {
	 pkgCache::PkgIterator P = D.SmartTargetPkg();
	 if (Dep[P].Delete() == true)
	 {
	    if (Added[P->ID] == true)
	       continue;
	    Added[P->ID] = true;
	    List += string(P.Name()) + " ";
	 }	 
      }      
   }
   
   if (List.empty() == false)
      out << "WARNING: The following essential packages will be removed" << endl;
   ShowList(out,"This should NOT be done unless you know exactly what you are doing!",List);

   delete [] Added;
}
									/*}}}*/
// Stats - Show some statistics						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Stats(ostream &out,pkgDepCache &Dep)
{
   unsigned long Upgrade = 0;
   unsigned long Install = 0;
   for (pkgCache::PkgIterator I = Dep.PkgBegin(); I.end() == false; I++)
   {
      if (Dep[I].NewInstall() == true)
	 Install++;
      else
	 if (Dep[I].Upgrade() == true)
	    Upgrade++;
   }   

   out << Upgrade << " packages upgraded, " << 
      Install << " newly installed, " <<
      Dep.DelCount() << " to remove and " << 
      Dep.KeepCount() << " not upgraded." << endl;

   if (Dep.BadCount() != 0)
      out << Dep.BadCount() << " packages not fully installed or removed." << endl;
}
									/*}}}*/

// class CacheFile - Cover class for some dependency cache functions	/*{{{*/
// ---------------------------------------------------------------------
/* */
class CacheFile
{
   public:
   
   FileFd *File;
   MMap *Map;
   pkgDepCache *Cache;
   
   inline operator pkgDepCache &() {return *Cache;};
   inline pkgDepCache *operator ->() {return Cache;};
   inline pkgDepCache &operator *() {return *Cache;};
   
   bool Open();
   CacheFile() : File(0), Map(0), Cache(0) {};
   ~CacheFile()
   {
      delete Cache;
      delete Map;
      delete File;
   }   
};
									/*}}}*/
// CacheFile::Open - Open the cache file				/*{{{*/
// ---------------------------------------------------------------------
/* This routine generates the caches and then opens the dependency cache
   and verifies that the system is OK. */
bool CacheFile::Open()
{
   // Create a progress class
   OpTextProgress Progress(*_config);
      
   // Read the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return _error->Error("The list of sources could not be read.");
   
   // Build all of the caches
   pkgMakeStatusCache(List,Progress);
   if (_error->PendingError() == true)
      return _error->Error("The package lists or status file could not be parsed or opened.");
   
   Progress.Done();
   
   // Open the cache file
   File = new FileFd(_config->FindFile("Dir::Cache::pkgcache"),FileFd::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   Map = new MMap(*File,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   Cache = new pkgDepCache(*Map,Progress);
   if (_error->PendingError() == true)
      return false;

   Progress.Done();
   
   // Check that the system is OK
   if (Cache->DelCount() != 0 || Cache->InstCount() != 0)
      return _error->Error("Internal Error, non-zero counts");
   
   // Apply corrections for half-installed packages
   if (pkgApplyStatus(*Cache) == false)
      return false;
   
   // Nothing is broken
   if (Cache->BrokenCount() == 0)
      return true;

   // Attempt to fix broken things
   if (_config->FindB("APT::Get::Fix-Broken",false) == true)
   {
      c1out << "Correcting dependencies..." << flush;
      if (pkgFixBroken(*Cache) == false || Cache->BrokenCount() != 0)
      {
	 c1out << " failed." << endl;
	 ShowBroken(c1out,*this);

	 return _error->Error("Unable to correct dependencies");
      }
      if (pkgMinimizeUpgrade(*Cache) == false)
	 return _error->Error("Unable to minimize the upgrade set");
      
      c1out << " Done" << endl;
   }
   else
   {
      c1out << "You might want to run `apt-get -f install' to correct these." << endl;
      ShowBroken(c1out,*this);

      return _error->Error("Unmet dependencies. Try using -f.");
   }
      
   return true;
}
									/*}}}*/

// InstallPackages - Actually download and install the packages		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the informative messages describing what is going to 
   happen and then calls the download routines */
bool InstallPackages(pkgDepCache &Cache,bool ShwKept)
{
   ShowDel(c1out,Cache);
   ShowNew(c1out,Cache);
   if (ShwKept == true)
      ShowKept(c1out,Cache);
   ShowHold(c1out,Cache);
   if (_config->FindB("APT::Get::Show-Upgraded",false) == true)
      ShowUpgraded(c1out,Cache);
   ShowEssential(c1out,Cache);
   Stats(c1out,Cache);
   
   // Sanity check
   if (Cache.BrokenCount() != 0)
   {
      ShowBroken(c1out,Cache);
      return _error->Error("Internal Error, InstallPackages was called with broken packages!");
   }

   if (Cache.DelCount() == 0 && Cache.InstCount() == 0 && 
       Cache.BadCount() == 0)
      return true;   
      
   return true;
}
									/*}}}*/

// DoUpdate - Update the package lists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoUpdate(CommandLine &CmdL)
{
}
									/*}}}*/
// DoUpgrade - Upgrade all packages					/*{{{*/
// ---------------------------------------------------------------------
/* Upgrade all packages without installing new packages or erasing old
   packages */
bool DoUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;

   // Do the upgrade
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache);
      return _error->Error("Internal Error, AllUpgrade broke stuff");
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
   if (Cache.Open() == false)
      return false;
   
   int ExpectedInst = 0;
   int Packages = 0;
   pkgProblemResolver Fix(Cache);
   
   bool DefRemove = false;
   if (strcasecmp(CmdL.FileList[0],"remove") == 0)
      DefRemove = true;
   
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      // Duplicate the string
      unsigned int Length = strlen(*I);
      char S[300];
      if (Length >= sizeof(S))
	 continue;
      strcpy(S,*I);
      
      // See if we are removing the package
      bool Remove = DefRemove;
      if (S[Length - 1] == '-')
      {
	 Remove = true;
	 S[--Length] = 0;
      }
      if (S[Length - 1] == '+')
      {
	 Remove = false;
	 S[--Length] = 0;
      }
      
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
      Packages++;
      if (Pkg.end() == true)
	 return _error->Error("Couldn't find package %s",S);
      
      // Check if there is something new to install
      pkgDepCache::StateCache &State = (*Cache)[Pkg];
      if (State.CandidateVer == 0)
      {
	 if (Pkg->ProvidesList != 0)
	 {
	    c1out << "Package " << S << " is a virtual package provided by:" << endl;

	    pkgCache::PrvIterator I = Pkg.ProvidesList();
	    for (; I.end() == false; I++)
	    {
	       pkgCache::PkgIterator Pkg = I.OwnerPkg();
	       
	       if ((*Cache)[Pkg].CandidateVerIter(*Cache) == I.OwnerVer())
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() << endl;

	       if ((*Cache)[Pkg].InstVerIter(*Cache) == I.OwnerVer())
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() <<
		    " [Installed]"<< endl;
	    }
	    c1out << "You should explicly select one to install." << endl;
	 }
	 else
	 {
	    c1out << "Package " << S << " has no available version, but exists in the database." << endl;
	    c1out << "This typically means that the package was mentioned in a dependency and " << endl;
	    c1out << "never uploaded, or that it is an obsolete package." << endl;
	 }
	 
	 return _error->Error("Package %s has no installation candidate",S);
      }
      
      Fix.Protect(Pkg);
      if (Remove == true)
      {
	 Fix.Remove(Pkg);
	 Cache->MarkDelete(Pkg);
	 continue;
      }
      
      // Install it
      Cache->MarkInstall(Pkg,false);
      if (State.Install() == false)
	 c1out << "Sorry, " << S << " is already the newest version"  << endl;
      else
	 ExpectedInst++;

      // Install it with autoinstalling enabled.
      if (State.InstBroken() == true)
	 Cache->MarkInstall(Pkg,true);
   }

   // Call the scored problem resolver
   Fix.InstallProtect();
   if (Fix.Resolve(true) == false)
      _error->Discard();

   // Now we check the state of the packages,
   if (Cache->BrokenCount() != 0)
   {
      c1out << "Some packages could not be installed. This may mean that you have" << endl;
      c1out << "requested an impossible situation or if you are using the unstable" << endl;
      c1out << "distribution that some required packages have not yet been created" << endl;
      c1out << "or been moved out of Incoming." << endl;
      if (Packages == 1)
      {
	 c1out << endl;
	 c1out << "Since you only requested a single operation it is extremely likely that" << endl;
	 c1out << "the package is simply not installable and a bug report against" << endl;
	 c1out << "that package should be filed." << endl;
      }

      c1out << "The following information may help to resolve the situation:" << endl;
      c1out << endl;
      ShowBroken(c1out,Cache);
      return _error->Error("Sorry, broken packages");
   }   
   
   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   if (Cache->InstCount() != ExpectedInst)
   {
      string List;
      pkgCache::PkgIterator I = Cache->PkgBegin();
      for (;I.end() != true; I++)
      {
	 if ((*Cache)[I].Install() == false)
	    continue;

	 const char **J;
	 for (J = CmdL.FileList + 1; *J != 0; J++)
	    if (strcmp(*J,I.Name()) == 0)
		break;
	 
	 if (*J == 0)
	    List += string(I.Name()) + " ";
      }
      
      ShowList(c1out,"The following extra packages will be installed:",List);
   }

   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoDistUpgrade - Automatic smart upgrader				/*{{{*/
// ---------------------------------------------------------------------
/* Intelligent upgrader that will install and remove packages at will */
bool DoDistUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;

   c0out << "Calculating Upgrade... " << flush;
   if (pkgDistUpgrade(*Cache) == false)
   {
      c0out << "Failed" << endl;
      ShowBroken(c1out,Cache);
      return false;
   }
   
   c0out << "Done" << endl;
   
   return InstallPackages(Cache,true);
}
									/*}}}*/
// DoDSelectUpgrade - Do an upgrade by following dselects selections	/*{{{*/
// ---------------------------------------------------------------------
/* Follows dselect's selections */
bool DoDSelectUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;
   
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
	 Cache->MarkInstall(I);
   }
   
   // Apply erasures now, they override everything else.
   for (I = Cache->PkgBegin();I.end() != true; I++)
   {
      // Remove packages 
      if (I->SelectedState == pkgCache::State::DeInstall ||
	  I->SelectedState == pkgCache::State::Purge)
	 Cache->MarkDelete(I);
   }

   /* Use updates smart upgrade to do the rest, it will automatically
      ignore held items */
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache);
      return _error->Error("Internal Error, AllUpgrade broke stuff");
   }
   
   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoClean - Remove download archives					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoClean(CommandLine &CmdL)
{
   return true;
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
   
   return true;
}
									/*}}}*/
      
// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   cout << PACKAGE << ' ' << VERSION << " for " << ARCHITECTURE <<
       " compiled on " << __DATE__ << "  " << __TIME__ << endl;
   
   cout << "Usage: apt-get [options] command" << endl;
   cout << "       apt-get [options] install pkg1 [pkg2 ...]" << endl;
   cout << endl;
   cout << "apt-get is a simple command line interface for downloading and" << endl;
   cout << "installing packages. The most frequently used commands are update" << endl;
   cout << "and install." << endl;   
   cout << endl;
   cout << "Commands:" << endl;
   cout << "   update - Retrieve new lists of packages" << endl;
   cout << "   upgrade - Perform an upgrade" << endl;
   cout << "   install - Install new packages (pkg is libc6 not libc6.deb)" << endl;
   cout << "   remove - Remove packages" << endl;
   cout << "   dist-upgrade - Distribution upgrade, see apt-get(8)" << endl;
   cout << "   dselect-upgrade - Follow dselect selections" << endl;
   cout << "   clean - Erase downloaded archive files" << endl;
   cout << "   check - Verify that there are no broken dependencies" << endl;
   cout << endl;
   cout << "Options:" << endl;
   cout << "  -h   This help text." << endl;
   cout << "  -q   Loggable output - no progress indicator" << endl;
   cout << "  -qq No output except for errors" << endl;
   cout << "  -d  Download only - do NOT install or unpack archives" << endl;
   cout << "  -s  No-act. Perform ordering simulation" << endl;
   cout << "  -y  Assume Yes to all queries and do not prompt" << endl;
   cout << "  -f  Attempt to continue if the integrity check fails" << endl;
   cout << "  -m  Attempt to continue if archives are unlocatable" << endl;
   cout << "  -u  Show a list of upgraded packages as well" << endl;
   cout << "  -c=? Read this configuration file" << endl;
   cout << "  -o=? Set an arbitary configuration option, ie -o dir::cache=/tmp" << endl;
   cout << "See the apt-get(8), sources.list(8) and apt.conf(8) manual" << endl;
   cout << "pages for more information." << endl;
   return 100;
}
									/*}}}*/
// GetInitialize - Initialize things for apt-get			/*{{{*/
// ---------------------------------------------------------------------
/* */
void GetInitialize()
{
   _config->Set("quiet",0);
   _config->Set("help",false);
   _config->Set("APT::Get::Download-Only",false);
   _config->Set("APT::Get::Simulate",false);
   _config->Set("APT::Get::Assume-Yes",false);
   _config->Set("APT::Get::Fix-Broken",false);
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'q',"silent","quiet",CommandLine::IntLevel},
      {'d',"download-only","APT::Get::Download-Only",0},
      {'s',"simulate","APT::Get::Simulate",0},      
      {'s',"just-print","APT::Get::Simulate",0},      
      {'s',"recon","APT::Get::Simulate",0},      
      {'s',"no-act","APT::Get::Simulate",0},      
      {'y',"yes","APT::Get::Assume-Yes",0},      
      {'y',"assume-yes","APT::Get::Assume-Yes",0},      
      {'f',"fix-broken","APT::Get::Fix-Broken",0},
      {'u',"show-upgraded","APT::Get::Show-Upgraded",0},
      {'m',"ignore-missing","APT::Get::Fix-Broken",0},      
      {0,"ignore-hold","APT::Ingore-Hold",0},      
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   
   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitialize(*_config) == false ||
       CmdL.Parse(argc,argv) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Setup the output streams
   c0out.rdbuf(cout.rdbuf());
   c1out.rdbuf(cout.rdbuf());
   c2out.rdbuf(cout.rdbuf());
   if (_config->FindI("quiet",0) > 0)
      c0out.rdbuf(devnull.rdbuf());
   if (_config->FindI("quiet",0) > 1)
      c1out.rdbuf(devnull.rdbuf());
   
   // Match the operation
   struct 
   {
      const char *Match;
      bool (*Handler)(CommandLine &);
   } Map[] = {{"update",&DoUpdate},
              {"upgrade",&DoUpgrade},
              {"install",&DoInstall},
              {"remove",&DoInstall},
              {"dist-upgrade",&DoDistUpgrade},
              {"dselect-upgrade",&DoDSelectUpgrade},
              {"clean",&DoClean},
              {"check",&DoCheck},
              {0,0}};
   int I;
   for (I = 0; Map[I].Match != 0; I++)
   {
      if (strcmp(CmdL.FileList[0],Map[I].Match) == 0)
      {
	 Map[I].Handler(CmdL);
	 break;
      }
   }
      
   // No matching name
   if (Map[I].Match == 0)
      _error->Error("Invalid operation %s", CmdL.FileList[0]);

   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      if (Errors == true)
	 cout << "Returning 100." << endl;
      return Errors == true?100:0;
   }
   
   return 0;   
}
