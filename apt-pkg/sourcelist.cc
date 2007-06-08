// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.cc,v 1.3 2002/08/15 20:51:37 niemeyer Exp $
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>

#include <apti18n.h>

#include <fstream>

// CNC:2003-03-03 - This is needed for ReadDir stuff.
#include <algorithm>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
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
bool pkgSourceList::Type::ParseLine(vector<metaIndex *> &List,
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
	 return _error->Error(_("Malformed line %lu in source list %s (absolute dist)"),CurLine,File.c_str());
      Dist = SubstVar(Dist,"$(ARCH)",_config->Find("APT::Architecture"));
      return CreateItem(List,URI,Dist,Section);
   }
   
   // Grab the rest of the dists
   if (ParseQuoteWord(Buffer,Section) == false)
      return _error->Error(_("Malformed line %lu in source list %s (dist parse)"),CurLine,File.c_str());
   
   do
   {
      if (CreateItem(List,URI,Dist,Section) == false)
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
}
									/*}}}*/
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   // CNC:2003-03-03 - Multiple sources list support.
   bool Res = true;
#if 0
   Res = ReadVendors();
   if (Res == false)
      return false;
#endif

   Reset();
   // CNC:2003-11-28 - Entries in sources.list have priority over
   //                  entries in sources.list.d.
   string Main = _config->FindFile("Dir::Etc::sourcelist");
   if (FileExists(Main) == true)
      Res &= ReadAppend(Main);   

   string Parts = _config->FindDir("Dir::Etc::sourceparts");
   if (FileExists(Parts) == true)
      Res &= ReadSourceDir(Parts);
   
   return Res;
}
									/*}}}*/
// CNC:2003-03-03 - Needed to preserve backwards compatibility.
// SourceList::Reset - Clear the sourcelist contents			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgSourceList::Reset()
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      delete *I;
   SrcList.erase(SrcList.begin(),SrcList.end());
}
									/*}}}*/
// CNC:2003-03-03 - Function moved to ReadAppend() and Reset().
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string File)
{
   Reset();
   return ReadAppend(File);
}
									/*}}}*/
// SourceList::ReadAppend - Parse a sourcelist file			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadAppend(string File)
{
   // Open the stream for reading
   ifstream F(File.c_str(),ios::in /*| ios::nocreate*/);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream",_("Opening %s"),File.c_str());
   
#if 0 // Now Reset() does this.
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      delete *I;
   SrcList.erase(SrcList.begin(),SrcList.end());
#endif
   // CNC:2003-12-10 - 300 is too short.
   char Buffer[1024];

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
      // CNC:2003-02-20 - Do not break if '#' is inside [].
      for (I = Buffer; *I != 0 && *I != '#'; I++)
         if (*I == '[')
	    for (I++; *I != 0 && *I != ']'; I++);
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
      
      // Vendor name specified
      if (C[0] == '[')
      {
	 string VendorID;
	 
	 if (ParseQuoteWord(C,VendorID) == false)
	     return _error->Error(_("Malformed line %u in source list %s (vendor id)"),CurLine,File.c_str());

	 if (VendorID.length() < 2 || VendorID.end()[-1] != ']')
	     return _error->Error(_("Malformed line %u in source list %s (vendor id)"),CurLine,File.c_str());
	 VendorID = string(VendorID,1,VendorID.size()-2);
	 
// 	 for (vector<const Vendor *>::const_iterator iter = VendorList.begin();
// 	      iter != VendorList.end(); iter++) 
// 	 {
// 	    if ((*iter)->GetVendorID() == VendorID)
// 	    {
// 	      if (_config->FindB("Debug::sourceList", false)) 
// 		std::cerr << "Comparing VendorID \"" << VendorID << "\" with \"" << (*iter)->GetVendorID() << '"' << std::endl;
// 	       Verifier = *iter;
// 	       break;
// 	    }
// 	 }

// 	 if (Verifier == 0)
// 	    return _error->Error(_("Unknown vendor ID '%s' in line %u of source list %s"),
// 				 VendorID.c_str(),CurLine,File.c_str());
      }

      if (Parse->ParseLine(SrcList,C,CurLine,File) == false)
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
      vector<pkgIndexFile *> *Indexes = (*I)->GetIndexFiles();
      for (vector<pkgIndexFile *>::const_iterator J = Indexes->begin();
	   J != Indexes->end(); J++)
      {
         if ((*J)->FindInCache(*File.Cache()) == File)
         {
            Found = (*J);
            return true;
         }
      }
   }

   return false;
}
									/*}}}*/
// SourceList::GetIndexes - Load the index files into the downloader	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::GetIndexes(pkgAcquire *Owner, bool GetAll) const
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); I++)
      if ((*I)->GetIndexes(Owner,GetAll) == false)
	 return false;
   return true;
}
									/*}}}*/
// CNC:2003-03-03 - By Anton V. Denisov <avd@altlinux.org>.
// SourceList::ReadSourceDir - Read a directory with sources files
// Based on ReadConfigDir()						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadSourceDir(string Dir)
{
   DIR *D = opendir(Dir.c_str());
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());

   vector<string> List;
   
   for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D))
   {
      if (Ent->d_name[0] == '.')
	 continue;

      // CNC:2003-12-02 Only accept .list files as valid sourceparts
      if (flExtension(Ent->d_name) != "list")
	 continue;
      
      // Skip bad file names ala run-parts
      const char *C = Ent->d_name;
      for (; *C != 0; C++)
	 if (isalpha(*C) == 0 && isdigit(*C) == 0
             && *C != '_' && *C != '-' && *C != '.')
	    break;
      if (*C != 0)
	 continue;
      
      // Make sure it is a file and not something else
      string File = flCombine(Dir,Ent->d_name);
      struct stat St;
      if (stat(File.c_str(),&St) != 0 || S_ISREG(St.st_mode) == 0)
	 continue;
      
      List.push_back(File);      
   }   
   closedir(D);
   
   sort(List.begin(),List.end());

   // Read the files
   for (vector<string>::const_iterator I = List.begin(); I != List.end(); I++)
      if (ReadAppend(*I) == false)
	 return false;
   return true;

}
									/*}}}*/

