// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: file.cc,v 1.5 1998/11/01 05:27:41 jgg Exp $
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

#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

class FileMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   FileMethod() : pkgAcqMethod("1.0",SingleInstance) {};
};

// FileMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string File = Get.Path;
   FetchResult Res;
   
   // See if the file exists
   struct stat Buf;
   if (stat(File.c_str(),&Buf) == 0)
   {
      Res.Size = Buf.st_size;
      Res.Filename = File;
      Res.LastModified = Buf.st_mtime;
      Res.IMSHit = false;
      if (Itm->LastModified == Buf.st_mtime)
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
	 if (Itm->LastModified == Buf.st_mtime)
	    AltRes.IMSHit = true;
	 
	 URIDone(Res,&AltRes);
	 return true;
      }      
   }
   
   if (Res.Filename.empty() == true)
      return _error->Error("File not found");
   
   URIDone(Res);
   return true;
}
									/*}}}*/

int main()
{
   FileMethod Mth;
   return Mth.Run();
}
