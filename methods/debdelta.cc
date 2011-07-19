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
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include <apti18n.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
using namespace std;

/*}}}*/
/** \brief DebdeltaMethod - TODO: say something about debdelta here!
 * */
class DebdeltaMethod : public pkgAcqMethod {
   bool Debug;
   string DebdeltaFile;
   string FromFile;
   string ToFile;
   string DebpatchOutput;
protected:
   // the main(i.e. most important) method of the debdelta method.
   virtual bool Fetch(FetchItem *Itm);
public:
   DebdeltaMethod() : pkgAcqMethod("1.1", SingleInstance | SendConfig) {};
   void MakeToFile();
};

bool DebdeltaMethod::Fetch(FetchItem *Itm)						/*{{{*/
{
   /// Testing only...
   //FetchResult ResTest;
   //ResTest.Filename = "/home/ishan/devel/apt/testrepo/binary/gcc-4.6-base_4.6.0-7_amd64.deb";
   //URIDone(ResTest);
   //return true; 
   ///
   Debug = true; //_config->FindB("Debug::pkgAcquire::Debdelta", false);
   FromFile = Itm->DestFile;
   URI U(Itm->Uri);
   DebdeltaFile = U.Path;
   
   if (flExtension(FromFile) != "deb" || !FileExists(FromFile))
      FromFile = "/";
   if (!FileExists(DebdeltaFile))
      return _error->Error("\n[Debdelta] Could not find a debdelta file.");
   MakeToFile();
   if (FileExists(ToFile))
      return _error->Error("\n[Debdelta] New .deb already exists.");

   if (Debug == true)
   {
      std::cerr << "\n[Debdelta] FromFile: " << FromFile
	        << "\n           ToFile: " << ToFile
	        << "\n           DebdelatFile: " << DebdeltaFile << std::endl;
   }
   
   int Fd[2];
   if (pipe(Fd) != 0)
      return _error->Error("[Debdelta] Could not create the pipe.");
   pid_t Process = fork();
   if (Process == 0)
   {
      // redirect debpatch's stdout,stderr to the pipe 
      close(Fd[0]);
      close(1);
      dup(Fd[1]);
      close(2);
      dup(Fd[1]);
      // make the debpatch command and run it.
      const char* Args[6] = {0};
      Args[0] = "/usr/bin/debpatch";
      if (!FileExists(Args[0]))
	 return _error->Error("[Debdelta] Could not find debpatch.");
      Args[1] = "-A";
      Args[2] = DebdeltaFile.c_str();
      Args[3] = FromFile.c_str();
      Args[4] = ToFile.c_str();
      if (Debug == true)
      {
	 std::cerr << "\n[Debdelta] Command:" << std::endl;
	 std::cerr << Args[0] << " " << Args[1] << " " << Args[2] << " " << Args[3] << " "
                   << Args[4] << std::endl;
      }
      std::cerr << "\n\n[Debdelta] Patching " << ToFile << "..." << std::endl;
      execv(Args[0], (char **)Args);
      close(Fd[1]);
   }
   else if (Process != -1)
   {
      int status;
      int options = 0;
      if (Process !=  waitpid(Process, &status, options))
	 return _error->Error("[Debdelta] debpatch did not return normally.");
      
      // read the stderr,stdout outputs of debpatch
      size_t LineSize = 1024;
      char *Line = (char *)malloc(LineSize + 1);
      close(Fd[1]);
      //close(0);
      //dup(Fd[0]);
      FILE *fp = fdopen(Fd[0], "r");
      DebpatchOutput = "";
      while (getline(&Line, &LineSize, fp) != EOF)
	 DebpatchOutput += string(Line);
      fclose(fp);
	 
      if (!FileExists(ToFile))
	 return _error->Error("\n[Debdelta] Failed to patch %s", ToFile.c_str());
      // move the .deb to Dir::Cache::Archives
      string FinalFile = _config->FindDir("Dir::Cache::Archives") + flNotDir(ToFile);
      Rename(ToFile, FinalFile);
      ToFile = FinalFile;
      FetchResult Res;
      Res.Filename = ToFile;
      if (Queue != 0)
	 URIDone(Res);
      else
	 std::cout << "Filename: " << Res.Filename << std::endl;
   }
   else
   {
      return _error->Error("[Debdelta] forking failed.");
   }
   return true;
}


void DebdeltaMethod::MakeToFile()
{
   string DebdeltaName = flNotDir(DebdeltaFile);
   int NewBegin = DebdeltaName.find("_", 0);
   string PkgName = DebdeltaName.substr(0, NewBegin); 
   NewBegin = DebdeltaName.find("_", NewBegin + 1);
   int NewEnd = DebdeltaName.find("_", NewBegin + 1);
   string NewVersion = DebdeltaName.substr(NewBegin + 1, NewEnd - NewBegin - 1);
   string Arch = DebdeltaName.substr(NewEnd + 1, DebdeltaName.find(".", NewEnd + 1) - NewEnd - 1);
   ToFile = _config->FindDir("Dir::Cache::Archives") + "partial/" 
      + PkgName + "_" + NewVersion + "_" + Arch + ".deb";
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
   bool Run(char const *DebdeltaFile, char const *FromFile)
   {
      if (pkgInitConfig(*_config) == false ||
          pkgInitSystem(*_config,_system) == false)
      {
	 std::cerr << "[Debdelta] E: Could not initialize the system/configuration." << std::endl;
	 _error->DumpErrors();
	 return 100;
      }
      _config->CndSet("Debug::pkgAcquire::Debdetla", "true");
      FetchItem *Test = new FetchItem;
      Test->DestFile = FromFile;
      Test->Uri = "debdelta://" + string(DebdeltaFile);
      Test->FailIgnore = false;
      Test->IndexFile = false;
      Test->Next = 0;
     
      return Fetch(Test);  
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

