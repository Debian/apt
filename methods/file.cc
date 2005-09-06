// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: file.cc,v 1.9.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   File URI method for APT
   
   This simply checks that the file specified exists, if so the relevent
   information is returned. If a .gz filename is specified then the file
   name with .gz removed will also be checked and information about it
   will be returned in Alt-*
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/fileutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <apti18n.h>
									/*}}}*/

class FileMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   FileMethod() : pkgAcqMethod("1.0",SingleInstance | LocalOnly) {};
};

// FileMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string File = Get.Path;
   FetchResult Res;
   if (Get.Host.empty() == false)
      return _error->Error(_("Invalid URI, local URIS must not start with //"));

   // See if the file exists
   struct stat Buf;
   if (stat(File.c_str(),&Buf) == 0)
   {
      Res.Size = Buf.st_size;
      Res.Filename = File;
      Res.LastModified = Buf.st_mtime;
      Res.IMSHit = false;
      if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
	 Res.IMSHit = true;
   }
   
   // See if we can compute a file without a .gz exentsion
   string::size_type Pos = File.rfind(".gz");
   if (Pos + 3 == File.length())
   {
      File = string(File,0,Pos);
      if (stat(File.c_str(),&Buf) == 0)
      {
	 FetchResult AltRes;
	 AltRes.Size = Buf.st_size;
	 AltRes.Filename = File;
	 AltRes.LastModified = Buf.st_mtime;
	 AltRes.IMSHit = false;
	 if (Itm->LastModified == Buf.st_mtime && Itm->LastModified != 0)
	    AltRes.IMSHit = true;
	 
	 URIDone(Res,&AltRes);
	 return true;
      }      
   }
   
   if (Res.Filename.empty() == true)
      return _error->Error(_("File not found"));

   Hashes Hash;
   FileFd Fd(Res.Filename, FileFd::ReadOnly);
   Hash.AddFD(Fd.Fd(), Fd.Size());
   Res.TakeHashes(Hash);
   URIDone(Res);
   return true;
}
									/*}}}*/

int main()
{
   setlocale(LC_ALL, "");

   FileMethod Mth;
   return Mth.Run();
}
