#include <apt-pkg/dpkgdb.h>
#include <apt-pkg/debfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/extract.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>

#include <stdio.h>
#include <stdlib.h>

using namespace std;

bool Go(int argc,char *argv[])
{
   // Init the database
   debDpkgDB Db;   
   {
      OpTextProgress Prog;

      if (Db.ReadyPkgCache(Prog) == false)
	 return false;
      Prog.Done();
      
      if (Db.ReadyFileList(Prog) == false)
	 return false;
   }

   for (int I = 1; I < argc; I++)
   {
      const char *Fake = 0;
      for (unsigned J = 0; argv[I][J] != 0; J++)
      {
	 if (argv[I][J] != ',')
	    continue;
	 Fake = argv[I] + J + 1;
	 argv[I][J] = 0;
      }
      
      FileFd F(argv[I],FileFd::ReadOnly);
      debDebFile Deb(F);

      if (_error->PendingError() == true)
	 return false;
      
      if (Deb.ExtractControl(Db) == false)
	 return false;
      cout << argv[I] << endl;
      
      pkgCache::VerIterator Ver = Deb.MergeControl(Db);
      if (Ver.end() == true)
	 return false;
      
      cout << Ver.ParentPkg().Name() << ' ' << Ver.VerStr() << endl;
      
      pkgExtract Extract(Db.GetFLCache(),Ver);
      
      if (Fake != 0)
      {
	 pkgExtract::Item Itm;
	 memset(&Itm,0,sizeof(Itm));
	 FILE *F = fopen(Fake,"r");
	 while (feof(F) == 0)
	 {
	    char Line[300];
	    fgets(Line,sizeof(Line),F);
	    Itm.Name = _strstrip(Line);
	    Itm.Type = pkgDirStream::Item::File;
	    if (Line[strlen(Line)-1] == '/')
	       Itm.Type = pkgDirStream::Item::Directory;

	    int Fd;
	    if (Extract.DoItem(Itm,Fd) == false)
	       return false;
	 }	 
      }
      else
	 if (Deb.ExtractArchive(Extract) == false)
	    return false;
   }
   return true;
}

int main(int argc,char *argv[])
{
   pkgInitConfig(*_config);
   pkgInitSystem(*_config,_system);
   _config->Set("Dir::State::status","/tmp/testing/status");

   Go(argc,argv);
   
   if (_error->PendingError() == true)
   {
      _error->DumpErrors();
      return 0;
   }
}
