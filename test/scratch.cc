#include <apt-pkg/dpkgdb.h>
#include <apt-pkg/debfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/extract.h>
#include <apt-pkg/init.h>
#include <apt-pkg/fileutl.h>

using namespace std;

int main(int argc,char *argv[])
{
   pkgInitConfig(*_config);
   pkgInitSystem(*_config,_system);

//   cout << flNoLink(argv[1]) << endl;
   
//   #if 0
/*   DynamicMMap *FileMap = new DynamicMMap(MMap::Public);
   pkgFLCache *FList = new pkgFLCache(*FileMap);
   
   char *Name = "/tmp/test";
   pkgFLCache::PkgIterator Pkg(*FList,0);
   pkgFLCache::NodeIterator Node = FList->GetNode(Name,Name+strlen(Name),Pkg.Offset(),true,false);
   cout << (pkgFLCache::Node *)Node << endl;
   Node = FList->GetNode(Name,Name+strlen(Name),Pkg.Offset(),true,false);
   cout << (pkgFLCache::Node *)Node << endl;
*/
//   #if 0
   _config->Set("Dir::State::status","/tmp/testing/status");

   debDpkgDB Db;
   
   {
      OpTextProgress Prog;
      
      if (Db.ReadyPkgCache(Prog) == false)
	 cerr << "Error!" << endl;
      Prog.Done();
      
      if (Db.ReadyFileList(Prog) == false)
	 cerr << "Error!" << endl;
   }
   
   if (_error->PendingError() == true)
   {
      _error->DumpErrors();
      return 0;
   }
   
/*   Db.GetFLCache().BeginDiverLoad();
   pkgFLCache::PkgIterator Pkg(Db.GetFLCache(),0);
   if (Db.GetFLCache().AddDiversion(Pkg,"/usr/include/linux/kerneld.h","/usr/bin/nslookup") == false)
      cerr << "Error!" << endl;

   const char *Tmp = "/usr/include/linux/kerneld.h";
   pkgFLCache::NodeIterator Nde = Db.GetFLCache().GetNode(Tmp,Tmp+strlen(Tmp),0,false,false);
   map_ptrloc Loc = Nde->File;
      
   for (; Nde.end() == false && Nde->File == Loc; Nde++)
      cout << Nde->Flags << ',' << Nde->Pointer << ',' << Nde.File() << endl;
   Db.GetFLCache().FinishDiverLoad();*/

/*   unsigned int I = 0;
   pkgFLCache &Fl = Db.GetFLCache();
   while (I < Fl.HeaderP->HashSize)
   {
      cout << I << endl;
      pkgFLCache::NodeIterator Node(Fl,Fl.NodeP + Fl.HeaderP->FileHash + I++);
      if (Node->Pointer == 0)
	 continue;
      for (; Node.end() == false; Node++)
      {
	 cout << Node.DirN() << '/' << Node.File();
	 if (Node->Flags == pkgFLCache::Node::Diversion)
	    cout << " (div)";
	 if (Node->Flags == pkgFLCache::Node::ConfFile)
	    cout << " (conf)";
	 cout << endl;
      }
   }*/

   for (int I = 1; I < argc; I++)
   {
      FileFd F(argv[I],FileFd::ReadOnly);
      debDebFile Deb(F);
      
      if (Deb.ExtractControl(Db) == false)
	 cerr << "Error!" << endl;
      cout << argv[I] << endl;
      
      pkgCache::VerIterator Ver = Deb.MergeControl(Db);
      if (Ver.end() == true)
	 cerr << "Failed" << endl;
      else
	 cout << Ver.ParentPkg().Name() << ' ' << Ver.VerStr() << endl;
      
      pkgExtract Extract(Db.GetFLCache(),Ver);
      Deb.ExtractArchive(Extract);
   }
//   #endif
//#endif      
   _error->DumpErrors();
}
