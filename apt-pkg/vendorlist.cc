#include <apt-pkg/vendorlist.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apti18n.h>

pkgVendorList::~pkgVendorList()
{
   for (vector<const Vendor *>::const_iterator I = VendorList.begin(); 
        I != VendorList.end(); I++)
      delete *I;
}

// pkgVendorList::ReadMainList - Read list of known package vendors		/*{{{*/
// ---------------------------------------------------------------------
/* This also scans a directory of vendor files similar to apt.conf.d 
   which can contain the usual suspects of distribution provided data.
   The APT config mechanism allows the user to override these in their
   configuration file. */
bool pkgVendorList::ReadMainList()
{
   Configuration Cnf;

   string CnfFile = _config->FindDir("Dir::Etc::vendorparts");
   if (FileExists(CnfFile) == true)
      if (ReadConfigDir(Cnf,CnfFile,true) == false)
	 return false;
   CnfFile = _config->FindFile("Dir::Etc::vendorlist");
   if (FileExists(CnfFile) == true)
      if (ReadConfigFile(Cnf,CnfFile,true) == false)
	 return false;

   return CreateList(Cnf);
}

bool pkgVendorList::Read(string File)
{
   Configuration Cnf;
   if (ReadConfigFile(Cnf,File,true) == false)
      return false;

   return CreateList(Cnf);
}

bool pkgVendorList::CreateList(Configuration& Cnf)
{
   for (vector<const Vendor *>::const_iterator I = VendorList.begin(); 
	I != VendorList.end(); I++)
      delete *I;
   VendorList.erase(VendorList.begin(),VendorList.end());

   const Configuration::Item *Top = Cnf.Tree("Vendor");
   for (Top = (Top == 0?0:Top->Child); Top != 0; Top = Top->Next)
   {
      Configuration Block(Top);
      string VendorID = Top->Tag;
      vector <struct Vendor::Fingerprint *> *Fingerprints = new vector<Vendor::Fingerprint *>;
      struct Vendor::Fingerprint *Fingerprint = new struct Vendor::Fingerprint;
      string Origin = Block.Find("Origin");

      Fingerprint->Print = Block.Find("Fingerprint");
      Fingerprint->Description = Block.Find("Name");
      Fingerprints->push_back(Fingerprint);

      if (Fingerprint->Print.empty() || Fingerprint->Description.empty())
      {
         _error->Error(_("Vendor block %s contains no fingerprint"), VendorID.c_str());
         delete Fingerprints;
         continue;
      }
      if (_config->FindB("Debug::sourceList", false)) 
         std::cerr << "Adding vendor with ID: " << VendorID
		   << " Fingerprint: " << Fingerprint->Print << std::endl;

      VendorList.push_back(new Vendor(VendorID, Origin, Fingerprints));
   }

   /* Process 'group-key' type sections */
   Top = Cnf.Tree("group-key");
   for (Top = (Top == 0?0:Top->Child); Top != 0; Top = Top->Next)
   {
//       Configuration Block(Top);
//       vector<Vendor::Fingerprint *> Fingerprints;
//       string VendorID = Top->Tag;

//       while (Block->Next)
//       {
// 	 struct Vendor::Fingerprint Fingerprint = new struct Vendor::Fingerprint;
// 	 Fingerprint->Print = Block.Find("Fingerprint");
// 	 Fingerprint->Description = Block.Find("Name");
// 	 if (Fingerprint->print.empty() || Fingerprint->Description.empty())
// 	 {
// 	    _error->Error(_("Vendor block %s is invalid"), 
// 			  Vendor->VendorID.c_str());
// 	    delete Fingerprint;
// 	    break;
// 	 }
// 	 Block = Block->Next->Next;
//       }
//       if (_error->PendingError())
//       {
// 	 for (vector <struct Vendor::Fingerprint *>::iterator I = Fingerprints.begin();
// 	      I != Fingerprints.end(); I++)
// 	    delete *I;
// 	 delete Fingerprints;
// 	 continue;
//       }

//       VendorList.push_back(new Vendor(VendorID, Fingerprints));
   }
   
   return !_error->PendingError();
}

const Vendor* pkgVendorList::LookupFingerprint(string Fingerprint)
{
   for (const_iterator I = VendorList.begin(); I != VendorList.end(); ++I)
   {
      if ((*I)->LookupFingerprint(Fingerprint) != "")
         return *I;
   }

   return NULL;
}

const Vendor* pkgVendorList::FindVendor(const std::vector<string> GPGVOutput)
{
   for (std::vector<string>::const_iterator I = GPGVOutput.begin(); I != GPGVOutput.end(); I++)
   {
      string::size_type pos = (*I).find("VALIDSIG ");
      if (_config->FindB("Debug::Vendor", false))
         std::cerr << "Looking for VALIDSIG in \"" << (*I) << "\": pos " << pos << std::endl;
      if (pos != std::string::npos)
      {
         string Fingerprint = (*I).substr(pos+sizeof("VALIDSIG"));
         if (_config->FindB("Debug::Vendor", false))
            std::cerr << "Looking for \"" << Fingerprint << "\" in vendor..." << std::endl;
         const Vendor* vendor = this->LookupFingerprint(Fingerprint);
         if (vendor != NULL)
            return vendor;
      }
   }

   return NULL;
}
