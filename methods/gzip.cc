// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.9 1999/12/10 23:40:29 jgg Exp $
/* ######################################################################

   GZip method - Take a file URI in and decompress it into the target 
   file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
									/*}}}*/

class GzipMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   GzipMethod() : pkgAcqMethod("1.0",SingleInstance | SendConfig) {};
};

// GzipMethod::Fetch - Decompress the passed URI			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GzipMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;

   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);
   
   // Open the source and destintation files
   FileFd From(Get.Path,FileFd::ReadOnly);
   FileFd To(Itm->DestFile,FileFd::WriteEmpty);   
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
   if (ExecWait(Process,_config->Find("Dir::bin::gzip","gzip").c_str(),false) == false)
   {
      To.OpFail();
      return false;
   }  
       
   To.Close();
   
   // Transfer the modification times
   struct stat Buf;
   if (stat(Get.Path.c_str(),&Buf) != 0)
      return _error->Errno("stat","Failed to stat");

   struct utimbuf TimeBuf;
   TimeBuf.actime = Buf.st_atime;
   TimeBuf.modtime = Buf.st_mtime;
   if (utime(Itm->DestFile.c_str(),&TimeBuf) != 0)
      return _error->Errno("utime","Failed to set modification time");

   if (stat(Itm->DestFile.c_str(),&Buf) != 0)
      return _error->Errno("stat","Failed to stat");
   
   // Return a Done response
   Res.LastModified = Buf.st_mtime;
   Res.Size = Buf.st_size;
   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main()
{
   GzipMethod Mth;
   return Mth.Run();
}
