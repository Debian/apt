// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   CDROM URI method for APT
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cdrom.h>
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include "aptmethod.h"

#include <string>
#include <vector>
#include <sys/stat.h>

#include <iostream>
#include <apti18n.h>
									/*}}}*/

using namespace std;

class CDROMMethod : public aptMethod
{
   bool DatabaseLoaded;
   bool Debug;

   ::Configuration Database;
   string CurrentID;
   string CDROM;
   bool MountedByApt;
   pkgUdevCdromDevices UdevCdroms;
 
   bool IsCorrectCD(URI want, string MountPath, string& NewID);
   bool AutoDetectAndMount(const URI, string &NewID);
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;
   std::string GetID(std::string const &Name);
   virtual void Exit() APT_OVERRIDE;
   virtual bool Configuration(std::string Message) APT_OVERRIDE;

   public:
   
   CDROMMethod();
};

// CDROMMethod::CDROMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CDROMMethod::CDROMMethod() : aptMethod("cdrom", "1.0",SingleInstance | LocalOnly |
					  SendConfig | NeedsCleanup |
					  Removable | SendURIEncoded),
                                          DatabaseLoaded(false),
					  Debug(false),
                                          MountedByApt(false)
{
}
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
std::string CDROMMethod::GetID(std::string const &Name)
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
// CDROMMethod::AutoDetectAndMount                                      /*{{{*/
// ---------------------------------------------------------------------
/* Modifies class variable CDROM to the mountpoint */
bool CDROMMethod::AutoDetectAndMount(const URI Get, string &NewID)
{
   vector<struct CdromDevice> v = UdevCdroms.Scan();

   // first check if its mounted somewhere already
   for (unsigned int i=0; i < v.size(); i++)
   {
      if (v[i].Mounted)
      {
	 if (Debug)
	    clog << "Checking mounted cdrom device " << v[i].DeviceName << endl;
	 if (IsCorrectCD(Get, v[i].MountPath, NewID))
	 {
	    CDROM = v[i].MountPath;
	    return true;
	 }
      }
   }

   // we are not supposed to mount, exit
   if (_config->FindB("APT::CDROM::NoMount",false) == true)
      return false;

   // check if we have the mount point
   string AptMountPoint = _config->FindDir("Dir::Media::MountPath");
   if (!FileExists(AptMountPoint))
      mkdir(AptMountPoint.c_str(), 0750);

   // now try mounting
   for (unsigned int i=0; i < v.size(); i++)
   {
      if (!v[i].Mounted)
      {
	 if(MountCdrom(AptMountPoint, v[i].DeviceName)) 
	 {
	    if (IsCorrectCD(Get, AptMountPoint, NewID))
	    {
	       MountedByApt = true;
	       CDROM = AptMountPoint;
	       return true;
	    } else {
	       UnmountCdrom(AptMountPoint);
	    }
	 }
      }
   }

   return false;
}
									/*}}}*/
// CDROMMethod::IsCorrectCD                                             /*{{{*/
// ---------------------------------------------------------------------
/* */
bool CDROMMethod::IsCorrectCD(URI want, string MountPath, string& NewID)
{
   for (unsigned int Version = 2; Version != 0; Version--)
   {
      if (IdentCdrom(MountPath,NewID,Version) == false)
	 return false;
      
      if (Debug)
	 clog << "ID " << Version << " " << NewID << endl;
      
      // A hit
      if (Database.Find("CD::" + NewID) == want.Host)
	 return true;
   }
   
   return false;
}
									/*}}}*/
// CDROMMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CDROMMethod::Fetch(FetchItem *Itm)
{
   FetchResult Res;

   URI Get(Itm->Uri);
   std::string const File = DecodeSendURI(Get.Path);
   Debug = DebugEnabled();

   if (Debug)
      clog << "CDROMMethod::Fetch " << Itm->Uri << endl;

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
   if (CurrentID.empty() == false && 
       CurrentID != "FAIL" &&
       Database.Find("CD::" + CurrentID) != Get.Host)
   {
      Fail(_("Wrong CD-ROM"),true);
      return true;
   }

   bool const AutoDetect = ConfigFindB("AutoDetect", true);
   CDROM = _config->FindDir("Acquire::cdrom::mount");
   if (Debug)
      clog << "Looking for CDROM at " << CDROM << endl;

   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;

   string NewID;
   while (CurrentID.empty() == true)
   {
      if (AutoDetect)
	 AutoDetectAndMount(Get, NewID);

      if(!IsMounted(CDROM))
	 MountedByApt = MountCdrom(CDROM);
      
      if (IsCorrectCD(Get, CDROM, NewID))
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

   URIStart(Res);
   if (NewID.empty() == false)
      CurrentID = NewID;
   Res.LastModified = Buf.st_mtime;
   Res.Size = Buf.st_size;

   Hashes Hash(Itm->ExpectedHashes);
   FileFd Fd(Res.Filename, FileFd::ReadOnly);
   Hash.AddFD(Fd);
   Res.TakeHashes(Hash);

   URIDone(Res);
   return true;
}
									/*}}}*/
bool CDROMMethod::Configuration(std::string Message)			/*{{{*/
{
   _config->CndSet("Binary::cdrom::Debug::NoDropPrivs", true);
   return aptMethod::Configuration(Message);
}
									/*}}}*/

int main()
{
   return CDROMMethod().Run();
}
