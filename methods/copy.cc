// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: copy.cc,v 1.4 1998/10/30 07:53:51 jgg Exp $
/* ######################################################################

   Copy URI - This method takes a uri like a file: uri and copies it
   to the destination file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>

#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
									/*}}}*/

class CopyMethod : public pkgAcqMethod
{
   virtual bool Fetch(string Message,URI Get);
   
   public:
   
   CopyMethod() : pkgAcqMethod("1.0",SingleInstance) {};
};

// CopyMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CopyMethod::Fetch(string Message,URI Get)
{
   string File = Get.Path;

   // See if the file exists
   FileFd From(File,FileFd::ReadOnly);
   FileFd To(DestFile,FileFd::WriteEmpty);
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   // Copy the file
   if (CopyFile(From,To) == false)
      return false;

   From.Close();
   To.Close();
   
   // Transfer the modification times
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
   {
      To.OpFail();
      return _error->Errno("stat","Failed to stat");
   }
   
   struct utimbuf TimeBuf;
   TimeBuf.actime = Buf.st_atime;
   TimeBuf.modtime = Buf.st_mtime;
   if (utime(DestFile.c_str(),&TimeBuf) != 0)
   {
      To.OpFail();
      return _error->Errno("utime","Failed to set modification time");
   }
   
   // Forumulate a result
   FetchResult Res;
   Res.Size = Buf.st_size;
   Res.Filename = DestFile;
   Res.LastModified = Buf.st_mtime;
   Res.IMSHit = false;
   
   URIDone(Res);
   return true;
}
									/*}}}*/

int main()
{
   CopyMethod Mth;
   return Mth.Run();
}
