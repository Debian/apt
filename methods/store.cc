// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Store method - Takes a file URI and stores its content (for which it will
   calculate the hashes) in the given destination. The input file will be
   extracted based on its file extension (or with the given compressor if
   called with one of the compatible symlinks) and potentially recompressed
   based on the file extension of the destination filename.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>
#include "aptmethod.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

class StoreMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;

   public:

   explicit StoreMethod(std::string &&pProg) : aptMethod(std::move(pProg),"1.2",SingleInstance | SendConfig)
   {
      if (Binary != "store")
	 methodNames.insert(methodNames.begin(), "store");
   }
};

static bool OpenFileWithCompressorByName(FileFd &fileFd, std::string const &Filename, unsigned int const Mode, std::string const &Name)
{
   if (Name == "store")
      return fileFd.Open(Filename, Mode, FileFd::Extension);

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   for (; compressor != compressors.end(); ++compressor)
      if (compressor->Name == Name)
	 break;
   if (compressor == compressors.end())
      return _error->Error("Extraction of file %s requires unknown compressor %s", Filename.c_str(), Name.c_str());
   return fileFd.Open(Filename, Mode, *compressor);
}


									/*}}}*/
bool StoreMethod::Fetch(FetchItem *Itm)					/*{{{*/
{
   URI Get = Itm->Uri;
   std::string Path = Get.Host + Get.Path; // To account for relative paths
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   // Open the source and destination files
   FileFd From;
   if (_config->FindB("Method::Compress", false) == false)
   {
      if (OpenFileWithCompressorByName(From, Path, FileFd::ReadOnly, Binary) == false)
	 return false;
      if(From.IsCompressed() && From.FileSize() == 0)
	 return _error->Error(_("Empty files can't be valid archives"));
   }
   else
      From.Open(Path, FileFd::ReadOnly, FileFd::Extension);
   if (From.IsOpen() == false || From.Failed() == true)
      return false;

   FileFd To;
   if (Itm->DestFile != "/dev/null" && Itm->DestFile != Path)
   {
      if (_config->FindB("Method::Compress", false) == false)
	 To.Open(Itm->DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Atomic, FileFd::Extension);
      else if (OpenFileWithCompressorByName(To, Itm->DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Empty, Binary) == false)
	    return false;

      if (To.IsOpen() == false || To.Failed() == true)
	 return false;
      To.EraseOnFailure();
   }

   // Read data from source, generate checksums and write
   Hashes Hash(Itm->ExpectedHashes);
   bool Failed = false;
   Res.Size = 0;
   while (1)
   {
      unsigned char Buffer[4*1024];
      unsigned long long Count = 0;

      if (!From.Read(Buffer,sizeof(Buffer),&Count))
      {
	 if (To.IsOpen())
	    To.OpFail();
	 return false;
      }
      if (Count == 0)
	 break;
      Res.Size += Count;

      Hash.Add(Buffer,Count);
      if (To.IsOpen() && To.Write(Buffer,Count) == false)
      {
	 Failed = true;
	 break;
      }
   }

   From.Close();
   To.Close();

   if (Failed == true)
      return false;

   if (TransferModificationTimes(Path.c_str(), Itm->DestFile.c_str(), Res.LastModified) == false)
      return false;

   // Return a Done response
   Res.TakeHashes(Hash);

   URIDone(Res);
   return true;
}
									/*}}}*/

int main(int, char *argv[])
{
   return StoreMethod(flNotDir(argv[0])).Run();
}
