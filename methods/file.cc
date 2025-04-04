// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   File URI method for APT
   
   This simply checks that the file specified exists, if so the relevant
   information is returned. If a .gz filename is specified then the file
   name with .gz removed will also be checked and information about it
   will be returned in Alt-*
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <string>
#include <sys/stat.h>

#include <apti18n.h>
									/*}}}*/

class FileMethod final : public aptMethod
{
   bool URIAcquire(std::string const &Message, FetchItem *Itm) override;

   public:
   FileMethod() : aptMethod("file", "1.0", SingleInstance | SendConfig | LocalOnly | SendURIEncoded)
   {
      SeccompFlags = aptMethod::BASE;
   }
};

// FileMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileMethod::URIAcquire(std::string const &Message, FetchItem *Itm)
{
   URI Get(Itm->Uri);
   if (Get.Host.empty() == false)
      return _error->Error(_("Invalid URI, local URIS must not start with //"));

   std::vector<std::string> Files;
   Files.emplace_back(DecodeSendURI(Get.Path));
   for (auto const &AltPath : VectorizeString(LookupTag(Message, "Alternate-Paths"), '\n'))
      Files.emplace_back(CombineWithAlternatePath(flNotFile(Files[0]), DecodeSendURI(AltPath)));

   FetchResult Res;
   // deal with destination files which might linger around
   struct stat Buf;
   if (lstat(Itm->DestFile.c_str(), &Buf) == 0 && S_ISLNK(Buf.st_mode) && Buf.st_size > 0)
   {
      char name[Buf.st_size + 1];
      if (ssize_t const sp = readlink(Itm->DestFile.c_str(), name, Buf.st_size); sp == -1)
      {
	 Itm->LastModified = 0;
	 RemoveFile("file", Itm->DestFile);
      }
   }

   int olderrno = 0;
   // See if the file exists
   for (auto const &File : Files)
   {
      if (stat(File.c_str(), &Buf) == 0)
      {
	 Res.Size = Buf.st_size;
	 Res.Filename = File;
	 Res.LastModified = Buf.st_mtime;
	 Res.IMSHit = false;
	 if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
	 {
	    auto const filesize = Itm->ExpectedHashes.FileSize();
	    if (filesize != 0 && filesize == Res.Size)
	       Res.IMSHit = true;
	 }
	 break;
      }
      if (olderrno == 0)
	 olderrno = errno;
   }

   if (not Res.IMSHit)
   {
      RemoveFile("file", Itm->DestFile);
      if (not Res.Filename.empty())
      {
	 URIStart(Res);
	 CalculateHashes(Itm, Res);
      }
   }

   // See if the uncompressed file exists and reuse it
   FetchResult AltRes;
   for (auto const &File : Files)
   {
      for (const auto &ext : APT::Configuration::getCompressorExtensions())
      {
	 if (APT::String::Endswith(File, ext))
	 {
	    std::string const unfile = File.substr(0, File.length() - ext.length());
	    if (stat(unfile.c_str(), &Buf) == 0)
	    {
	       AltRes.Size = Buf.st_size;
	       AltRes.Filename = unfile;
	       AltRes.LastModified = Buf.st_mtime;
	       AltRes.IMSHit = false;
	       if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
		  AltRes.IMSHit = true;
	       if (Res.Filename.empty())
	       {
		  URIStart(Res);
		  CalculateHashes(Itm, AltRes);
	       }
	       break;
	    }
	    // no break here as we could have situations similar to '.gz' vs '.tar.gz' here
	 }
      }
      if (not AltRes.Filename.empty())
	 break;
   }

   if (not AltRes.Filename.empty())
      URIDone(Res,&AltRes);
   else if (not Res.Filename.empty())
      URIDone(Res);
   else
   {
      errno = olderrno;
      return _error->Errno(Files[0].c_str(), _("File not found"));
   }

   return true;
}
									/*}}}*/

int main()
{
   return FileMethod().Run();
}
