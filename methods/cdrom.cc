// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdrom.cc,v 1.13 1999/07/12 03:00:53 jgg Exp $
/* ######################################################################

   CDROM URI method for APT
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

class CDROMMethod : public pkgAcqMethod
{
   Configuration Database;
   bool DatabaseLoaded;
   string CurrentID;
   
   virtual bool Fetch(FetchItem *Itm);
   string GetID(string Name);
   
   public:
   
   CDROMMethod();
};

// CDROMMethod::CDROMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CDROMMethod::CDROMMethod() : pkgAcqMethod("1.0",SingleInstance | LocalOnly | 
					  SendConfig), DatabaseLoaded(false)
{
};
									/*}}}*/
// CDROMMethod::GetID - Search the database for a matching string	/*{{{*/
// ---------------------------------------------------------------------
/* */
string CDROMMethod::GetID(string Name)
{
   // Search for an ID
   const Configuration::Item *Top = Database.Tree("CD");
   if (Top != 0)
      Top = Top->Child;
   
   for (; Top != 0;)
   {      
      if (Top->Value == Name)
	 return Top->Tag;
      
      Top = Top->Next;
   }
   return string();
}
									/*}}}*/
// CDROMMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CDROMMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string File = Get.Path;
   FetchResult Res;

   bool Debug = _config->FindB("Debug::Acquire::cdrom",false);

   /* All IMS queries are returned as a hit, CDROMs are readonly so 
      time stamps never change */
   if (Itm->LastModified != 0)
   {
      Res.LastModified = Itm->LastModified;
      Res.IMSHit = true;
      URIDone(Res);
      return true;
   }

   // Load the database
   if (DatabaseLoaded == false)
   {
      // Read the database
      string DFile = _config->FindFile("Dir::State::cdroms");
      if (FileExists(DFile) == true)
      {
	 if (ReadConfigFile(Database,DFile) == false)
	    return _error->Error("Unable to read the cdrom database %s",
			  DFile.c_str());
      }
      DatabaseLoaded = true;
   }
       
   // All non IMS queries for package files fail.
   if (Itm->IndexFile == true || GetID(Get.Host).empty() == true)
   {
      Fail("Please use apt-cdrom to make this CD recognized by APT."
	   " apt-get update cannot be used to add new CDs");
      return true;
   }

   // We already have a CD inserted, but it is the wrong one
   if (CurrentID.empty() == false && Database.Find("CD::" + CurrentID) != Get.Host)
   {
      Fail("Wrong CD",true);
      return true;
   }
   
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;
   string NewID;
   while (CurrentID.empty() == true)
   {
      bool Hit = false;
      for (unsigned int Version = 2; Version != 0; Version--)
      {
	 if (IdentCdrom(CDROM,NewID,Version) == false)
	    return false;
	 
	 if (Debug == true)
	    clog << "ID " << Version << " " << NewID << endl;
      
	 // A hit
	 if (Database.Find("CD::" + NewID) == Get.Host)
	 {
	    Hit = true;
	    break;
	 }	 
      }

      if (Hit == true)
	 break;
	 
      // I suppose this should prompt somehow?
      if (UnmountCdrom(CDROM) == false)
	 return _error->Error("Unable to unmount the CD-ROM in %s, it may still be in use.",
			      CDROM.c_str());
      if (MediaFail(Get.Host,CDROM) == false)
      {
	 CurrentID = "FAIL";
	 Fail("Wrong CD",true);
	 return true;
      }
      
      MountCdrom(CDROM);
   }
   
   // Found a CD
   Res.Filename = CDROM + File;
   struct stat Buf;
   if (stat(Res.Filename.c_str(),&Buf) != 0)
      return _error->Error("File not found");
   
   if (NewID.empty() == false)
      CurrentID = NewID;
   Res.LastModified = Buf.st_mtime;
   Res.IMSHit = true;
   Res.Size = Buf.st_size;
   URIDone(Res);
   return true;
}
									/*}}}*/

int main()
{
   CDROMMethod Mth;
   return Mth.Run();
}
