// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-get.cc,v 1.86 1999/10/27 05:00:25 jgg Exp $
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
#include <apt-pkg/algorithms.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/clean.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/cachefile.h>

#include <config.h>

#include "acqprogress.h"

#include <fstream.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <regex.h>
#include <sys/wait.h>
									/*}}}*/

ostream c0out;
ostream c1out;
ostream c2out;
ofstream devnull("/dev/null");
unsigned int ScreenWidth = 80;

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
   bool Open(bool WithLock = true) 
   {
      OpTextProgress Prog(*_config);
      if (pkgCacheFile::Open(Prog,WithLock) == false)
	 return false;
      Sort();
      return true;
   };
   CacheFile() : List(0) {};
};
									/*}}}*/

// YnPrompt - Yes No Prompt.						/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool YnPrompt()
{
   if (_config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      c1out << 'Y' << endl;
      return true;
   }
   
   char C = 0;
   char Jnk = 0;
   read(STDIN_FILENO,&C,1);
   while (C != '\n' && Jnk != '\n') read(STDIN_FILENO,&Jnk,1);
   
   if (!(C == 'Y' || C == 'y' || C == '\n' || C == '\r'))
      return false;
   return true;
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
/* This prints out a string of space seperated words with a title and 
   a two space indent line wraped to the current screen width. */
bool ShowList(ostream &out,string Title,string List)
{
   if (List.empty() == true)
      return true;

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
   return false;
}
									/*}}}*/
// ShowBroken - Debugging aide						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
   with the name of each each broken dependency and a quite version 
   description. */
void ShowBroken(ostream &out,CacheFile &Cache,bool Now)
{
   out << "Sorry, but the following packages have unmet dependencies:" << endl;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      if (Cache[I].InstBroken() == false)
	  continue;
	  
      // Print out each package and the failed dependencies
      out <<"  " <<  I.Name() << ":";
      unsigned Indent = strlen(I.Name()) + 3;
      bool First = true;
      if (Cache[I].InstVerIter(Cache).end() == true)
      {
	 cout << endl;
	 continue;
      }
      
      for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList(); D.end() == false;)
      {
	 // Compute a single dependency element (glob or)
	 pkgCache::DepIterator Start;
	 pkgCache::DepIterator End;
	 D.GlobOr(Start,End);

	 if (Cache->IsImportantDep(End) == false || 
	     (Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    continue;

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
	    
	    out << Start.TargetPkg().Name();
	 
	    // Show a quick summary of the version requirements
	    if (Start.TargetVer() != 0)
	       out << " (" << Start.CompType() << " " << Start.TargetVer() << 
	       ")";
	    
	    /* Show a summary of the target package if possible. In the case
	       of virtual packages we show nothing */	 
	    pkgCache::PkgIterator Targ = Start.TargetPkg();
	    if (Targ->ProvidesList == 0)
	    {
	       out << " but ";
	       pkgCache::VerIterator Ver = Cache[Targ].InstVerIter(Cache);
	       if (Ver.end() == false)
		  out << Ver.VerStr() << (Now?" is installed":" is to be installed");
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
		     out << (Now?"it is not installed":"it is not going to be installed");
	       }	       
	    }
	    
	    if (Start != End)
	       cout << " or";
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
   /* Print out a list of packages that are going to be removed extra
      to what the user asked */
   string List;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].NewInstall() == true)
	 List += string(I.Name()) + " ";
   }
   
   ShowList(out,"The following NEW packages will be installed:",List);
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
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].Delete() == true)
      {
	 if ((Cache[I].iFlags & pkgDepCache::Purge) == pkgDepCache::Purge)
	    List += string(I.Name()) + "* ";
	 else
	    List += string(I.Name()) + " ";
      }
   }
   
   ShowList(out,"The following packages will be REMOVED:",List);
}
									/*}}}*/
// ShowKept - Show kept packages					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowKept(ostream &out,CacheFile &Cache)
{
   string List;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {	 
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == true || Cache[I].Upgradable() == false ||
	  I->CurrentVer == 0 || Cache[I].Delete() == true)
	 continue;
      
      List += string(I.Name()) + " ";
   }
   ShowList(out,"The following packages have been kept back",List);
}
									/*}}}*/
// ShowUpgraded - Show upgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowUpgraded(ostream &out,CacheFile &Cache)
{
   string List;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == false || Cache[I].NewInstall() == true)
	 continue;
      
      List += string(I.Name()) + " ";
   }
   ShowList(out,"The following packages will be upgraded",List);
}
									/*}}}*/
// ShowHold - Show held but changed packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHold(ostream &out,CacheFile &Cache)
{
   string List;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].InstallVer != (pkgCache::Version *)I.CurrentVer() &&
	  I->SelectedState == pkgCache::State::Hold)
	 List += string(I.Name()) + " ";
   }

   return ShowList(out,"The following held packages will be changed:",List);
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
   bool *Added = new bool[Cache->HeaderP->PackageCount];
   for (unsigned int I = 0; I != Cache->HeaderP->PackageCount; I++)
      Added[I] = false;
   
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if ((I->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential)
	 continue;
      
      // The essential package is being removed
      if (Cache[I].Delete() == true)
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
	    sprintf(S,"%s (due to %s) ",P.Name(),I.Name());
	    List += S;
	 }	 
      }      
   }
   
   delete [] Added;
   if (List.empty() == false)
      out << "WARNING: The following essential packages will be removed" << endl;
   return ShowList(out,"This should NOT be done unless you know exactly what you are doing!",List);
}
									/*}}}*/
// Stats - Show some statistics						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Stats(ostream &out,pkgDepCache &Dep)
{
   unsigned long Upgrade = 0;
   unsigned long Install = 0;
   unsigned long ReInstall = 0;
   for (pkgCache::PkgIterator I = Dep.PkgBegin(); I.end() == false; I++)
   {
      if (Dep[I].NewInstall() == true)
	 Install++;
      else
	 if (Dep[I].Upgrade() == true)
	    Upgrade++;
      if (Dep[I].Delete() == false && (Dep[I].iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall)
	 ReInstall++;
   }   

   out << Upgrade << " packages upgraded, " << 
      Install << " newly installed, ";
   if (ReInstall != 0)
      out << ReInstall << " reinstalled, ";
   out << Dep.DelCount() << " to remove and " << 
      Dep.KeepCount() << " not upgraded." << endl;

   if (Dep.BadCount() != 0)
      out << Dep.BadCount() << " packages not fully installed or removed." << endl;
}
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
// CacheFile::Open - Open the cache file				/*{{{*/
// ---------------------------------------------------------------------
/* This routine generates the caches and then opens the dependency cache
   and verifies that the system is OK. */
bool CacheFile::CheckDeps(bool AllowBroken)
{
   if (_error->PendingError() == true)
      return false;

   // Check that the system is OK
   if (Cache->DelCount() != 0 || Cache->InstCount() != 0)
      return _error->Error("Internal Error, non-zero counts");
   
   // Apply corrections for half-installed packages
   if (pkgApplyStatus(*Cache) == false)
      return false;
   
   // Nothing is broken
   if (Cache->BrokenCount() == 0 || AllowBroken == true)
      return true;

   // Attempt to fix broken things
   if (_config->FindB("APT::Get::Fix-Broken",false) == true)
   {
      c1out << "Correcting dependencies..." << flush;
      if (pkgFixBroken(*Cache) == false || Cache->BrokenCount() != 0)
      {
	 c1out << " failed." << endl;
	 ShowBroken(c1out,*this,true);

	 return _error->Error("Unable to correct dependencies");
      }
      if (pkgMinimizeUpgrade(*Cache) == false)
	 return _error->Error("Unable to minimize the upgrade set");
      
      c1out << " Done" << endl;
   }
   else
   {
      c1out << "You might want to run `apt-get -f install' to correct these." << endl;
      ShowBroken(c1out,*this,true);

      return _error->Error("Unmet dependencies. Try using -f.");
   }
      
   return true;
}
									/*}}}*/

// InstallPackages - Actually download and install the packages		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the informative messages describing what is going to 
   happen and then calls the download routines */
bool InstallPackages(CacheFile &Cache,bool ShwKept,bool Ask = true,bool Saftey = true)
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
   if (_config->FindB("APT::Get::Show-Upgraded",false) == true)
      ShowUpgraded(c1out,Cache);
   Essential = !ShowEssential(c1out,Cache);
   Fail |= Essential;
   Stats(c1out,Cache);
   
   // Sanity check
   if (Cache->BrokenCount() != 0)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error("Internal Error, InstallPackages was called with broken packages!");
   }

   if (Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
       Cache->BadCount() == 0)
      return true;

   // Run the simulator ..
   if (_config->FindB("APT::Get::Simulate") == true)
   {
      pkgSimulate PM(Cache);
      pkgPackageManager::OrderResult Res = PM.DoInstall();
      if (Res == pkgPackageManager::Failed)
	 return false;
      if (Res != pkgPackageManager::Completed)
	 return _error->Error("Internal Error, Ordering didn't finish");
      return true;
   }
   
   // Create the text record parser
   pkgRecords Recs(Cache);
   if (_error->PendingError() == true)
      return false;
   
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
      if (_error->PendingError() == true)
	 return _error->Error("Unable to lock the download directory");
   }
   
   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher(&Stat);

   // Read the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return _error->Error("The list of sources could not be read.");
   
   // Create the package manager and prepare to download
   pkgDPkgPM PM(Cache);
   if (PM.GetArchives(&Fetcher,&List,&Recs) == false || 
       _error->PendingError() == true)
      return false;

   // Display statistics
   unsigned long FetchBytes = Fetcher.FetchNeeded();
   unsigned long FetchPBytes = Fetcher.PartialPresent();
   unsigned long DebBytes = Fetcher.TotalNeeded();
   if (DebBytes != Cache->DebSize())
   {
      c0out << DebBytes << ',' << Cache->DebSize() << endl;
      c0out << "How odd.. The sizes didn't match, email apt@packages.debian.org" << endl;
   }
   
   // Number of bytes
   c1out << "Need to get ";
   if (DebBytes != FetchBytes)
      c1out << SizeToStr(FetchBytes) << "B/" << SizeToStr(DebBytes) << 'B';
   else
      c1out << SizeToStr(DebBytes) << 'B';
      
   c1out << " of archives. After unpacking ";

   // Check for enough free space
   struct statfs Buf;
   string OutputDir = _config->FindDir("Dir::Cache::Archives");
   if (statfs(OutputDir.c_str(),&Buf) != 0)
      return _error->Errno("statfs","Couldn't determine free space in %s",
			   OutputDir.c_str());
   if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
      return _error->Error("Sorry, you don't have enough free space in %s to hold all the .debs.",
			   OutputDir.c_str());
   
   // Size delta
   if (Cache->UsrSize() >= 0)
      c1out << SizeToStr(Cache->UsrSize()) << "B will be used." << endl;
   else
      c1out << SizeToStr(-1*Cache->UsrSize()) << "B will be freed." << endl;

   if (_error->PendingError() == true)
      return false;

   // Fail safe check
   if (_config->FindI("quiet",0) >= 2 ||
       _config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      if (Fail == true && _config->FindB("APT::Get::Force-Yes",false) == false)
	 return _error->Error("There are problems and -y was used without --force-yes");
   }         

   if (Essential == true && Saftey == true)
   {
      c2out << "You are about to do something potentially harmful" << endl;
      c2out << "To continue type in the phrase 'Yes, I understand this may be bad'" << endl;
      c2out << " ?] " << flush;
      if (AnalPrompt("Yes, I understand this may be bad") == false)
      {
	 c2out << "Abort." << endl;
	 exit(1);
      }     
   }
   else
   {
      // Prompt to continue
      if (Ask == true || Fail == true)
      {            
	 if (_config->FindI("quiet",0) < 2 &&
	     _config->FindB("APT::Get::Assume-Yes",false) == false)
	 {
	    c2out << "Do you want to continue? [Y/n] " << flush;
	 
	    if (YnPrompt() == false)
	    {
	       c2out << "Abort." << endl;
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
	       I->Owner->FileSize << ' ' << I->Owner->MD5Sum() << endl;
      return true;
   }
   
   // Run it
   while (1)
   {
      if (_config->FindB("APT::Get::No-Download",false) == false)
	 if (Fetcher.Run() == pkgAcquire::Failed)
	    return false;
      
      // Print out errors
      bool Failed = false;
      bool Transient = false;
      for (pkgAcquire::Item **I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
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
	 
	 cerr << "Failed to fetch " << (*I)->DescURI() << endl;
	 cerr << "  " << (*I)->ErrorText << endl;
	 Failed = true;
      }

      /* If we are in no download mode and missing files then there were
         'failures' then the user must specify -m. Furthermore, there 
         is no such thing as a transient error in no-download mode! */
      if (Transient == true && 
	  _config->FindB("APT::Get::No-Download",false) == true)
      {
	 Transient = false;
	 Failed = true;
      }
      
      if (_config->FindB("APT::Get::Download-Only",false) == true)
      {
	 if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
	    return _error->Error("Some files failed to download");
	 return true;
      }
      
      if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
      {
	 return _error->Error("Unable to fetch some archives, maybe try with --fix-missing?");
      }
      
      if (Transient == true && Failed == true)
	 return _error->Error("--fix-missing and media swapping is not currently supported");
      
      // Try to deal with missing package files
      if (Failed == true && PM.FixMissing() == false)
      {
	 cerr << "Unable to correct missing packages." << endl;
	 return _error->Error("Aborting Install.");
      }
      
      Cache.ReleaseLock();
      pkgPackageManager::OrderResult Res = PM.DoInstall();
      if (Res == pkgPackageManager::Failed || _error->PendingError() == true)
	 return false;
      if (Res == pkgPackageManager::Completed)
	 return true;
      
      // Reload the fetcher object and loop again for media swapping
      Fetcher.Shutdown();
      if (PM.GetArchives(&Fetcher,&List,&Recs) == false)
	 return false;
   }   
}
									/*}}}*/
// TryToInstall - Try to install a single package			/*{{{*/
// ---------------------------------------------------------------------
/* This used to be inlined in DoInstall, but with the advent of regex package
   name matching it was split out.. */
bool TryToInstall(pkgCache::PkgIterator Pkg,pkgDepCache &Cache,
		  pkgProblemResolver &Fix,bool Remove,bool BrokenFix,
		  unsigned int &ExpectedInst,bool AllowFail = true)
{
   /* This is a pure virtual package and there is a single available 
      provides */
   if (Cache[Pkg].CandidateVer == 0 && Pkg->ProvidesList != 0 &&
       Pkg.ProvidesList()->NextProvides == 0)
   {
      pkgCache::PkgIterator Tmp = Pkg.ProvidesList().OwnerPkg();
      c1out << "Note, installing " << Tmp.Name() << " instead of " << Pkg.Name() << endl;
      Pkg = Tmp;
   }
   
   // Handle the no-upgrade case
   if (_config->FindB("APT::Get::no-upgrade",false) == true &&
       Pkg->CurrentVer != 0)
   {
      if (AllowFail == true)
	 c1out << "Skipping " << Pkg.Name() << ", it is already installed and no-upgrade is set." << endl;
      return true;
   }
   
   // Check if there is something at all to install
   pkgDepCache::StateCache &State = Cache[Pkg];
   if (State.CandidateVer == 0)
   {
      if (AllowFail == false)
	 return false;
      
      if (Pkg->ProvidesList != 0)
      {
	 c1out << "Package " << Pkg.Name() << " is a virtual package provided by:" << endl;
	 
	 pkgCache::PrvIterator I = Pkg.ProvidesList();
	 for (; I.end() == false; I++)
	 {
	    pkgCache::PkgIterator Pkg = I.OwnerPkg();
	    
	    if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer())
	    {
	       if (Cache[Pkg].Install() == true && Cache[Pkg].NewInstall() == false)
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() <<
		  " [Installed]"<< endl;
	       else
		  c1out << "  " << Pkg.Name() << " " << I.OwnerVer().VerStr() << endl;
	    }      
	 }
	 c1out << "You should explicitly select one to install." << endl;
      }
      else
      {
	 c1out << "Package " << Pkg.Name() << " has no available version, but exists in the database." << endl;
	 c1out << "This typically means that the package was mentioned in a dependency and " << endl;
	 c1out << "never uploaded, has been obsoleted or is not available with the contents " << endl;
	 c1out << "of sources.list" << endl;
	 
	 string List;
	 pkgCache::DepIterator Dep = Pkg.RevDependsList();
	 for (; Dep.end() == false; Dep++)
	 {
	    if (Dep->Type != pkgCache::Dep::Replaces)
	       continue;
	    List += string(Dep.ParentPkg().Name()) + " ";
	 }	    
	 ShowList(c1out,"However the following packages replace it:",List);
      }
      
      _error->Error("Package %s has no installation candidate",Pkg.Name());
      return false;
   }

   Fix.Clear(Pkg);
   Fix.Protect(Pkg);
   if (Remove == true)
   {
      Fix.Remove(Pkg);
      Cache.MarkDelete(Pkg,_config->FindB("APT::Get::Purge",false));
      return true;
   }
   
   // Install it
   Cache.MarkInstall(Pkg,false);
   if (State.Install() == false)
   {
      if (_config->FindB("APT::Get::ReInstall",false) == true)
      {
	 if (Pkg->CurrentVer == 0 || Pkg.CurrentVer().Downloadable() == false)
	    c1out << "Sorry, re-installation of " << Pkg.Name() << " is not possible, it cannot be downloaded" << endl;
	 else
	    Cache.SetReInstall(Pkg,true);
      }      
      else
      {
	 if (AllowFail == true)
	    c1out << "Sorry, " << Pkg.Name() << " is already the newest version"  << endl;
      }      
   }   
   else
      ExpectedInst++;
   
   // Install it with autoinstalling enabled.
   if (State.InstBroken() == true && BrokenFix == false)
      Cache.MarkInstall(Pkg,true);
   return true;
}
									/*}}}*/

// DoUpdate - Update the package lists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoUpdate(CommandLine &)
{
   // Get the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return false;

   // Lock the list directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
      if (_error->PendingError() == true)
	 return _error->Error("Unable to lock the list directory");
   }
   
   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));
   pkgAcquire Fetcher(&Stat);
   
   // Populate it with the source selection
   pkgSourceList::const_iterator I;
   for (I = List.begin(); I != List.end(); I++)
   {
      new pkgAcqIndex(&Fetcher,I);
      if (_error->PendingError() == true)
	 return false;
   }
   
   // Run it
   if (Fetcher.Run() == pkgAcquire::Failed)
      return false;

   bool Failed = false;
   for (pkgAcquire::Item **I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 continue;

      (*I)->Finished();
      
      cerr << "Failed to fetch " << (*I)->DescURI() << endl;
      cerr << "  " << (*I)->ErrorText << endl;
      Failed = true;
   }
   
   // Clean out any old list files
   if (_config->FindB("APT::Get::List-Cleanup",false) == false)
   {
      if (Fetcher.Clean(_config->FindDir("Dir::State::lists")) == false ||
	  Fetcher.Clean(_config->FindDir("Dir::State::lists") + "partial/") == false)
	 return false;
   }
   
   // Prepare the cache.   
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;
   
   if (Failed == true)
      return _error->Error("Some index files failed to download, they have been ignored, or old ones used instead.");
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
   if (Cache.Open() == false || Cache.CheckDeps() == false)
      return false;

   // Do the upgrade
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
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
   if (Cache.Open() == false || Cache.CheckDeps(CmdL.FileSize() != 1) == false)
      return false;
   
   // Enter the special broken fixing mode if the user specified arguments
   bool BrokenFix = false;
   if (Cache->BrokenCount() != 0)
      BrokenFix = true;
   
   unsigned int ExpectedInst = 0;
   unsigned int Packages = 0;
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
      while (Cache->FindPkg(S).end() == true)
      {
	 // Handle an optional end tag indicating what to do
	 if (S[Length - 1] == '-')
	 {
	    Remove = true;
	    S[--Length] = 0;
	    continue;
	 }
	 
	 if (S[Length - 1] == '+')
	 {
	    Remove = false;
	    S[--Length] = 0;
	    continue;
	 }
	 break;
      }
      
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
      Packages++;
      if (Pkg.end() == true)
      {
	 // Check if the name is a regex
	 const char *I;
	 for (I = S; *I != 0; I++)
	    if (*I == '.' || *I == '?' || *I == '*')
	       break;
	 if (*I == 0)
	    return _error->Error("Couldn't find package %s",S);

	 // Regexs must always be confirmed
	 ExpectedInst += 1000;
	 
	 // Compile the regex pattern
	 regex_t Pattern;
	 if (regcomp(&Pattern,S,REG_EXTENDED | REG_ICASE | 
		     REG_NOSUB) != 0)
	    return _error->Error("Regex compilation error");
	 
	 // Run over the matches
	 bool Hit = false;
	 for (Pkg = Cache->PkgBegin(); Pkg.end() == false; Pkg++)
	 {
	    if (regexec(&Pattern,Pkg.Name(),0,0,0) != 0)
	       continue;
	    
	    Hit |= TryToInstall(Pkg,Cache,Fix,Remove,BrokenFix,
				ExpectedInst,false);
	 }
	 regfree(&Pattern);
	 
	 if (Hit == false)
	    return _error->Error("Couldn't find package %s",S);
      }
      else
      {
	 if (TryToInstall(Pkg,Cache,Fix,Remove,BrokenFix,ExpectedInst) == false)
	    return false;
      }      
   }

   /* If we are in the Broken fixing mode we do not attempt to fix the
      problems. This is if the user invoked install without -f and gave
      packages */
   if (BrokenFix == true && Cache->BrokenCount() != 0)
   {
      c1out << "You might want to run `apt-get -f install' to correct these:" << endl;
      ShowBroken(c1out,Cache,false);

      return _error->Error("Unmet dependencies. Try 'apt-get -f install' with no packages (or specify a solution).");
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
      ShowBroken(c1out,Cache,false);
      return _error->Error("Sorry, broken packages");
   }   
   
   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   if (Cache->InstCount() != ExpectedInst)
   {
      string List;
      for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
      {
	 pkgCache::PkgIterator I(Cache,Cache.List[J]);
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

   // See if we need to prompt
   if (Cache->InstCount() == ExpectedInst && Cache->DelCount() == 0)
      return InstallPackages(Cache,false,false);
   
   return InstallPackages(Cache,false);   
}
									/*}}}*/
// DoDistUpgrade - Automatic smart upgrader				/*{{{*/
// ---------------------------------------------------------------------
/* Intelligent upgrader that will install and remove packages at will */
bool DoDistUpgrade(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open() == false || Cache.CheckDeps() == false)
      return false;

   c0out << "Calculating Upgrade... " << flush;
   if (pkgDistUpgrade(*Cache) == false)
   {
      c0out << "Failed" << endl;
      ShowBroken(c1out,Cache,false);
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
   if (Cache.Open() == false || Cache.CheckDeps() == false)
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
      if (_config->FindB("APT::Ingore-Hold",false) == false)
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
	 return _error->Error("Internal Error, problem resolver broke stuff");
      }
   }

   // Now upgrade everything
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error("Internal Error, problem resolver broke stuff");
   }
   
   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoClean - Remove download archives					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoClean(CommandLine &CmdL)
{
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
      if (_error->PendingError() == true)
	 return _error->Error("Unable to lock the download directory");
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
      cout << "Del " << Pkg << " " << Ver << " [" << SizeToStr(St.st_size) << "B]" << endl;
      
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
	 return _error->Error("Unable to lock the download directory");
   }
   
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;
   
   LogCleaner Cleaner;
   
   return Cleaner.Go(_config->FindDir("Dir::Cache::archives"),*Cache) &&
      Cleaner.Go(_config->FindDir("Dir::Cache::archives") + "partial/",*Cache);
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
      return _error->Error("Must specify at least one package to fetch source for");
   
   // Read the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return _error->Error("The list of sources could not be read.");
   
   // Create the text record parsers
   pkgRecords Recs(Cache);
   pkgSrcRecords SrcRecs(List);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher(&Stat);

   DscFile *Dsc = new DscFile[CmdL.FileSize()];
   
   // Load the requestd sources into the fetcher
   unsigned J = 0;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      string Src;
      
      /* Lookup the version of the package we would install if we were to
         install a version and determine the source package name, then look
         in the archive for a source package of the same name. In theory
         we could stash the version string as well and match that too but
         today there aren't multi source versions in the archive. */
      pkgCache::PkgIterator Pkg = Cache->FindPkg(*I);
      if (Pkg.end() == false)
      {
	 pkgCache::VerIterator Ver = Cache->GetCandidateVer(Pkg);
	 if (Ver.end() == false)
	 {
	    pkgRecords::Parser &Parse = Recs.Lookup(Ver.FileList());
	    Src = Parse.SourcePkg();
	 }	 
      }   

      // No source package name..
      if (Src.empty() == true)
	 Src = *I;
      
      // The best hit
      pkgSrcRecords::Parser *Last = 0;
      unsigned long Offset = 0;
      string Version;
      bool IsMatch = false;
	 
      // Iterate over all of the hits
      pkgSrcRecords::Parser *Parse;
      SrcRecs.Restart();
      while ((Parse = SrcRecs.Find(Src.c_str(),false)) != 0)
      {
	 string Ver = Parse->Version();
	 
	 // Skip name mismatches
	 if (IsMatch == true && Parse->Package() != Src)
	    continue;

	 // Newer version or an exact match
	 if (Last == 0 || pkgVersionCompare(Version,Ver) < 0 || 
	     (Parse->Package() == Src && IsMatch == false))
	 {
	    IsMatch = Parse->Package() == Src;
	    Last = Parse;
	    Offset = Parse->Offset();
	    Version = Ver;
	 }      
      }
      
      if (Last == 0)
	 return _error->Error("Unable to find a source package for %s",Src.c_str());
      
      // Back track
      vector<pkgSrcRecords::File> Lst;
      if (Last->Jump(Offset) == false || Last->Files(Lst) == false)
	 return false;

      // Load them into the fetcher
      for (vector<pkgSrcRecords::File>::const_iterator I = Lst.begin();
	   I != Lst.end(); I++)
      {
	 // Try to guess what sort of file it is we are getting.
	 string Comp;
	 if (I->Path.find(".dsc") != string::npos)
	 {
	    Comp = "dsc";
	    Dsc[J].Package = Last->Package();
	    Dsc[J].Version = Last->Version();
	    Dsc[J].Dsc = flNotDir(I->Path);
	 }
	 
	 if (I->Path.find(".tar.gz") != string::npos)
	    Comp = "tar";
	 if (I->Path.find(".diff.gz") != string::npos)
	    Comp = "diff";
	 
	 // Diff only mode only fetches .diff files
	 if (_config->FindB("APT::Get::Diff-Only",false) == true &&
	     Comp != "diff")
	    continue;
	 
	 // Tar only mode only fetches .tar files
	 if (_config->FindB("APT::Get::Tar-Only",false) == true &&
	     Comp != "tar")
	    continue;
	 
	 new pkgAcqFile(&Fetcher,Last->Source()->ArchiveURI(I->Path),
			I->MD5Hash,I->Size,Last->Source()->SourceInfo(Src,
			Last->Version(),Comp),Src);
      }
   }
   
   // Display statistics
   unsigned long FetchBytes = Fetcher.FetchNeeded();
   unsigned long FetchPBytes = Fetcher.PartialPresent();
   unsigned long DebBytes = Fetcher.TotalNeeded();

   // Check for enough free space
   struct statfs Buf;
   string OutputDir = ".";
   if (statfs(OutputDir.c_str(),&Buf) != 0)
      return _error->Errno("statfs","Couldn't determine free space in %s",
			   OutputDir.c_str());
   if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
      return _error->Error("Sorry, you don't have enough free space in %s",
			   OutputDir.c_str());
   
   // Number of bytes
   c1out << "Need to get ";
   if (DebBytes != FetchBytes)
      c1out << SizeToStr(FetchBytes) << "B/" << SizeToStr(DebBytes) << 'B';
   else
      c1out << SizeToStr(DebBytes) << 'B';
   c1out << " of source archives." << endl;

   if (_config->FindB("APT::Get::Simulate",false) == true)
   {
      for (unsigned I = 0; I != J; I++)
	 cout << "Fetch Source " << Dsc[I].Package << endl;
      return true;
   }
   
   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); I++)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->MD5Sum() << endl;
      return true;
   }
   
   // Run it
   if (Fetcher.Run() == pkgAcquire::Failed)
      return false;

   // Print error messages
   bool Failed = false;
   for (pkgAcquire::Item **I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone &&
	  (*I)->Complete == true)
	 continue;
      
      cerr << "Failed to fetch " << (*I)->DescURI() << endl;
      cerr << "  " << (*I)->ErrorText << endl;
      Failed = true;
   }
   if (Failed == true)
      return _error->Error("Failed to fetch some archives.");
   
   if (_config->FindB("APT::Get::Download-only",false) == true)
      return true;
   
   // Unpack the sources
   pid_t Process = ExecFork();
   
   if (Process == 0)
   {
      for (unsigned I = 0; I != J; I++)
      {
	 string Dir = Dsc[I].Package + '-' + pkgBaseVersion(Dsc[I].Version.c_str());
	 
	 // Diff only mode only fetches .diff files
	 if (_config->FindB("APT::Get::Diff-Only",false) == true ||
	     _config->FindB("APT::Get::Tar-Only",false) == true)
	    continue;
	 
	 // See if the package is already unpacked
	 struct stat Stat;
	 if (stat(Dir.c_str(),&Stat) == 0 &&
	     S_ISDIR(Stat.st_mode) != 0)
	 {
	    c0out << "Skipping unpack of already unpacked source in " << Dir << endl;
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
	       cerr << "Unpack command '" << S << "' failed." << endl;
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
	       cerr << "Build command '" << S << "' failed." << endl;
	       _exit(1);
	    }	    
	 }      
      }
      
      _exit(0);
   }
   
   // Wait for the subprocess
   int Status = 0;
   while (waitpid(Process,&Status,0) != Process)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }

   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      return _error->Error("Child process failed");
   
   return true;
}
									/*}}}*/

// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp(CommandLine &CmdL)
{
   cout << PACKAGE << ' ' << VERSION << " for " << ARCHITECTURE <<
       " compiled on " << __DATE__ << "  " << __TIME__ << endl;
   if (_config->FindB("version") == true)
      return 100;
       
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
   cout << "   source - Download source archives" << endl;
   cout << "   dist-upgrade - Distribution upgrade, see apt-get(8)" << endl;
   cout << "   dselect-upgrade - Follow dselect selections" << endl;
   cout << "   clean - Erase downloaded archive files" << endl;
   cout << "   autoclean - Erase old downloaded archive files" << endl;
   cout << "   check - Verify that there are no broken dependencies" << endl;
   cout << endl;
   cout << "Options:" << endl;
   cout << "  -h  This help text." << endl;
   cout << "  -q  Loggable output - no progress indicator" << endl;
   cout << "  -qq No output except for errors" << endl;
   cout << "  -d  Download only - do NOT install or unpack archives" << endl;
   cout << "  -s  No-act. Perform ordering simulation" << endl;
   cout << "  -y  Assume Yes to all queries and do not prompt" << endl;
   cout << "  -f  Attempt to continue if the integrity check fails" << endl;
   cout << "  -m  Attempt to continue if archives are unlocatable" << endl;
   cout << "  -u  Show a list of upgraded packages as well" << endl;
   cout << "  -b  Build the source package after fetching it" << endl;
   cout << "  -c=? Read this configuration file" << endl;
   cout << "  -o=? Set an arbitary configuration option, eg -o dir::cache=/tmp" << endl;
   cout << "See the apt-get(8), sources.list(5) and apt.conf(5) manual" << endl;
   cout << "pages for more information and options." << endl;
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
   _config->Set("APT::Get::Force-Yes",false);
   _config->Set("APT::Get::APT::Get::No-List-Cleanup",true);
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

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'q',"silent","quiet",CommandLine::IntLevel},
      {'d',"download-only","APT::Get::Download-Only",0},
      {'b',"compile","APT::Get::Compile",0},
      {'b',"build","APT::Get::Compile",0},
      {'s',"simulate","APT::Get::Simulate",0},      
      {'s',"just-print","APT::Get::Simulate",0},      
      {'s',"recon","APT::Get::Simulate",0},      
      {'s',"no-act","APT::Get::Simulate",0},      
      {'y',"yes","APT::Get::Assume-Yes",0},      
      {'y',"assume-yes","APT::Get::Assume-Yes",0},      
      {'f',"fix-broken","APT::Get::Fix-Broken",0},
      {'u',"show-upgraded","APT::Get::Show-Upgraded",0},
      {'m',"ignore-missing","APT::Get::Fix-Missing",0},
      {0,"no-download","APT::Get::No-Download",0},
      {0,"fix-missing","APT::Get::Fix-Missing",0},
      {0,"ignore-hold","APT::Ingore-Hold",0},      
      {0,"no-upgrade","APT::Get::no-upgrade",0},
      {0,"force-yes","APT::Get::force-yes",0},
      {0,"print-uris","APT::Get::Print-URIs",0},
      {0,"diff-only","APT::Get::Diff-Only",0},
      {0,"tar-only","APT::Get::tar-Only",0},
      {0,"purge","APT::Get::Purge",0},
      {0,"list-cleanup","APT::Get::List-Cleanup",0},
      {0,"reinstall","APT::Get::ReInstall",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {{"update",&DoUpdate},
                                   {"upgrade",&DoUpgrade},
                                   {"install",&DoInstall},
                                   {"remove",&DoInstall},
                                   {"dist-upgrade",&DoDistUpgrade},
                                   {"dselect-upgrade",&DoDSelectUpgrade},
                                   {"clean",&DoClean},
                                   {"autoclean",&DoAutoClean},
                                   {"check",&DoCheck},
      				   {"source",&DoSource},
      				   {"help",&ShowHelp},
                                   {0,0}};
   
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
       _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp(CmdL);

   // Deal with stdout not being a tty
   if (ttyname(STDOUT_FILENO) == 0 && _config->FindI("quiet",0) < 1)
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
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;   
}
