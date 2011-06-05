// Includes									/*{{{*/
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/init.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include <apti18n.h>

/*}}}*/
/** \brief DebdeltaMethod - This method is used to patch a new deb from
 *  an input deb and a debdelta file.
 * */
class DebdeltaMethod : public pkgAcqMethod {
   bool Debug;  

protected:
   // the main(i.e. most important) method of the debdelta method.
   virtual bool Fetch(FetchItem *Itm);
public:
   DebdeltaMethod() : pkgAcqMethod("1.1", SingleInstance | SendConfig) {};
   string GetNewPackageName(string debdeltaName);
};

bool DebdeltaMethod::Fetch(FetchItem *Itm)						/*{{{*/
{
   Debug = _config->FindB("Debug::pkgAcquire::RRed", false);
   string OldDebFile = Itm->DestFile;
   string DebDeltaFile = Itm->Uri;
   
   if (!FileExists(OldDebFile))
      OldDebFile = "/";
   
   if (!FileExists(DebDeltaFile))
      return _error->Error("Could not find a debdelta file.");

   string NewDeb = GetNewPackageName(DebDeltaFile);
   Itm->DestFile = _config->FindDir("Dir::Cache::Archives") + "partial/" + NewDeb;
   if (FileExists(Itm->DestFile))
      return _error->Error("New .deb already exists.");
   
   pid_t Process = ExecFork();      
   if (Process == 0)
   {
      const char* Args[6] = {0};
      Args[0] = "/usr/bin/debpatch";
      if (!FileExists(Args[0]))
	 return _error->Error("Could not find debpatch.");
      Args[1] = "-A";
      Args[2] = DebDeltaFile.c_str();
      Args[3] = OldDebFile.c_str();
      Args[4] = Itm->DestFile.c_str();
      if (Debug == true)
      {
	 std::cerr << "Debdelta command:" << std::endl;
	 std::cerr << Args[0] << " " << Args[1] << " " << Args[2] << " " << Args[3] << " "
                   << Args[4] << std::endl;
      }
      execv(Args[0], (char **)Args);
   }
   if (ExecWait(Process, "debpatch"))
   {
      if (!FileExists(Itm->DestFile))
	 return _error->Error("Failed to patch %s", Itm->DestFile.c_str());
      // move the .deb to Dir::Cache::Archives
      OldDebFile = _config->FindDir("Dir::Cache::Archives") + NewDeb;
      Rename(Itm->DestFile, OldDebFile);
      Itm->DestFile = OldDebFile;
      return true;
   }
   Itm->DestFile = OldDebFile;
   return false;
}

/**
 * \brief Receives a debdelta file name in the form of path/P_O_N_A.debdelta and constructs the name
 * of the deb with the newer version.
 * @param debdeltaName the name of the debdelta file.
 * @return path/P_N_A.deb
 */
string DebdeltaMethod::GetNewPackageName(string DebdeltaName)
{
   int Slashpos = DebdeltaName.rfind("/", DebdeltaName.length() - 1);
   string Path = DebdeltaName.substr(0, Slashpos + 1);
   DebdeltaName = DebdeltaName.substr(Slashpos + 1, DebdeltaName.length() - 1);
   int NewBegin = DebdeltaName.find("_", 0);
   string PkgName = DebdeltaName.substr(0, NewBegin); 
   NewBegin = DebdeltaName.find("_", NewBegin + 1);
   int NewEnd = DebdeltaName.find("_", NewBegin + 1);
   string NewVersion = DebdeltaName.substr(NewBegin + 1, NewEnd - NewBegin - 1);
   string Arch = DebdeltaName.substr(NewEnd + 1, DebdeltaName.find(".", NewEnd + 1) - NewEnd - 1);
   return PkgName + "_" + NewVersion + "_" + Arch + ".deb";
}

/*}}}*/
/** \brief Wrapper class for testing debdelta */					/*{{{*/
class TestDebdeltaMethod : public DebdeltaMethod {
public:
   /** \brief Run debdelta in debug test mode
    *
    *  This method can be used to run the debdelta method outside
    *  of the "normal" acquire environment for easier testing.
    *
    *  \param base basename of all files involved in this debdelta test
    */
   bool Run(char const *debFile, char const *debdeltaFile)
   {
      if (pkgInitConfig(*_config) == false ||
          pkgInitSystem(*_config,_system) == false)
      {
	 std::cerr << "E: Could not initialize the system/configuration." << std::endl;
	 _error->DumpErrors();
	 return 100;
      }
      _config->CndSet("Debug::pkgAcquire::Debdetla", "true");
      FetchItem *test = new FetchItem;
      test->DestFile = debFile;
      test->Uri = debdeltaFile;
      test->FailIgnore = false;
      test->IndexFile = false;
      test->Next = 0;
      if (Fetch(test))
      {
	 std::cout << "Result-Deb: " << test->DestFile << std::endl;
	 return true;
      }
      return false;   
   }
};

/*}}}*/
/** \brief Starter for the debdelta method (or its test method)			{{{
 *
 *  Used without parameters is the normal behavior for methods for
 *  the APT acquire system. While this works great for the acquire system
 *  it is very hard to test the method and therefore the method also
 *  accepts one parameter which will switch it directly to debug test mode:
 */
int main(int argc, char *argv[])
{
   if (argc <= 1)
   {
      DebdeltaMethod Mth;
      return Mth.Run();
   }
   else
   {
      TestDebdeltaMethod Mth;
      bool result = Mth.Run(argv[1], argv[2]);
      _error->DumpErrors();
      return result;
   }
}
/*}}}*/

