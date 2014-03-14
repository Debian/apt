// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.17.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   GZip method - Take a file URI in and decompress it into the target 
   file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>

#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

const char *Prog;

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
   std::string Path = Get.Host + Get.Path; // To account for relative paths
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   for (; compressor != compressors.end(); ++compressor)
      if (compressor->Name == Prog)
	 break;
   if (compressor == compressors.end())
      return _error->Error("Extraction of file %s requires unknown compressor %s", Path.c_str(), Prog);

   // Open the source and destination files
   FileFd From, To;
   if (_config->FindB("Method::Compress", false) == false)
   {
      From.Open(Path, FileFd::ReadOnly, *compressor);
      if(From.FileSize() == 0)
	 return _error->Error(_("Empty files can't be valid archives"));
      To.Open(Itm->DestFile, FileFd::WriteAtomic);
   }
   else
   {
      From.Open(Path, FileFd::ReadOnly);
      To.Open(Itm->DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Empty, *compressor);
   }
   To.EraseOnFailure();

   if (From.IsOpen() == false || From.Failed() == true ||
	 To.IsOpen() == false || To.Failed() == true)
      return false;

   // Read data from source, generate checksums and write
   Hashes Hash;
   bool Failed = false;
   while (1) 
   {
      unsigned char Buffer[4*1024];
      unsigned long long Count = 0;
      
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
   Res.Size = To.FileSize();
   To.Close();

   if (Failed == true)
      return false;

   // Transfer the modification times
   struct stat Buf;
   if (stat(Path.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   struct timeval times[2];
   times[0].tv_sec = Buf.st_atime;
   Res.LastModified = times[1].tv_sec = Buf.st_mtime;
   times[0].tv_usec = times[1].tv_usec = 0;
   if (utimes(Itm->DestFile.c_str(), times) != 0)
      return _error->Errno("utimes",_("Failed to set modification time"));

   // Return a Done response
   Res.TakeHashes(Hash);

   URIDone(Res);
   return true;
}
									/*}}}*/

int main(int, char *argv[])
{
   setlocale(LC_ALL, "");

   Prog = strrchr(argv[0],'/');
   ++Prog;

   GzipMethod Mth;
   return Mth.Run();
}
