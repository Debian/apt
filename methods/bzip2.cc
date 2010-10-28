// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Bzip2 method - Take a file URI in and decompress it into the target 
   file.

   While the method is named "bzip2" it handles also other compression
   types as it calls binaries based on the name of the method,
   so it can also be used to handle gzip, lzma and others if named
   correctly.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <apti18n.h>
									/*}}}*/

const char *Prog;

class Bzip2Method : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   Bzip2Method() : pkgAcqMethod("1.1",SingleInstance | SendConfig) {};
};


// Bzip2Method::Fetch - Decompress the passed URI			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Bzip2Method::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths
   
   string GzPathOption = "Dir::bin::"+string(Prog);

   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);
   
   // Open the source and destination files
   FileFd From(Path,FileFd::ReadOnly);

   // FIXME add an error message saying that empty files can't be valid archives
   if(From.FileSize() == 0)
      return false;

   int GzOut[2];   
   if (pipe(GzOut) < 0)
      return _error->Errno("pipe",_("Couldn't open pipe for %s"),Prog);

   // Fork bzip2
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      close(GzOut[0]);
      dup2(From.Fd(),STDIN_FILENO);
      dup2(GzOut[1],STDOUT_FILENO);
      From.Close();
      close(GzOut[1]);
      SetCloseExec(STDIN_FILENO,false);
      SetCloseExec(STDOUT_FILENO,false);
      
      const char *Args[3];
      string Tmp = _config->Find(GzPathOption,Prog);
      Args[0] = Tmp.c_str();
      Args[1] = "-d";
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      _exit(100);
   }
   From.Close();
   close(GzOut[1]);
   
   FileFd FromGz(GzOut[0]);  // For autoclose   
   FileFd To(Itm->DestFile,FileFd::WriteAtomic);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   // Read data from bzip2, generate checksums and write
   Hashes Hash;
   bool Failed = false;
   while (1) 
   {
      unsigned char Buffer[4*1024];
      
      ssize_t Count = read(GzOut[0],Buffer,sizeof(Buffer));
      if (Count < 0 && errno == EINTR)
	 continue;
      
      if (Count < 0)
      {
	 _error->Errno("read", _("Read error from %s process"),Prog);
	 Failed = true;
	 break;
      }
      
      if (Count == 0)
	 break;
      
      Hash.Add(Buffer,Count);
      if (To.Write(Buffer,Count) == false)
      {
	 Failed = true;
	 FromGz.Close();
	 break;
      }      
   }
   
   // Wait for bzip2 to finish
   if (ExecWait(Process,_config->Find(GzPathOption,Prog).c_str(),false) == false)
   {
      To.OpFail();
      return false;
   }  
       
   To.Close();
   
   if (Failed == true)
      return false;
   
   // Transfer the modification times
   struct stat Buf;
   if (stat(Path.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   struct utimbuf TimeBuf;
   TimeBuf.actime = Buf.st_atime;
   TimeBuf.modtime = Buf.st_mtime;
   if (utime(Itm->DestFile.c_str(),&TimeBuf) != 0)
      return _error->Errno("utime",_("Failed to set modification time"));

   if (stat(Itm->DestFile.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));
   
   // Return a Done response
   Res.LastModified = Buf.st_mtime;
   Res.Size = Buf.st_size;
   Res.TakeHashes(Hash);

   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main(int argc, char *argv[])
{
   setlocale(LC_ALL, "");

   Bzip2Method Mth;

   Prog = strrchr(argv[0],'/');
   Prog++;
   
   return Mth.Run();
}
