// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Copy URI - This method takes a uri like a file: uri and copies it
   to the destination file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <string>
#include <sys/stat.h>
#include <sys/time.h>

#include <apti18n.h>
									/*}}}*/

class CopyMethod final : public aptMethod
{
   bool URIAcquire(std::string const &Message, FetchItem *Itm) override;

   public:
   CopyMethod() : aptMethod("copy", "1.0", SingleInstance | SendConfig | SendURIEncoded)
   {
      SeccompFlags = aptMethod::BASE;
   }
};

// CopyMethod::Fetch - Fetch a file					/*{{{*/
bool CopyMethod::URIAcquire(std::string const &Message, FetchItem *Itm)
{
   struct FileCopyType {
      std::string name;
      struct stat stat{};
      explicit FileCopyType(std::string &&file) : name{std::move(file)} {}
   };
   std::vector<FileCopyType> files;
   // this ensures that relative paths work
   files.emplace_back(DecodeSendURI(Itm->Uri.substr(Itm->Uri.find(':')+1)));
   for (auto const &AltPath : VectorizeString(LookupTag(Message, "Alternate-Paths"), '\n'))
      files.emplace_back(CombineWithAlternatePath(flNotFile(files[0].name), DecodeSendURI(AltPath)));
   files.erase(std::remove_if(files.begin(), files.end(), [](auto &file)
			      { return stat(file.name.c_str(), &file.stat) != 0; }),
	       files.end());
   if (files.empty())
      return _error->Errno("copy-stat", _("Failed to stat"));

   // Forumulate a result and send a start message
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   Res.IMSHit = false;

   for (auto const &File : files)
   {
      Res.Size = File.stat.st_size;
      Res.LastModified = File.stat.st_mtime;

      // just calc the hashes if the source and destination are identical
      if (Itm->DestFile == "/dev/null" || File.name == Itm->DestFile)
      {
	 URIStart(Res);
	 CalculateHashes(Itm, Res);
	 URIDone(Res);
	 return true;
      }

      FileFd From(File.name, FileFd::ReadOnly);
      FileFd To(Itm->DestFile, FileFd::WriteAtomic);
      To.EraseOnFailure();
      if (not From.IsOpen() || not To.IsOpen())
	 continue;

      // Copy the file
      URIStart(Res);
      if (not CopyFile(From, To))
      {
	 To.OpFail();
	 continue;
      }
      From.Close();
      To.Close();

      CalculateHashes(Itm, Res);
      if (not Itm->ExpectedHashes.empty() && Itm->ExpectedHashes != Res.Hashes)
	 continue;

      if (not TransferModificationTimes(File.name.c_str(), Res.Filename.c_str(), Res.LastModified))
	 continue;

      URIDone(Res);
      return true;
   }

   return false;
}
									/*}}}*/

int main()
{
   return CopyMethod().Run();
}
