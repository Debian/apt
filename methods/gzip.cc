// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.12 2001/03/06 07:15:29 jgg Exp $
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
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
									/*}}}*/

class GzipMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   GzipMethod() : pkgAcqMethod("1.1",SingleInstance | SendConfig) {};
};


// GzipMethod::Fetch - Decompress the passed URI			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GzipMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);
   
   // Open the source and destination files
   FileFd From(Path,FileFd::ReadOnly);

   int GzOut[2];   
   if (pipe(GzOut) < 0)
      return _error->Errno("pipe","Couldn't open pipe for gzip");

   // Fork gzip
   int Process = ExecFork();
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
      Args[0] = _config->Find("Dir::bin::gzip","gzip").c_str();
      Args[1] = "-d";
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      _exit(100);
   }
   From.Close();
   close(GzOut[1]);
   
   FileFd FromGz(GzOut[0]);  // For autoclose   
   FileFd To(Itm->DestFile,FileFd::WriteEmpty);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   // Read data from gzip, generate checksums and write
   Hashes Hash;
   bool Failed = false;
   while (1) 
   {
      unsigned char Buffer[4*1024];
      unsigned long Count;
      
      Count = read(GzOut[0],Buffer,sizeof(Buffer));
      if (Count < 0 && errno == EINTR)
	 continue;
      
      if (Count < 0)
      {
	 _error->Errno("read", "Read error from gzip process");
	 Failed = true;
	 break;
      }
      
      if (Count == 0)
	 break;
      
      Hash.Add(Buffer,Count);
      To.Write(Buffer,Count);
   }
   
   // Wait for gzip to finish
   if (ExecWait(Process,_config->Find("Dir::bin::gzip","gzip").c_str(),false) == false)
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
   Res.MD5Sum = Hash.MD5.Result();

   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main()
{
   GzipMethod Mth;
   return Mth.Run();
}
