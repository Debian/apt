// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cache.cc,v 1.2 1998/07/16 06:08:43 jgg Exp $
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
// DoAdd - Perform an adding operation					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoAdd(int argc,char *argv[])
{
   string FileName;
   string Dist;
   string Ver;
   
   File CacheF(CacheFile,File::WriteEmpty);
   if (_error->PendingError() == true)
      return false;
   
   DynamicMMap Map(CacheF,MMap::Public);
   if (_error->PendingError() == true)
      return false;
   
   pkgCacheGenerator Gen(Map);
   if (_error->PendingError() == true)
      return false;

   for (int I = 0; I != argc; I++)
   {
      if (SplitArg(argv[I],FileName,Dist,Ver) == false)
	 return false;
      cout << FileName << endl;
      
      // Do the merge
      File TagF(FileName.c_str(),File::ReadOnly);
      debListParser Parser(TagF);
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",FileName.c_str());
      
      if (Gen.SelectFile(FileName) == false)
	 return _error->Error("Problem with SelectFile");
	 
      if (Gen.MergeList(Parser) == false)
	 return _error->Error("Problem with MergeList");
   }
   
   return true;
}
									/*}}}*/
// DumpPackage - Show a dump of a package record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DumpPackage(int argc,char *argv[])
{
   File CacheF(CacheFile,File::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   pkgCache Cache(Map);   
   if (_error->PendingError() == true)
      return false;
   
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
bool Stats(const char *FileName)
{
   File CacheF(FileName,File::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   pkgCache Cache(Map);   
   if (_error->PendingError() == true)
      return false;
       
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
bool Dump()
{
   File CacheF(CacheFile,File::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   pkgCache Cache(Map);   
   if (_error->PendingError() == true)
      return false;

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
bool DumpAvail()
{
#if 0
   pkgCache Cache(CacheFile,true,true);
   if (_error->PendingError() == true)
      return false;

   pkgControlCache CCache(Cache);
   if (_error->PendingError() == true)
      return false;

   vector<string> Lines;
   Lines.reserve(30);
   
   pkgCache::PkgIterator I = Cache.PkgBegin();
   for (;I.end() != true; I++)
   {
      if (I->VersionList == 0)
	 continue;
      
      pkgSPkgCtrlInfo Inf = CCache[I.VersionList()];
      if (Inf.isNull() == true)
	 return _error->Error("Couldn't locate info record");
      
      // Iterate over each element
      pkgPkgCtrlInfo::const_iterator Elm = Inf->begin();
      for (; Elm != Inf->end(); Elm++)
      {
	 // Write the tag: value
	 cout << (*Elm)->Tag() << ": " << (*Elm)->Value() << endl;
	 
	 // Write the multiline
	 (*Elm)->GetLines(Lines);
	 for (vector<string>::iterator j = Lines.begin(); j != Lines.end(); j++)
	 {
	    if ((*j).length() == 0)
	       cout << " ." << endl;
	    else
	       cout << " " << *j << endl;
	 }
	 
	 Lines.erase(Lines.begin(),Lines.end());
      }
      
      cout << endl;
   }
#endif
   return true;
}
									/*}}}*/

int main(int argc, char *argv[])
{
   // Check arguments.
   if (argc < 3)
   {
      cerr << "Usage is apt-cache add cache file1:dist:ver file2:dist:ver ..." << endl;
      return 100;
   }

   pkgInitialize(*_config);
   while (1)
   {
      if (strcmp(argv[1],"add") == 0)
      {
	 CacheFile = argv[2];
	 if (DoAdd(argc - 3,argv + 3) == true) 
	    Stats(argv[2]);
	 break;
      }
    
      if (strcmp(argv[1],"showpkg") == 0)
      {
	 CacheFile = argv[2];
	 DumpPackage(argc - 3,argv + 3);
	 break;
      }

      if (strcmp(argv[1],"stats") == 0)
      {
	 Stats(argv[2]);
	 break;
      }
      
      if (strcmp(argv[1],"dump") == 0)
      {
	 CacheFile = argv[2];
	 Dump();
	 break;
      }
      
      if (strcmp(argv[1],"dumpavail") == 0)
      {
	 CacheFile = argv[2];
	 DumpAvail();
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
