// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdrom.cc,v 1.1 1998/12/03 07:29:21 jgg Exp $
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
   string CurrentID;
   
   virtual bool Fetch(FetchItem *Itm);
   string GetID(string Name);
   
   public:
   
   CDROMMethod();
};

// CDROMMethod::CDROMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CDROMMethod::CDROMMethod() : pkgAcqMethod("1.0",SingleInstance | LocalOnly) 
{
   // Read the database
   string DFile = _config->FindFile("Dir::State::cdroms");
   if (FileExists(DFile) == true)
   {
      if (ReadConfigFile(Database,DFile) == false)
      {
	 _error->Error("Unable to read the cdrom database %s",
		       DFile.c_str());
	 Fail();
      }   
   }      
};
									/*}}}*/
// CDROMMethod::GetID - Get the ID hash for 									/*{{{*/
// ---------------------------------------------------------------------
/* We search the configuration space for the name and then return the ID
   tag associated with it. */
string CDROMMethod::GetID(string Name)
{
   const Configuration::Item *Top = Database.Tree(0);
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
   
   /* All IMS queries are returned as a hit, CDROMs are readonly so 
      time stamps never change */
   if (Itm->LastModified != 0)
   {
      Res.LastModified = Itm->LastModified;
      Res.IMSHit = true;
      URIDone(Res);
      return true;
   }
   
   string ID = GetID(Get.Host);
   
   // All non IMS queries for package files fail.
   if (Itm->IndexFile == true || ID.empty() == false)
   {
      Fail("Please use apt-cdrom to make this CD recognized by APT."
	   " apt-get update cannot be used to add new CDs");
      return true;
   }

   // We already have a CD inserted, but it is the wrong one
   if (CurrentID.empty() == false && ID != CurrentID)
   {
      Fail("Wrong CD",true);
      return true;
   }
   
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   string NewID;
   while (1)
   {
      if (IdentCdrom(CDROM,NewID) == false)
	 return false;
   
      // A hit
      if (NewID == ID)
	 break;
      
      UnmountCdrom(CDROM);
      MediaFail(Get.Host,CDROM);
      MountCdrom(CDROM);
   }
   
   // ID matches
   if (NewID == ID)
   {
      Res.Filename = CDROM + File;
      if (FileExists(Res.Filename) == false)
	 return _error->Error("File not found");
    
      CurrentID = ID;
      Res.LastModified = Itm->LastModified;
      Res.IMSHit = true;
      URIDone(Res);
      return true;
   }
   
   return _error->Error("CDROM not found");
}
									/*}}}*/

int main()
{
   CDROMMethod Mth;
   return Mth.Run();
}
