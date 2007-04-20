// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdrom.cc,v 1.20.2.1 2004/01/16 18:58:50 mdz Exp $
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
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <apti18n.h>
									/*}}}*/

using namespace std;

class CDROMMethod : public pkgAcqMethod
{
   bool DatabaseLoaded;
   ::Configuration Database;
   string CurrentID;
   string CDROM;
   bool MountedByApt;
   
   virtual bool Fetch(FetchItem *Itm);
   string GetID(string Name);
   virtual void Exit();
   
   public:
   
   CDROMMethod();
};

// CDROMMethod::CDROMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CDROMMethod::CDROMMethod() : pkgAcqMethod("1.0",SingleInstance | LocalOnly |
					  SendConfig | NeedsCleanup |
					  Removable), 
                                          DatabaseLoaded(false), 
                                          MountedByApt(false)
{
};
									/*}}}*/
// CDROMMethod::Exit - Unmount the disc if necessary			/*{{{*/
// ---------------------------------------------------------------------
/* */
void CDROMMethod::Exit()
{
   if (MountedByApt == true)
      UnmountCdrom(CDROM);
}
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
      Res.Filename = Itm->DestFile;
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
	    return _error->Error(_("Unable to read the cdrom database %s"),
			  DFile.c_str());
      }
      DatabaseLoaded = true;
   }
       
   // All non IMS queries for package files fail.
   if (Itm->IndexFile == true || GetID(Get.Host).empty() == true)
   {
      Fail(_("Please use apt-cdrom to make this CD-ROM recognized by APT."
	   " apt-get update cannot be used to add new CD-ROMs"));
      return true;
   }

   // We already have a CD inserted, but it is the wrong one
   if (CurrentID.empty() == false && Database.Find("CD::" + CurrentID) != Get.Host)
   {
      Fail(_("Wrong CD-ROM"),true);
      return true;
   }
   
   CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;
   string NewID;
   while (CurrentID.empty() == true)
   {
      bool Hit = false;
      if(!IsMounted(CDROM))
	 MountedByApt = MountCdrom(CDROM);
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
      if (_config->FindB("APT::CDROM::NoMount",false) == false &&
	  UnmountCdrom(CDROM) == false)
	 return _error->Error(_("Unable to unmount the CD-ROM in %s, it may still be in use."),
			      CDROM.c_str());
      if (MediaFail(Get.Host,CDROM) == false)
      {
	 CurrentID = "FAIL";
	 return _error->Error(_("Disk not found."));
      }
   }
   
   // Found a CD
   Res.Filename = CDROM + File;
   struct stat Buf;
   if (stat(Res.Filename.c_str(),&Buf) != 0)
      return _error->Error(_("File not found"));
   
   if (NewID.empty() == false)
      CurrentID = NewID;
   Res.LastModified = Buf.st_mtime;
   Res.Size = Buf.st_size;

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

   CDROMMethod Mth;
   return Mth.Run();
}
