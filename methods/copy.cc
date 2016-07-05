// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: copy.cc,v 1.7.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Copy URI - This method takes a uri like a file: uri and copies it
   to the destination file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/configuration.h>
#include "aptmethod.h"

#include <string>
#include <sys/stat.h>
#include <sys/time.h>

#include <apti18n.h>
									/*}}}*/

class CopyMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;

   public:

   CopyMethod() : aptMethod("copy", "1.0",SingleInstance | SendConfig) {};
};

// CopyMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CopyMethod::Fetch(FetchItem *Itm)
{
   // this ensures that relative paths work in copy
   std::string const File = Itm->Uri.substr(Itm->Uri.find(':')+1);

   // Stat the file and send a start message
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   // Forumulate a result and send a start message
   FetchResult Res;
   Res.Size = Buf.st_size;
   Res.Filename = Itm->DestFile;
   Res.LastModified = Buf.st_mtime;
   Res.IMSHit = false;
   URIStart(Res);

   // just calc the hashes if the source and destination are identical
   if (File == Itm->DestFile || Itm->DestFile == "/dev/null")
   {
      CalculateHashes(Itm, Res);
      URIDone(Res);
      return true;
   }

   // See if the file exists
   FileFd From(File,FileFd::ReadOnly);
   FileFd To(Itm->DestFile,FileFd::WriteAtomic);
   To.EraseOnFailure();

   // Copy the file
   if (CopyFile(From,To) == false)
   {
      To.OpFail();
      return false;
   }

   From.Close();
   To.Close();

   if (TransferModificationTimes(File.c_str(), Res.Filename.c_str(), Res.LastModified) == false)
      return false;

   CalculateHashes(Itm, Res);
   URIDone(Res);
   return true;
}
									/*}}}*/

int main()
{
   return CopyMethod().Run();
}
