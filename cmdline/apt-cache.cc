// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cache.cc,v 1.14 1998/12/04 22:56:53 jgg Exp $
/* ######################################################################
   
   apt-cache - Manages the cache files
   
   apt-cache provides some functions fo manipulating the cache files.
   It uses the command line interface common to all the APT tools. The
   only really usefull function right now is dumpavail which is used
   by the dselect method. Everything else is ment as a debug aide.
   
   Returns 100 on failure, 0 on success.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/init.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>

#include <iostream.h>
#include <config.h>
									/*}}}*/

// UnMet - Show unmet dependencies					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool UnMet(pkgCache &Cache)
{
   bool Important = _config->FindB("Important",false);
   
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
      {
	 bool Header = false;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false;)
	 {
	    // Collect or groups
	    pkgCache::DepIterator Start;
	    pkgCache::DepIterator End;
	    D.GlobOr(Start,End);
	    
/*	    cout << "s: Check " << Start.TargetPkg().Name() << ',' <<
	       End.TargetPkg().Name() << endl;*/
	       
	    // Skip conflicts and replaces
	    if (End->Type != pkgCache::Dep::PreDepends &&
		End->Type != pkgCache::Dep::Depends && 
		End->Type != pkgCache::Dep::Suggests &&
		End->Type != pkgCache::Dep::Recommends)
	       continue;

	    // Important deps only
	    if (Important == true)
	       if (End->Type != pkgCache::Dep::PreDepends &&
		   End->Type != pkgCache::Dep::Depends)
		  continue;
	    
	    // Verify the or group
	    bool OK = false;
	    pkgCache::DepIterator RealStart = Start;
	    do
	    {
	       // See if this dep is Ok
	       pkgCache::Version **VList = Start.AllTargets();
	       if (*VList != 0)
	       {
		  OK = true;
		  delete [] VList;
		  break;
	       }
	       delete [] VList;
	       
	       if (Start == End)
		  break;
	       Start++;
	    }
	    while (1);

	    // The group is OK
	    if (OK == true)
	       continue;
	    
	    // Oops, it failed..
	    if (Header == false)
		  cout << "Package " << P.Name() << " version " << 
	       V.VerStr() << " has an unmet dep:" << endl;
	    Header = true;
	    
	    // Print out the dep type
	    cout << " " << End.DepType() << ": ";

	    // Show the group
	    Start = RealStart;
	    do
	    {
	       cout << Start.TargetPkg().Name();
	       if (Start.TargetVer() != 0)
		  cout << " (" << Start.CompType() << " " << Start.TargetVer() <<
		  ")";
	       if (Start == End)
		  break;
	       cout << " | ";
	       Start++;
	    }
	    while (1);
	    
	    cout << endl;
	 }	 
      }
   }   
   return true;
}
									/*}}}*/
// DumpPackage - Show a dump of a package record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DumpPackage(pkgCache &Cache,CommandLine &CmdL)
{   
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning("Unable to locate package %s",*I);
	 continue;
      }

      cout << "Package: " << Pkg.Name() << endl;
      cout << "Versions: ";
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr();
	 for (pkgCache::VerFileIterator Vf = Cur.FileList(); Vf.end() == false; Vf++)
	    cout << "(" << Vf.File().FileName() << ")";
	 cout << ',';
      }
      
      cout << endl;
      
      cout << "Reverse Depends: " << endl;
      for (pkgCache::DepIterator D = Pkg.RevDependsList(); D.end() != true; D++)
	 cout << "  " << D.ParentPkg().Name() << ',' << D.TargetPkg().Name() << endl;

      cout << "Dependencies: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::DepIterator Dep = Cur.DependsList(); Dep.end() != true; Dep++)
	    cout << Dep.TargetPkg().Name() << " (" << (int)Dep->CompareOp << " " << Dep.TargetVer() << ") ";
	 cout << endl;
      }      

      cout << "Provides: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::PrvIterator Prv = Cur.ProvidesList(); Prv.end() != true; Prv++)
	    cout << Prv.ParentPkg().Name() << " ";
	 cout << endl;
      }
      cout << "Reverse Provides: " << endl;
      for (pkgCache::PrvIterator Prv = Pkg.ProvidesList(); Prv.end() != true; Prv++)
	 cout << Prv.OwnerPkg().Name() << " " << Prv.OwnerVer().VerStr();
      cout << endl;
            
   }

   return true;
}
									/*}}}*/
// Stats - Dump some nice statistics					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Stats(pkgCache &Cache)
{
   cout << "Total Package Names : " << Cache.Head().PackageCount << endl;
   pkgCache::PkgIterator I = Cache.PkgBegin();
   
   int Normal = 0;
   int Virtual = 0;
   int NVirt = 0;
   int DVirt = 0;
   int Missing = 0;
   for (;I.end() != true; I++)
   {
      if (I->VersionList != 0 && I->ProvidesList == 0)
      {
	 Normal++;
	 continue;
      }

      if (I->VersionList != 0 && I->ProvidesList != 0)
      {
	 NVirt++;
	 continue;
      }
      
      if (I->VersionList == 0 && I->ProvidesList != 0)
      {
	 // Only 1 provides
	 if (I.ProvidesList()->NextProvides == 0)
	 {
	    DVirt++;
	 }
	 else
	    Virtual++;
	 continue;
      }
      if (I->VersionList == 0 && I->ProvidesList == 0)
      {
	 Missing++;
	 continue;
      }
   }
   cout << "  Normal Packages: " << Normal << endl;
   cout << "  Pure Virtual Packages: " << Virtual << endl;
   cout << "  Single Virtual Packages: " << DVirt << endl;
   cout << "  Mixed Virtual Packages: " << NVirt << endl;
   cout << "  Missing: " << Missing << endl;
   
   cout << "Total Distinct Versions: " << Cache.Head().VersionCount << endl;
   cout << "Total Dependencies: " << Cache.Head().DependsCount << endl;
   return true;
}
									/*}}}*/
// Check - Check some things about the cache				/*{{{*/
// ---------------------------------------------------------------------
/* Debug aide mostly */
bool Check(pkgCache &Cache)
{
   pkgCache::PkgIterator Pkg = Cache.PkgBegin();
   for (;Pkg.end() != true; Pkg++)
   {
      if (Pkg.Section() == 0 && Pkg->VersionList != 0)
	 cout << "Bad section " << Pkg.Name() << endl;
      
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); 
	   Cur.end() != true; Cur++)
      {
	 if (Cur->Priority < 1 || Cur->Priority > 5)
	    cout << "Bad prio " << Pkg.Name() << ',' << Cur.VerStr() << " == " << (int)Cur->Priority << endl;
      }
   }
   return true;
}
									/*}}}*/
// Dump - show everything						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Dump(pkgCache &Cache)
{
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      cout << "Package: " << P.Name() << endl;
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
      {
	 cout << " Version: " << V.VerStr() << endl;
	 cout << "     File: " << V.FileList().File().FileName() << endl;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false; D++)
	    cout << "  Depends: " << D.TargetPkg().Name() << ' ' << D.TargetVer() << endl;
      }      
   }

   for (pkgCache::PkgFileIterator F(Cache); F.end() == false; F++)
   {
      cout << "File: " << F.FileName() << endl;
      cout << " Size: " << F->Size << endl;
      cout << " ID: " << F->ID << endl;
      cout << " Flags: " << F->Flags << endl;
      cout << " Time: " << ctime(&F->mtime) << endl;
   }

   return true;
}
									/*}}}*/
// DumpAvail - Print out the available list				/*{{{*/
// ---------------------------------------------------------------------
/* This is needed to make dpkg --merge happy */
bool DumpAvail(pkgCache &Cache)
{
   unsigned char *Buffer = new unsigned char[Cache.HeaderP->MaxVerFileSize];

   for (pkgCache::PkgFileIterator I = Cache.FileBegin(); I.end() == false; I++)
   {
      if ((I->Flags & pkgCache::Flag::NotSource) != 0)
	 continue;
      
      if (I.IsOk() == false)
      {
	 delete [] Buffer;
	 return _error->Error("Package file %s is out of sync.",I.FileName());
      }
      
      FileFd PkgF(I.FileName(),FileFd::ReadOnly);
      if (_error->PendingError() == true)
      {
	 delete [] Buffer;
	 return false;
      }

      /* Write all of the records from this package file, we search the entire
         structure to find them */
      for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
      {
	 for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
	 {
	    if (V->FileList == 0)
	       continue;
	    if (V.FileList().File() != I)
	       continue;
	    
	    // Read the record and then write it out again.
	    if (PkgF.Seek(V.FileList()->Offset) == false ||
		PkgF.Read(Buffer,V.FileList()->Size) == false ||
		write(STDOUT_FILENO,Buffer,V.FileList()->Size) != V.FileList()->Size)
	    {
	       delete [] Buffer;
	       return false;
	    }	    
	 }
      }
   }
   
   return true;
}
									/*}}}*/
// DoAdd - Perform an adding operation					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoAdd(CommandLine &CmdL)
{
   // Make sure there is at least one argument
   if (CmdL.FileSize() <= 1)
      return _error->Error("You must give at least one file name");
   
   // Open the cache
   FileFd CacheF(_config->FindFile("Dir::Cache::pkgcache"),FileFd::WriteAny);
   if (_error->PendingError() == true)
      return false;
   
   DynamicMMap Map(CacheF,MMap::Public);
   if (_error->PendingError() == true)
      return false;

   OpTextProgress Progress(*_config);
   pkgCacheGenerator Gen(Map,Progress);
   if (_error->PendingError() == true)
      return false;

   unsigned long Length = CmdL.FileSize() - 1;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      Progress.OverallProgress(I - CmdL.FileList,Length,1,"Generating cache");
      Progress.SubProgress(Length);

      // Do the merge
      FileFd TagF(*I,FileFd::ReadOnly);
      debListParser Parser(TagF);
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",*I);
      
      if (Gen.SelectFile(*I) == false)
	 return _error->Error("Problem with SelectFile");
	 
      if (Gen.MergeList(Parser) == false)
	 return _error->Error("Problem with MergeList");
   }

   Progress.Done();
   Stats(Gen.GetCache());
   
   return true;
}
									/*}}}*/
// GenCaches - Call the main cache generator				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GenCaches()
{
   OpTextProgress Progress(*_config);
   
   pkgSourceList List;
   List.ReadMainList();
   return pkgMakeStatusCache(List,Progress);
}
									/*}}}*/
// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   cout << PACKAGE << ' ' << VERSION << " for " << ARCHITECTURE <<
       " compiled on " << __DATE__ << "  " << __TIME__ << endl;
   
   cout << "Usage: apt-cache [options] command" << endl;
   cout << "       apt-cache [options] add file1 [file1 ...]" << endl;
   cout << "       apt-cache [options] showpkg pkg1 [pkg2 ...]" << endl;
   cout << endl;
   cout << "apt-cache is a low-level tool used to manipulate APT's binary" << endl;
   cout << "cache files stored in " << _config->FindFile("Dir::Cache") << endl;
   cout << "It is not ment for ordinary use only as a debug aide." << endl;
   cout << endl;
   cout << "Commands:" << endl;
   cout << "   add - Add an package file to the source cache" << endl;
   cout << "   gencaches - Build both the package and source cache" << endl;
   cout << "   showpkg - Show some general information for a single package" << endl;
   cout << "   stats - Show some basic statistics" << endl;
   cout << "   dump - Show the entire file in a terse form" << endl;
   cout << "   dumpavail - Print an available file to stdout" << endl;
   cout << "   unmet - Show unmet dependencies" << endl;
   cout << "   check - Check the cache a bit" << endl;
   cout << endl;
   cout << "Options:" << endl;
   cout << "  -h   This help text." << endl;
   cout << "  -p=? The package cache. [" << _config->FindFile("Dir::Cache::pkgcache") << ']' << endl;
   cout << "  -s=? The source cache. [" << _config->FindFile("Dir::Cache::srcpkgcache") << ']' << endl;
   cout << "  -q   Disable progress indicator. " << endl;
   cout << "  -c=? Read this configuration file" << endl;
   cout << "  -o=? Set an arbitary configuration option, ie -o dir::cache=/tmp" << endl;
   cout << "See the apt-cache(8) and apt.conf(8) manual pages for more information." << endl;
   return 100;
}
									/*}}}*/
// CacheInitialize - Initialize things for apt-cache			/*{{{*/
// ---------------------------------------------------------------------
/* */
void CacheInitialize()
{
   _config->Set("quiet",0);
   _config->Set("help",false);
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'p',"pkg-cache","Dir::Cache::pkgcache",CommandLine::HasArg},
      {'s',"src-cache","Dir::Cache::srcpkgcache",CommandLine::HasArg},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'i',"important","Important",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};

   CacheInitialize();
   
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
   
   while (1)
   {
      if (strcmp(CmdL.FileList[0],"add") == 0)
      {
	 DoAdd(CmdL);
	 break;
      }

      if (strcmp(CmdL.FileList[0],"gencaches") == 0)
      {
	 GenCaches();
	 break;
      }

      // Open the cache file
      FileFd CacheF(_config->FindFile("Dir::Cache::pkgcache"),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 break;
      
      MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
      if (_error->PendingError() == true)
	 break;
      
      pkgCache Cache(Map);   
      if (_error->PendingError() == true)
	 break;
    
      if (strcmp(CmdL.FileList[0],"showpkg") == 0)
      {
	 DumpPackage(Cache,CmdL);
	 break;
      }

      if (strcmp(CmdL.FileList[0],"stats") == 0)
      {
	 Stats(Cache);
	 break;
      }
      
      if (strcmp(CmdL.FileList[0],"dump") == 0)
      {
	 Dump(Cache);
	 break;
      }
      
      if (strcmp(CmdL.FileList[0],"dumpavail") == 0)
      {
	 DumpAvail(Cache);
	 break;
      }
      
      if (strcmp(CmdL.FileList[0],"unmet") == 0)
      {
	 UnMet(Cache);
	 break;
      }

      if (strcmp(CmdL.FileList[0],"check") == 0)
      {
	 Check(Cache);
	 break;
      }
            
      _error->Error("Invalid operation %s", CmdL.FileList[0]);
      break;
   }
   
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
          
   return 0;
}
