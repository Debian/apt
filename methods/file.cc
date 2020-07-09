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

class FileMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;

   public:
   FileMethod() : aptMethod("file", "1.0", SingleInstance | SendConfig | LocalOnly | SendURIEncoded)
   {
      SeccompFlags = aptMethod::BASE;
   }
};

// FileMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileMethod::Fetch(FetchItem *Itm)
{
   URI Get(Itm->Uri);
   std::string const File = DecodeSendURI(Get.Path);
   FetchResult Res;
   if (Get.Host.empty() == false)
      return _error->Error(_("Invalid URI, local URIS must not start with //"));

   struct stat Buf;
   // deal with destination files which might linger around
   if (lstat(Itm->DestFile.c_str(), &Buf) == 0)
   {
      if ((Buf.st_mode & S_IFREG) != 0)
      {
	 if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
	 {
	    if (Itm->ExpectedHashes.VerifyFile(File))
	    {
	       Res.Filename = Itm->DestFile;
	       Res.IMSHit = true;
	    }
	 }
      }
   }
   if (Res.IMSHit != true)
      RemoveFile("file", Itm->DestFile);

   int olderrno = 0;
   // See if the file exists
   if (stat(File.c_str(),&Buf) == 0)
   {
      Res.Size = Buf.st_size;
      Res.Filename = File;
      Res.LastModified = Buf.st_mtime;
      Res.IMSHit = false;
      if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
      {
	 unsigned long long const filesize = Itm->ExpectedHashes.FileSize();
	 if (filesize != 0 && filesize == Res.Size)
	    Res.IMSHit = true;
      }

      CalculateHashes(Itm, Res);
   }
   else
      olderrno = errno;
   if (Res.IMSHit == false)
      URIStart(Res);

   // See if the uncompressed file exists and reuse it
   FetchResult AltRes;
   AltRes.Filename.clear();
   std::vector<std::string> extensions = APT::Configuration::getCompressorExtensions();
   for (std::vector<std::string>::const_iterator ext = extensions.begin(); ext != extensions.end(); ++ext)
   {
      if (APT::String::Endswith(File, *ext) == true)
      {
	 std::string const unfile = File.substr(0, File.length() - ext->length());
	 if (stat(unfile.c_str(),&Buf) == 0)
	 {
	    AltRes.Size = Buf.st_size;
	    AltRes.Filename = unfile;
	    AltRes.LastModified = Buf.st_mtime;
	    AltRes.IMSHit = false;
	    if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
	       AltRes.IMSHit = true;
	    break;
	 }
	 // no break here as we could have situations similar to '.gz' vs '.tar.gz' here
      }
   }

   if (AltRes.Filename.empty() == false)
      URIDone(Res,&AltRes);
   else if (Res.Filename.empty() == false)
      URIDone(Res);
   else
   {
      errno = olderrno;
      return _error->Errno(File.c_str(), _("File not found"));
   }

   return true;
}
									/*}}}*/

int main()
{
   return FileMethod().Run();
}
