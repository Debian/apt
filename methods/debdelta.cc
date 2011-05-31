// Includes									/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include <apti18n.h>

/*}}}*/
/** \brief DebdeltaMethod - TODO: say something about debdelta here!
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
   std::cerr << "Starting DebdeltaMethod::Fetch()." << std::endl;
   Debug = false;//_config->FindB("Debug::pkgAcquire::RRed", false);
   string oldDebFile = Itm->DestFile;
   string debDeltaFile = Itm->Uri;
   
   if (debDeltaFile.empty())
      return _error->Error("Could not find a debdelta file.");
   Itm->DestFile = GetNewPackageName(debDeltaFile);

   pid_t Process = ExecFork();      
   if (Process == 0)
   {
      const char* Args[6] = {0};
      Args[0] = "/usr/bin/debpatch";
      if (!FileExists(Args[0]))
	 return _error->Error("Could not find debpatch.");
      Args[1] = "-A";
      Args[2] = debDeltaFile.c_str();
      if (oldDebFile.empty())
         Args[3] = "/";
      else
         Args[3] = oldDebFile.c_str();
      Args[4] = Itm->DestFile.c_str();
      if (Debug == true)
      {
	 std::cerr << "Debdelta command:" << std::endl;
	 std::cerr << Args[0] << " " << Args[1] << " " << Args[2] << " " << Args[3] << " "
                   << Args[4] << std::endl;
      }
      execv(Args[0], (char **)Args);
   }
   return ExecWait(Process, "debpatch");
}

/**
 * \brief Receives a debdelta file name in the form of path/P_O_N_A.debdelta and constructs the name
 * of the deb with the newer version.
 * @param debdeltaName the name of the debdelta file.
 * @return path/P_N_A.deb
 */
string DebdeltaMethod::GetNewPackageName(string debdeltaName)
{
   int slashpos = debdeltaName.rfind("/", debdeltaName.length() - 1);
   string path = debdeltaName.substr(0, slashpos + 1);
   debdeltaName = debdeltaName.substr(slashpos + 1, debdeltaName.length() - 1);
   int newBegin = debdeltaName.find("_", 0);
   string pckgName = debdeltaName.substr(0, newBegin); 
   newBegin = debdeltaName.find("_", newBegin + 1);
   int newEnd = debdeltaName.find("_", newBegin + 1);
   string newVersion = debdeltaName.substr(newBegin + 1, newEnd - newBegin - 1);
   string arch = debdeltaName.substr(newEnd + 1, debdeltaName.find(".", newEnd + 1) - newEnd - 1);
   string debname = pckgName + "_" + newVersion + "_" + arch + ".deb";
   return path+debname;
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
      //_config->CndSet("Debug::pkgAcquire::RRed", "true");
      FetchItem *test = new FetchItem;
      test->DestFile = debFile;
      test->Uri = debdeltaFile;
      test->FailIgnore = false;
      test->IndexFile = false;
      test->Next = 0;
      return Fetch(test);
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
   // /home/ishan/devel/apt/testrepo/testitems/cpp-4.6_4.6.0-2_amd64.deb
   // /home/ishan/devel/apt/testrepo/testitems/cpp-4.6_4.6.0-2_4.6.0-7_amd64.debdelta
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

