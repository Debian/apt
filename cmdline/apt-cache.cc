// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cache.cc,v 1.7 1998/09/22 05:30:30 jgg Exp $
/* ######################################################################
   
   apt-cache - Manages the cache file.
   
   This program should eventually handle both low and high level
   manipulation of the cache file. Depending how far things go it 
   might get quite a sophisticated UI.
   
   Currently the command line is as follows:
      apt-cache add cache file1:dist:ver file2:dist:ver ...
    ie:
      apt-cache add ./cache Pacakges:hamm:1.0

   A usefull feature is 'upgradable' ie
      apt-cache upgradable ./cache
   will list .debs that should be installed to make all packages the latest
   version.
   
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
#include <fstream.h>

									/*}}}*/

string CacheFile;

// SplitArg - Split the triple						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SplitArg(const char *Arg,string &File,string &Dist,string Ver)
{
   const char *Start = Arg;
   const char *I = Arg;
   for (;*I != 0 && *I != ':'; I++);
   if (*I != ':')
      return _error->Error("Malformed argument %s, must be in file:dist:rev form",Arg);
   File = string(Start,I - Start);

   I++;
   Start = I;
   for (;*I != 0 && *I != ':'; I++);
   if (*I != ':')
      return _error->Error("Malformed argument %s, must be in file:dist:rev form",Arg);
   Dist = string(Start,I - Start);
   
   I++;
   Start = I;
   for (;*I != 0 && *I != ':'; I++);
   if (I == Start)
      return _error->Error("Malformed argument %s, must be in file:dist:rev form",Arg);
   Ver = string(Start,I - Start);

   return true;
}
									/*}}}*/
// DumpPackage - Show a dump of a package record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DumpPackage(pkgCache &Cache,int argc,const char *argv[])
{   
   for (int I = 0; I != argc; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(argv[I]);
      if (Pkg.end() == true)
      {
	 _error->Warning("Unable to locate package %s",argv[0]);
	 continue;
      }

      cout << "Package: " << Pkg.Name() << endl;
      cout << "Versions: ";
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
	 cout << Cur.VerStr() << ',';
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
bool DoAdd(int argc,const char *argv[])
{
   string FileName;
   string Dist;
   string Ver;
   
   // Open the cache
   FileFd CacheF(CacheFile,FileFd::WriteEmpty);
   if (_error->PendingError() == true)
      return false;
   
   DynamicMMap Map(CacheF,MMap::Public);
   if (_error->PendingError() == true)
      return false;

   OpTextProgress Progress;
   pkgCacheGenerator Gen(Map,Progress);
   if (_error->PendingError() == true)
      return false;

   for (int I = 0; I != argc; I++)
   {
      Progress.OverallProgress(I,argc,1,"Generating cache");
      if (SplitArg(argv[I],FileName,Dist,Ver) == false)
	 return false;
      
      // Do the merge
      FileFd TagF(FileName.c_str(),FileFd::ReadOnly);
      debListParser Parser(TagF);
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",FileName.c_str());
      
      if (Gen.SelectFile(FileName) == false)
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
   OpTextProgress Progress;
   pkgSourceList List;
   List.ReadMainList();
   return pkgMakeStatusCache(List,Progress);  
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {0,0,0,0}};
	 
   CommandLine Cmds(Args,_config);
   if (pkgInitialize(*_config) == false ||
       Cmds.Parse(argc,argv) == false)
   {
      _error->DumpErrors();
      return 100;
   }   
   cout << _config->Find("help") << endl;
   
   // Check arguments.
   if (argc < 3)
   {
      cerr << "Usage is apt-cache add cache file1:dist:ver file2:dist:ver ..." << endl;
      return 100;
   }

   while (1)
   {
      CacheFile = argv[2];
      if (strcmp(argv[1],"add") == 0)
      {
	 DoAdd(argc - 3,argv + 3);
	 break;
      }

      if (strcmp(argv[1],"gencaches") == 0)
      {
	 GenCaches();
	 break;
      }

      // Open the cache file
      FileFd CacheF(CacheFile,FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 break;
      
      MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
      if (_error->PendingError() == true)
	 break;
      
      pkgCache Cache(Map);   
      if (_error->PendingError() == true)
	 break;
    
      if (strcmp(argv[1],"showpkg") == 0)
      {
	 CacheFile = argv[2];
	 DumpPackage(Cache,argc - 3,argv + 3);
	 break;
      }

      if (strcmp(argv[1],"stats") == 0)
      {
	 Stats(Cache);
	 break;
      }
      
      if (strcmp(argv[1],"dump") == 0)
      {
	 Dump(Cache);
	 break;
      }
      
      if (strcmp(argv[1],"dumpavail") == 0)
      {
	 DumpAvail(Cache);
	 break;
      }
            
      _error->Error("Invalid operation %s", argv[1]);
      break;
   }
   
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      _error->DumpErrors();
      return 100;
   }
          
   return 0;
}
