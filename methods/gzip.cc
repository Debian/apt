// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.17.2.1 2004/01/16 18:58:50 mdz Exp $
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
#include <apti18n.h>
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
   FileFd From(Path,FileFd::ReadOnlyGzip);

   if(From.FileSize() == 0)
      return _error->Error(_("Empty files can't be valid archives"));

   FileFd To(Itm->DestFile,FileFd::WriteAtomic);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   // Read data from source, generate checksums and write
   Hashes Hash;
   bool Failed = false;
   while (1) 
   {
      unsigned char Buffer[4*1024];
      unsigned long Count;
      
      if (!From.Read(Buffer,sizeof(Buffer),&Count))
      {
	 To.OpFail();
	 return false;
      }
      if (Count == 0)
	 break;

      Hash.Add(Buffer,Count);
      if (To.Write(Buffer,Count) == false)
      {
	 Failed = true;
	 break;
      }      
   }
   
   From.Close();
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

   GzipMethod Mth;
   return Mth.Run();
}
