// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.cc,v 1.25 2004/06/07 23:08:00 mdz Exp $
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/sourcelist.h"
#endif

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>

#include <fstream>
									/*}}}*/

using namespace std;

// Global list of Items supported
static  pkgSourceList::Type *ItmList[10];
pkgSourceList::Type **pkgSourceList::Type::GlobalList = ItmList;
unsigned long pkgSourceList::Type::GlobalListLen = 0;

// Type::Type - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* Link this to the global list of items*/
pkgSourceList::Type::Type()
{
   ItmList[GlobalListLen] = this;
   GlobalListLen++;
}
									/*}}}*/
// Type::GetType - Get a specific meta for a given type			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::Type *pkgSourceList::Type::GetType(const char *Type)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(GlobalList[I]->Name,Type) == 0)
	 return GlobalList[I];
   return 0;
}
									/*}}}*/
// Type::FixupURI - Normalize the URI and check it..			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Type::FixupURI(string &URI) const
{
   if (URI.empty() == true)
      return false;

   if (URI.find(':') == string::npos)
      return false;

   URI = SubstVar(URI,"$(ARCH)",_config->Find("APT::Architecture"));
   
   // Make sure that the URI is / postfixed
   if (URI[URI.size() - 1] != '/')
      URI += '/';
   
   return true;
}
									/*}}}*/
// Type::ParseLine - Parse a single line				/*{{{*/
// ---------------------------------------------------------------------
/* This is a generic one that is the 'usual' format for sources.list
   Weird types may override this. */
bool pkgSourceList::Type::ParseLine(vector<pkgIndexFile *> &List,
				    Vendor const *Vendor,
				    const char *Buffer,
				    unsigned long CurLine,
				    string File) const
{
   string URI;
   string Dist;
   string Section;   
   
   if (ParseQuoteWord(Buffer,URI) == false)
      return _error->Error(_("Malformed line %lu in source list %s (URI)"),CurLine,File.c_str());
   if (ParseQuoteWord(Buffer,Dist) == false)
      return _error->Error(_("Malformed line %lu in source list %s (dist)"),CurLine,File.c_str());
      
   if (FixupURI(URI) == false)
      return _error->Error(_("Malformed line %lu in source list %s (URI parse)"),CurLine,File.c_str());
   
   // Check for an absolute dists specification.
   if (Dist.empty() == false && Dist[Dist.size() - 1] == '/')
   {
      if (ParseQuoteWord(Buffer,Section) == true)
	 return _error->Error(_("Malformed line %lu in source list %s (Absolute dist)"),CurLine,File.c_str());
      Dist = SubstVar(Dist,"$(ARCH)",_config->Find("APT::Architecture"));
      return CreateItem(List,URI,Dist,Section,Vendor);
   }
   
   // Grab the rest of the dists
   if (ParseQuoteWord(Buffer,Section) == false)
      return _error->Error(_("Malformed line %lu in source list %s (dist parse)"),CurLine,File.c_str());
   
   do
   {
      if (CreateItem(List,URI,Dist,Section,Vendor) == false)
	 return false;
   }
   while (ParseQuoteWord(Buffer,Section) == true);
   
   return true;
}
									/*}}}*/

// SourceList::pkgSourceList - Constructors				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::pkgSourceList()
{
}

pkgSourceList::pkgSourceList(string File)
{
   Read(File);
}
									/*}}}*/
// SourceList::~pkgSourceList - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::~pkgSourceList()
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      delete *I;
   for (vector<Vendor const *>::const_iterator I = VendorList.begin(); 
	I != VendorList.end(); I++)
      delete *I;
}
									/*}}}*/
// SourceList::ReadVendors - Read list of known package vendors		/*{{{*/
// ---------------------------------------------------------------------
/* This also scans a directory of vendor files similar to apt.conf.d 
   which can contain the usual suspects of distribution provided data.
   The APT config mechanism allows the user to override these in their
   configuration file. */
bool pkgSourceList::ReadVendors()
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

   for (vector<Vendor const *>::const_iterator I = VendorList.begin(); 
	I != VendorList.end(); I++)
      delete *I;
   VendorList.erase(VendorList.begin(),VendorList.end());
   
   // Process 'simple-key' type sections
   const Configuration::Item *Top = Cnf.Tree("simple-key");
   for (Top = (Top == 0?0:Top->Child); Top != 0; Top = Top->Next)
   {
      Configuration Block(Top);
      Vendor *Vendor;
      
      Vendor = new pkgSourceList::Vendor;
      
      Vendor->VendorID = Top->Tag;
      Vendor->FingerPrint = Block.Find("Fingerprint");
      Vendor->Description = Block.Find("Name");
      
      if (Vendor->FingerPrint.empty() == true || 
	  Vendor->Description.empty() == true)
      {
         _error->Error(_("Vendor block %s is invalid"), Vendor->VendorID.c_str());
	 delete Vendor;
	 continue;
      }
      
      VendorList.push_back(Vendor);
   }

   /* XXX Process 'group-key' type sections
      This is currently faked out so that the vendors file format is
      parsed but nothing is done with it except check for validity */
   Top = Cnf.Tree("group-key");
   for (Top = (Top == 0?0:Top->Child); Top != 0; Top = Top->Next)
   {
      Configuration Block(Top);
      Vendor *Vendor;
      
      Vendor = new pkgSourceList::Vendor;
      
      Vendor->VendorID = Top->Tag;
      Vendor->Description = Block.Find("Name");

      if (Vendor->Description.empty() == true)
      {
         _error->Error(_("Vendor block %s is invalid"), 
		       Vendor->VendorID.c_str());
	 delete Vendor;
	 continue;
      }
      
      VendorList.push_back(Vendor);
   }
   
   return !_error->PendingError();
}
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   return ReadVendors() && Read(_config->FindFile("Dir::Etc::sourcelist"));
}
									/*}}}*/
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string File)
{
   // Open the stream for reading
   ifstream F(File.c_str(),ios::in /*| ios::nocreate*/);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream",_("Opening %s"),File.c_str());
   
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      delete *I;
   SrcList.erase(SrcList.begin(),SrcList.end());
   char Buffer[300];

   int CurLine = 0;
   while (F.eof() == false)
   {
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      _strtabexpand(Buffer,sizeof(Buffer));
      if (F.fail() && !F.eof())
	 return _error->Error(_("Line %u too long in source list %s."),
			      CurLine,File.c_str());

      
      char *I;
      for (I = Buffer; *I != 0 && *I != '#'; I++);
      *I = 0;
      
      const char *C = _strstrip(Buffer);
      
      // Comment or blank
      if (C[0] == '#' || C[0] == 0)
	 continue;
      	    
      // Grok it
      string LineType;
      if (ParseQuoteWord(C,LineType) == false)
	 return _error->Error(_("Malformed line %u in source list %s (type)"),CurLine,File.c_str());

      Type *Parse = Type::GetType(LineType.c_str());
      if (Parse == 0)
	 return _error->Error(_("Type '%s' is not known on line %u in source list %s"),LineType.c_str(),CurLine,File.c_str());
      
      // Authenticated repository
      Vendor const *Vndr = 0;
      if (C[0] == '[')
      {
	 string VendorID;
	 
	 if (ParseQuoteWord(C,VendorID) == false)
	     return _error->Error(_("Malformed line %u in source list %s (vendor id)"),CurLine,File.c_str());

	 if (VendorID.length() < 2 || VendorID.end()[-1] != ']')
	     return _error->Error(_("Malformed line %u in source list %s (vendor id)"),CurLine,File.c_str());
	 VendorID = string(VendorID,1,VendorID.size()-2);
	 
	 for (vector<Vendor const *>::const_iterator iter = VendorList.begin();
	      iter != VendorList.end(); iter++) 
	 {
	    if ((*iter)->VendorID == VendorID)
	    {
	       Vndr = *iter;
	       break;
	    }
	 }

	 if (Vndr == 0)
	    return _error->Error(_("Unknown vendor ID '%s' in line %u of source list %s"),
				 VendorID.c_str(),CurLine,File.c_str());
      }
      
      if (Parse->ParseLine(SrcList,Vndr,C,CurLine,File) == false)
	 return false;
   }
   return true;
}
									/*}}}*/
// SourceList::FindIndex - Get the index associated with a file		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::FindIndex(pkgCache::PkgFileIterator File,
			      pkgIndexFile *&Found) const
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
   {
      if ((*I)->FindInCache(*File.Cache()) == File)
      {
	 Found = *I;
	 return true;
      }
   }
   
   return false;
}
									/*}}}*/
// SourceList::GetIndexes - Load the index files into the downloader	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::GetIndexes(pkgAcquire *Owner) const
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      if ((*I)->GetIndexes(Owner) == false)
	 return false;
   return true;
}
									/*}}}*/
