// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.3 1998/10/30 07:53:53 jgg Exp $
/* ######################################################################

   GZip method - Take a file URI in and decompress it into the target 
   file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <strutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <wait.h>
#include <stdio.h>
									/*}}}*/

class GzipMethod : public pkgAcqMethod
{
   virtual bool Fetch(string Message,URI Get);
   
   public:
   
   GzipMethod() : pkgAcqMethod("1.0",SingleInstance | SendConfig) {};
};

// GzipMethod::Fetch - Decompress the passed URI			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GzipMethod::Fetch(string Message,URI Get)
{
   // Open the source and destintation files
   FileFd From(Get.Path,FileFd::ReadOnly);
   FileFd To(DestFile,FileFd::WriteEmpty);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   // Fork gzip
   int Process = fork();
   if (Process < 0)
      return _error->Errno("fork","Couldn't fork gzip");
   
   // The child
   if (Process == 0)
   {
      dup2(From.Fd(),STDIN_FILENO);
      dup2(To.Fd(),STDOUT_FILENO);
      From.Close();
      To.Close();
      SetCloseExec(STDIN_FILENO,false);
      SetCloseExec(STDOUT_FILENO,false);
      
      const char *Args[3];
      Args[0] = _config->Find("Dir::bin::gzip","gzip").c_str();
      Args[1] = "-d";
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      exit(100);
   }
   From.Close();
   
   // Wait for gzip to finish
   int Status;
   if (waitpid(Process,&Status,0) != Process)
   {
      To.OpFail();
      return _error->Errno("wait","Waiting for gzip failed");
   }	 
   
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
   {
      To.OpFail();
      return _error->Error("gzip failed, perhaps the disk is full or the directory permissions are wrong.");
   }	 
   
   To.Close();
   
   // Transfer the modification times
   struct stat Buf;
   if (stat(Get.Path.c_str(),&Buf) != 0)
      return _error->Errno("stat","Failed to stat");

   struct utimbuf TimeBuf;
   TimeBuf.actime = Buf.st_atime;
   TimeBuf.modtime = Buf.st_mtime;
   if (utime(DestFile.c_str(),&TimeBuf) != 0)
      return _error->Errno("utime","Failed to set modification time");

   // Return a Done response
   FetchResult Res;
   Res.LastModified = Buf.st_mtime;
   Res.Filename = DestFile;
   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main()
{
   GzipMethod Mth;
   return Mth.Run();
}
