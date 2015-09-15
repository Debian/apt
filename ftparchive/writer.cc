// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: writer.cc,v 1.14 2004/03/24 01:40:43 mdz Exp $
/* ######################################################################

   Writer 
   
   The file writer classes. These write various types of output, sources,
   packages and contents.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/debfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>
#include <apt-pkg/tagfile.h>

#include <ctype.h>
#include <fnmatch.h>
#include <ftw.h>
#include <locale.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <memory>
#include <utility>
#include <algorithm>

#include "apt-ftparchive.h"
#include "writer.h"
#include "cachedb.h"
#include "multicompress.h"
#include "byhash.h"

#include <apti18n.h>
									/*}}}*/
using namespace std;
FTWScanner *FTWScanner::Owner;

// ConfigToDoHashes - which hashes to generate				/*{{{*/
static void SingleConfigToDoHashes(unsigned int &DoHashes, std::string const &Conf, unsigned int const Flag)
{
   if (_config->FindB(Conf, true) == true)
      DoHashes |= Flag;
   else
      DoHashes &= ~Flag;
}
static void ConfigToDoHashes(unsigned int &DoHashes, std::string const &Conf)
{
   SingleConfigToDoHashes(DoHashes, Conf + "::MD5", Hashes::MD5SUM);
   SingleConfigToDoHashes(DoHashes, Conf + "::SHA1", Hashes::SHA1SUM);
   SingleConfigToDoHashes(DoHashes, Conf + "::SHA256", Hashes::SHA256SUM);
   SingleConfigToDoHashes(DoHashes, Conf + "::SHA512", Hashes::SHA512SUM);
}
									/*}}}*/

// FTWScanner::FTWScanner - Constructor					/*{{{*/
FTWScanner::FTWScanner(FileFd * const GivenOutput, string const &Arch): Arch(Arch), DoHashes(~0)
{
   if (GivenOutput == NULL)
   {
      Output = new FileFd;
      OwnsOutput = true;
      Output->OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly, false);
   }
   else
   {
      Output = GivenOutput;
      OwnsOutput = false;
   }
   ErrorPrinted = false;
   NoLinkAct = !_config->FindB("APT::FTPArchive::DeLinkAct",true);
   ConfigToDoHashes(DoHashes, "APT::FTPArchive");
}
									/*}}}*/
FTWScanner::~FTWScanner()
{
   if (Output != NULL && OwnsOutput)
      delete Output;
}
// FTWScanner::Scanner - FTW Scanner					/*{{{*/
// ---------------------------------------------------------------------
/* This is the FTW scanner, it processes each directory element in the
   directory tree. */
int FTWScanner::ScannerFTW(const char *File,const struct stat * /*sb*/,int Flag)
{
   if (Flag == FTW_DNR)
   {
      Owner->NewLine(1);
      ioprintf(c1out, _("W: Unable to read directory %s\n"), File);
   }   
   if (Flag == FTW_NS)
   {
      Owner->NewLine(1);
      ioprintf(c1out, _("W: Unable to stat %s\n"), File);
   }   
   if (Flag != FTW_F)
      return 0;

   return ScannerFile(File, true);
}
									/*}}}*/
// FTWScanner::ScannerFile - File Scanner				/*{{{*/
// ---------------------------------------------------------------------
/* */
int FTWScanner::ScannerFile(const char *File, bool const &ReadLink)
{
   const char *LastComponent = strrchr(File, '/');
   char *RealPath = NULL;

   if (LastComponent == NULL)
      LastComponent = File;
   else
      LastComponent++;

   vector<string>::const_iterator I;
   for(I = Owner->Patterns.begin(); I != Owner->Patterns.end(); ++I)
   {
      if (fnmatch((*I).c_str(), LastComponent, 0) == 0)
         break;
   }
   if (I == Owner->Patterns.end())
      return 0;

   /* Process it. If the file is a link then resolve it into an absolute
      name.. This works best if the directory components the scanner are
      given are not links themselves. */
   char Jnk[2];
   Owner->OriginalPath = File;
   if (ReadLink &&
       readlink(File,Jnk,sizeof(Jnk)) != -1 &&
       (RealPath = realpath(File,NULL)) != 0)
   {
      Owner->DoPackage(RealPath);
      free(RealPath);
   }
   else
      Owner->DoPackage(File);
   
   if (_error->empty() == false)
   {
      // Print any errors or warnings found
      string Err;
      bool SeenPath = false;
      while (_error->empty() == false)
      {
	 Owner->NewLine(1);
	 
	 bool const Type = _error->PopMessage(Err);
	 if (Type == true)
	    cerr << _("E: ") << Err << endl;
	 else
	    cerr << _("W: ") << Err << endl;
	 
	 if (Err.find(File) != string::npos)
	    SeenPath = true;
      }      
      
      if (SeenPath == false)
	 cerr << _("E: Errors apply to file ") << "'" << File << "'" << endl;
      return 0;
   }
   
   return 0;
}
									/*}}}*/
// FTWScanner::RecursiveScan - Just scan a directory tree		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FTWScanner::RecursiveScan(string const &Dir)
{
   char *RealPath = NULL;
   /* If noprefix is set then jam the scan root in, so we don't generate
      link followed paths out of control */
   if (InternalPrefix.empty() == true)
   {
      if ((RealPath = realpath(Dir.c_str(),NULL)) == 0)
	 return _error->Errno("realpath",_("Failed to resolve %s"),Dir.c_str());
      InternalPrefix = RealPath;
      free(RealPath);
   }
   
   // Do recursive directory searching
   Owner = this;
   int const Res = ftw(Dir.c_str(),ScannerFTW,30);
   
   // Error treewalking?
   if (Res != 0)
   {
      if (_error->PendingError() == false)
	 _error->Errno("ftw",_("Tree walking failed"));
      return false;
   }
   
   return true;
}
									/*}}}*/
// FTWScanner::LoadFileList - Load the file list from a file		/*{{{*/
// ---------------------------------------------------------------------
/* This is an alternative to using FTW to locate files, it reads the list
   of files from another file. */
bool FTWScanner::LoadFileList(string const &Dir, string const &File)
{
   char *RealPath = NULL;
   /* If noprefix is set then jam the scan root in, so we don't generate
      link followed paths out of control */
   if (InternalPrefix.empty() == true)
   {
      if ((RealPath = realpath(Dir.c_str(),NULL)) == 0)
	 return _error->Errno("realpath",_("Failed to resolve %s"),Dir.c_str());
      InternalPrefix = RealPath;      
      free(RealPath);
   }
   
   Owner = this;
   FILE *List = fopen(File.c_str(),"r");
   if (List == 0)
      return _error->Errno("fopen",_("Failed to open %s"),File.c_str());
   
   /* We are a tad tricky here.. We prefix the buffer with the directory
      name, that way if we need a full path with just use line.. Sneaky and
      fully evil. */
   char Line[1000];
   char *FileStart;
   if (Dir.empty() == true || Dir.end()[-1] != '/')
      FileStart = Line + snprintf(Line,sizeof(Line),"%s/",Dir.c_str());
   else
      FileStart = Line + snprintf(Line,sizeof(Line),"%s",Dir.c_str());   
   while (fgets(FileStart,sizeof(Line) - (FileStart - Line),List) != 0)
   {
      char *FileName = _strstrip(FileStart);
      if (FileName[0] == 0)
	 continue;
	 
      if (FileName[0] != '/')
      {
	 if (FileName != FileStart)
	    memmove(FileStart,FileName,strlen(FileStart));
	 FileName = Line;
      }
      
#if 0
      struct stat St;
      int Flag = FTW_F;
      if (stat(FileName,&St) != 0)
	 Flag = FTW_NS;
#endif

      if (ScannerFile(FileName, false) != 0)
	 break;
   }
  
   fclose(List);
   return true;
}
									/*}}}*/
// FTWScanner::Delink - Delink symlinks					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FTWScanner::Delink(string &FileName,const char *OriginalPath,
			unsigned long long &DeLinkBytes,
			unsigned long long const &FileSize)
{
   // See if this isn't an internaly prefix'd file name.
   if (InternalPrefix.empty() == false &&
       InternalPrefix.length() < FileName.length() && 
       stringcmp(FileName.begin(),FileName.begin() + InternalPrefix.length(),
		 InternalPrefix.begin(),InternalPrefix.end()) != 0)
   {
      if (DeLinkLimit != 0 && DeLinkBytes/1024 < DeLinkLimit)
      {
	 // Tidy up the display
	 if (DeLinkBytes == 0)
	    cout << endl;
	 
	 NewLine(1);
	 ioprintf(c1out, _(" DeLink %s [%s]\n"), (OriginalPath + InternalPrefix.length()),
		    SizeToStr(FileSize).c_str());
	 c1out << flush;
	 
	 if (NoLinkAct == false)
	 {
	    char OldLink[400];
	    if (readlink(OriginalPath,OldLink,sizeof(OldLink)) == -1)
	       _error->Errno("readlink",_("Failed to readlink %s"),OriginalPath);
	    else
	    {
	       if (unlink(OriginalPath) != 0)
		  _error->Errno("unlink",_("Failed to unlink %s"),OriginalPath);
	       else
	       {
		  if (link(FileName.c_str(),OriginalPath) != 0)
		  {
		     // Panic! Restore the symlink
		     if (symlink(OldLink,OriginalPath) != 0)
                        _error->Errno("symlink", "failed to restore symlink");
		     return _error->Errno("link",_("*** Failed to link %s to %s"),
					  FileName.c_str(),
					  OriginalPath);
		  }	       
	       }
	    }	    
	 }
	 
	 DeLinkBytes += FileSize;
	 if (DeLinkBytes/1024 >= DeLinkLimit)
	    ioprintf(c1out, _(" DeLink limit of %sB hit.\n"), SizeToStr(DeLinkBytes).c_str());      
      }
      
      FileName = OriginalPath;
   }
   
   return true;
}
									/*}}}*/

// PackagesWriter::PackagesWriter - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
PackagesWriter::PackagesWriter(FileFd * const GivenOutput, TranslationWriter * const transWriter,
      string const &DB,string const &Overrides,string const &ExtOverrides,
      string const &Arch) :
   FTWScanner(GivenOutput, Arch), Db(DB), Stats(Db.Stats), TransWriter(transWriter)
{
   SetExts(".deb .udeb");
   DeLinkLimit = 0;

   // Process the command line options
   ConfigToDoHashes(DoHashes, "APT::FTPArchive::Packages");
   DoAlwaysStat = _config->FindB("APT::FTPArchive::AlwaysStat", false);
   DoContents = _config->FindB("APT::FTPArchive::Contents",true);
   NoOverride = _config->FindB("APT::FTPArchive::NoOverrideMsg",false);
   LongDescription = _config->FindB("APT::FTPArchive::LongDescription",true);

   if (Db.Loaded() == false)
      DoContents = false;

   // Read the override file
   if (Overrides.empty() == false && Over.ReadOverride(Overrides) == false)
      return;
   else
      NoOverride = true;

   if (ExtOverrides.empty() == false)
      Over.ReadExtraOverride(ExtOverrides);

   _error->DumpErrors();
}
                                                                        /*}}}*/
// FTWScanner::SetExts - Set extensions to support                      /*{{{*/
// ---------------------------------------------------------------------
/* */
bool FTWScanner::SetExts(string const &Vals)
{
   ClearPatterns();
   string::size_type Start = 0;
   while (Start <= Vals.length()-1)
   {
      string::size_type const Space = Vals.find(' ',Start);
      string::size_type const Length = ((Space == string::npos) ? Vals.length() : Space) - Start;
      if ( Arch.empty() == false )
      {
	 AddPattern(string("*_") + Arch + Vals.substr(Start, Length));
	 AddPattern(string("*_all") + Vals.substr(Start, Length));
      }
      else
	 AddPattern(string("*") + Vals.substr(Start, Length));

      Start += Length + 1;
   }

   return true;
}
									/*}}}*/
// PackagesWriter::DoPackage - Process a single package			/*{{{*/
// ---------------------------------------------------------------------
/* This method takes a package and gets its control information and 
   MD5, SHA1 and SHA256 then writes out a control record with the proper fields 
   rewritten and the path/size/hash appended. */
bool PackagesWriter::DoPackage(string FileName)
{      
   // Pull all the data we need form the DB
   if (Db.GetFileInfo(FileName,
	    true, /* DoControl */
	    DoContents,
	    true, /* GenContentsOnly */
	    false, /* DoSource */
	    DoHashes, DoAlwaysStat) == false)
   {
     return false;
   }

   unsigned long long FileSize = Db.GetFileSize();
   if (Delink(FileName,OriginalPath,Stats.DeLinkBytes,FileSize) == false)
      return false;
   
   // Lookup the overide information
   pkgTagSection &Tags = Db.Control.Section;
   string Package = Tags.FindS("Package");
   string Architecture;
   // if we generate a Packages file for a given arch, we use it to
   // look for overrides. if we run in "simple" mode without the 
   // "Architecures" variable in the config we use the architecure value
   // from the deb file
   if(Arch != "")
      Architecture = Arch;
   else
      Architecture = Tags.FindS("Architecture");
   unique_ptr<Override::Item> OverItem(Over.GetItem(Package,Architecture));
   
   if (Package.empty() == true)
      return _error->Error(_("Archive had no package field"));

   // If we need to do any rewriting of the header do it now..
   if (OverItem.get() == 0)
   {
      if (NoOverride == false)
      {
	 NewLine(1);
	 ioprintf(c1out, _("  %s has no override entry\n"), Package.c_str());
      }
      
      OverItem = unique_ptr<Override::Item>(new Override::Item);
      OverItem->FieldOverride["Section"] = Tags.FindS("Section");
      OverItem->Priority = Tags.FindS("Priority");
   }

   // Strip the DirStrip prefix from the FileName and add the PathPrefix
   string NewFileName;
   if (DirStrip.empty() == false &&
       FileName.length() > DirStrip.length() &&
       stringcmp(FileName.begin(),FileName.begin() + DirStrip.length(),
		 DirStrip.begin(),DirStrip.end()) == 0)
      NewFileName = string(FileName.begin() + DirStrip.length(),FileName.end());
   else 
      NewFileName = FileName;
   if (PathPrefix.empty() == false)
      NewFileName = flCombine(PathPrefix,NewFileName);

   /* Configuration says we don't want to include the long Description
      in the package file - instead we want to ship a separated file */
   string desc;
   if (LongDescription == false) {
      desc = Tags.FindS("Description").append("\n");
      OverItem->FieldOverride["Description"] = desc.substr(0, desc.find('\n')).c_str();
   }

   // This lists all the changes to the fields we are going to make.
   std::vector<pkgTagSection::Tag> Changes;

   std::string Size;
   strprintf(Size, "%llu", (unsigned long long) FileSize);
   Changes.push_back(pkgTagSection::Tag::Rewrite("Size", Size));

   for (HashStringList::const_iterator hs = Db.HashesList.begin(); hs != Db.HashesList.end(); ++hs)
   {
      if (hs->HashType() == "MD5Sum")
	 Changes.push_back(pkgTagSection::Tag::Rewrite("MD5sum", hs->HashValue()));
      else if (hs->HashType() == "Checksum-FileSize")
	 continue;
      else
	 Changes.push_back(pkgTagSection::Tag::Rewrite(hs->HashType(), hs->HashValue()));
   }
   Changes.push_back(pkgTagSection::Tag::Rewrite("Filename", NewFileName));
   Changes.push_back(pkgTagSection::Tag::Rewrite("Priority", OverItem->Priority));
   Changes.push_back(pkgTagSection::Tag::Remove("Status"));
   Changes.push_back(pkgTagSection::Tag::Remove("Optional"));

   string DescriptionMd5;
   if (LongDescription == false) {
      MD5Summation descmd5;
      descmd5.Add(desc.c_str());
      DescriptionMd5 = descmd5.Result().Value();
      Changes.push_back(pkgTagSection::Tag::Rewrite("Description-md5", DescriptionMd5));
      if (TransWriter != NULL)
	 TransWriter->DoPackage(Package, desc, DescriptionMd5);
   }

   // Rewrite the maintainer field if necessary
   bool MaintFailed;
   string NewMaint = OverItem->SwapMaint(Tags.FindS("Maintainer"),MaintFailed);
   if (MaintFailed == true)
   {
      if (NoOverride == false)
      {
	 NewLine(1);
	 ioprintf(c1out, _("  %s maintainer is %s not %s\n"),
	       Package.c_str(), Tags.FindS("Maintainer").c_str(), OverItem->OldMaint.c_str());
      }
   }

   if (NewMaint.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Maintainer", NewMaint));

   /* Get rid of the Optional tag. This is an ugly, ugly, ugly hack that
      dpkg-scanpackages does. Well sort of. dpkg-scanpackages just does renaming
      but dpkg does this append bit. So we do the append bit, at least that way the
      status file and package file will remain similar. There are other transforms
      but optional is the only legacy one still in use for some lazy reason. */
   string OptionalStr = Tags.FindS("Optional");
   if (OptionalStr.empty() == false)
   {
      if (Tags.FindS("Suggests").empty() == false)
	 OptionalStr = Tags.FindS("Suggests") + ", " + OptionalStr;
      Changes.push_back(pkgTagSection::Tag::Rewrite("Suggests", OptionalStr));
   }

   for (map<string,string>::const_iterator I = OverItem->FieldOverride.begin();
        I != OverItem->FieldOverride.end(); ++I)
      Changes.push_back(pkgTagSection::Tag::Rewrite(I->first, I->second));

   // Rewrite and store the fields.
   if (Tags.Write(*Output, TFRewritePackageOrder, Changes) == false ||
	 Output->Write("\n", 1) == false)
      return false;

   return Db.Finish();
}
									/*}}}*/
PackagesWriter::~PackagesWriter()					/*{{{*/
{
}
									/*}}}*/

// TranslationWriter::TranslationWriter - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* Create a Translation-Master file for this Packages file */
TranslationWriter::TranslationWriter(string const &File, string const &TransCompress,
					mode_t const &Permissions) : Comp(NULL), Output(NULL)
{
   if (File.empty() == true)
      return;

   Comp = new MultiCompress(File, TransCompress, Permissions);
   Output = &Comp->Input;
}
									/*}}}*/
// TranslationWriter::DoPackage - Process a single package		/*{{{*/
// ---------------------------------------------------------------------
/* Create a Translation-Master file for this Packages file */
bool TranslationWriter::DoPackage(string const &Pkg, string const &Desc,
				  string const &MD5)
{
   if (Output == NULL)
      return true;

   // Different archs can include different versions and therefore
   // different descriptions - so we need to check for both name and md5.
   string const Record = Pkg + ":" + MD5;

   if (Included.find(Record) != Included.end())
      return true;

   std::string out;
   strprintf(out, "Package: %s\nDescription-md5: %s\nDescription-en: %s\n",
	   Pkg.c_str(), MD5.c_str(), Desc.c_str());
   Output->Write(out.c_str(), out.length());

   Included.insert(Record);
   return true;
}
									/*}}}*/
// TranslationWriter::~TranslationWriter - Destructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
TranslationWriter::~TranslationWriter()
{
   if (Comp != NULL)
      delete Comp;
}
									/*}}}*/

// SourcesWriter::SourcesWriter - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
SourcesWriter::SourcesWriter(FileFd * const GivenOutput, string const &DB, string const &BOverrides,string const &SOverrides,
			     string const &ExtOverrides) :
   FTWScanner(GivenOutput), Db(DB), Stats(Db.Stats)
{
   AddPattern("*.dsc");
   DeLinkLimit = 0;
   Buffer = 0;
   BufSize = 0;
   
   // Process the command line options
   ConfigToDoHashes(DoHashes, "APT::FTPArchive::Sources");
   NoOverride = _config->FindB("APT::FTPArchive::NoOverrideMsg",false);
   DoAlwaysStat = _config->FindB("APT::FTPArchive::AlwaysStat", false);

   // Read the override file
   if (BOverrides.empty() == false && BOver.ReadOverride(BOverrides) == false)
      return;
   else
      NoOverride = true;

   // WTF?? The logic above: if we can't read binary overrides, don't even try
   // reading source overrides. if we can read binary overrides, then say there
   // are no overrides. THIS MAKES NO SENSE! -- ajt@d.o, 2006/02/28

   if (ExtOverrides.empty() == false)
      SOver.ReadExtraOverride(ExtOverrides);
   
   if (SOverrides.empty() == false && FileExists(SOverrides) == true)
      SOver.ReadOverride(SOverrides,true);
}
									/*}}}*/
// SourcesWriter::DoPackage - Process a single package			/*{{{*/
static std::string getDscHash(unsigned int const DoHashes,
      Hashes::SupportedHashes const DoIt, pkgTagSection &Tags, char const * const FieldName,
      HashString const * const Hash, unsigned long long Size, std::string FileName)
{
   if ((DoHashes & DoIt) != DoIt || Tags.Exists(FieldName) == false || Hash == NULL)
      return "";
   std::ostringstream out;
   out << "\n " << Hash->HashValue() << " " << Size << " " << FileName
      << "\n " << Tags.FindS(FieldName);
   return out.str();
}
bool SourcesWriter::DoPackage(string FileName)
{
   // Pull all the data we need form the DB
   if (Db.GetFileInfo(FileName,
	    false, /* DoControl */
	    false, /* DoContents */
	    false, /* GenContentsOnly */
	    true, /* DoSource */
	    DoHashes, DoAlwaysStat) == false)
   {
      return false;
   }

   // we need to perform a "write" here (this is what finish is doing)
   // because the call to Db.GetFileInfo() in the loop will change
   // the "db cursor"
   Db.Finish();

   pkgTagSection Tags;
   if (Tags.Scan(Db.Dsc.Data.c_str(), Db.Dsc.Data.length()) == false)
      return _error->Error("Could not find a record in the DSC '%s'",FileName.c_str());

   if (Tags.Exists("Source") == false)
      return _error->Error("Could not find a Source entry in the DSC '%s'",FileName.c_str());
   Tags.Trim();

   // Lookup the overide information, finding first the best priority.
   string BestPrio;
   string Bins = Tags.FindS("Binary");
   char Buffer[Bins.length() + 1];
   unique_ptr<Override::Item> OverItem(nullptr);
   if (Bins.empty() == false)
   {
      strcpy(Buffer,Bins.c_str());
      
      // Ignore too-long errors.
      char *BinList[400];
      TokSplitString(',',Buffer,BinList,sizeof(BinList)/sizeof(BinList[0]));
      
      // Look at all the binaries
      unsigned char BestPrioV = pkgCache::State::Extra;
      for (unsigned I = 0; BinList[I] != 0; I++)
      {
	 unique_ptr<Override::Item> Itm(BOver.GetItem(BinList[I]));
	 if (Itm.get() == 0)
	    continue;

	 unsigned char NewPrioV = debListParser::GetPrio(Itm->Priority);
	 if (NewPrioV < BestPrioV || BestPrio.empty() == true)
	 {
	    BestPrioV = NewPrioV;
	    BestPrio = Itm->Priority;
	 }	 

	 if (OverItem.get() == 0)
	    OverItem = std::move(Itm);
      }
   }
   
   // If we need to do any rewriting of the header do it now..
   if (OverItem.get() == 0)
   {
      if (NoOverride == false)
      {
	 NewLine(1);	 
	 ioprintf(c1out, _("  %s has no override entry\n"), Tags.FindS("Source").c_str());
      }
      
      OverItem.reset(new Override::Item);
   }
   
   struct stat St;
   if (stat(FileName.c_str(), &St) != 0)
      return _error->Errno("fstat","Failed to stat %s",FileName.c_str());

   unique_ptr<Override::Item> SOverItem(SOver.GetItem(Tags.FindS("Source")));
   // const unique_ptr<Override::Item> autoSOverItem(SOverItem);
   if (SOverItem.get() == 0)
   {
      ioprintf(c1out, _("  %s has no source override entry\n"), Tags.FindS("Source").c_str());
      SOverItem = unique_ptr<Override::Item>(BOver.GetItem(Tags.FindS("Source")));
      if (SOverItem.get() == 0)
      {
        ioprintf(c1out, _("  %s has no binary override entry either\n"), Tags.FindS("Source").c_str());
	 SOverItem = unique_ptr<Override::Item>(new Override::Item);
	 *SOverItem = *OverItem;
      }
   }

   // Add the dsc to the files hash list
   string const strippedName = flNotDir(FileName);
   std::string const Files = getDscHash(DoHashes, Hashes::MD5SUM, Tags, "Files", Db.HashesList.find("MD5Sum"), St.st_size, strippedName);
   std::string ChecksumsSha1 = getDscHash(DoHashes, Hashes::SHA1SUM, Tags, "Checksums-Sha1", Db.HashesList.find("SHA1"), St.st_size, strippedName);
   std::string ChecksumsSha256 = getDscHash(DoHashes, Hashes::SHA256SUM, Tags, "Checksums-Sha256", Db.HashesList.find("SHA256"), St.st_size, strippedName);
   std::string ChecksumsSha512 = getDscHash(DoHashes, Hashes::SHA512SUM, Tags, "Checksums-Sha512", Db.HashesList.find("SHA512"), St.st_size, strippedName);

   // Strip the DirStrip prefix from the FileName and add the PathPrefix
   string NewFileName;
   if (DirStrip.empty() == false &&
       FileName.length() > DirStrip.length() &&
       stringcmp(DirStrip,OriginalPath,OriginalPath + DirStrip.length()) == 0)
      NewFileName = string(OriginalPath + DirStrip.length());
   else 
      NewFileName = OriginalPath;
   if (PathPrefix.empty() == false)
      NewFileName = flCombine(PathPrefix,NewFileName);

   string Directory = flNotFile(OriginalPath);
   string Package = Tags.FindS("Source");

   // Perform operation over all of the files
   string ParseJnk;
   const char *C = Files.c_str();
   char *RealPath = NULL;
   for (;isspace(*C); C++);
   while (*C != 0)
   {
      // Parse each of the elements
      if (ParseQuoteWord(C,ParseJnk) == false ||
	  ParseQuoteWord(C,ParseJnk) == false ||
	  ParseQuoteWord(C,ParseJnk) == false)
	 return _error->Error("Error parsing file record");

      string OriginalPath = Directory + ParseJnk;

      // Add missing hashes to source files
      if (((DoHashes & Hashes::SHA1SUM) == Hashes::SHA1SUM && !Tags.Exists("Checksums-Sha1")) ||
          ((DoHashes & Hashes::SHA256SUM) == Hashes::SHA256SUM && !Tags.Exists("Checksums-Sha256")) ||
          ((DoHashes & Hashes::SHA512SUM) == Hashes::SHA512SUM && !Tags.Exists("Checksums-Sha512")))
      {
         if (Db.GetFileInfo(OriginalPath,
                            false, /* DoControl */
                            false, /* DoContents */
                            false, /* GenContentsOnly */
                            false, /* DoSource */
                            DoHashes,
                            DoAlwaysStat) == false)
         {
            return _error->Error("Error getting file info");
         }

         for (HashStringList::const_iterator hs = Db.HashesList.begin(); hs != Db.HashesList.end(); ++hs)
	 {
	    if (hs->HashType() == "MD5Sum" || hs->HashType() == "Checksum-FileSize")
	       continue;
	    char const * fieldname;
	    std::string * out;
	    if (hs->HashType() == "SHA1")
	    {
	       fieldname = "Checksums-Sha1";
	       out = &ChecksumsSha1;
	    }
	    else if (hs->HashType() == "SHA256")
	    {
	       fieldname = "Checksums-Sha256";
	       out = &ChecksumsSha256;
	    }
	    else if (hs->HashType() == "SHA512")
	    {
	       fieldname = "Checksums-Sha512";
	       out = &ChecksumsSha512;
	    }
	    else
	    {
	       _error->Warning("Ignoring unknown Checksumtype %s in SourcesWriter::DoPackages", hs->HashType().c_str());
	       continue;
	    }
	    if (Tags.Exists(fieldname) == true)
	       continue;
	    std::ostringstream streamout;
	    streamout << "\n " << hs->HashValue() << " " << Db.GetFileSize() << " " << ParseJnk;
	    out->append(streamout.str());
	 }

	 // write back the GetFileInfo() stats data
	 Db.Finish();
      }

      // Perform the delinking operation
      char Jnk[2];

      if (readlink(OriginalPath.c_str(),Jnk,sizeof(Jnk)) != -1 &&
	  (RealPath = realpath(OriginalPath.c_str(),NULL)) != 0)
      {
	 string RP = RealPath;
	 free(RealPath);
	 if (Delink(RP,OriginalPath.c_str(),Stats.DeLinkBytes,St.st_size) == false)
	    return false;
      }
   }

   Directory = flNotFile(NewFileName);
   if (Directory.length() > 2)
      Directory.erase(Directory.end()-1);

   // This lists all the changes to the fields we are going to make.
   // (5 hardcoded + checksums + maintainer + end marker)
   std::vector<pkgTagSection::Tag> Changes;

   Changes.push_back(pkgTagSection::Tag::Remove("Source"));
   Changes.push_back(pkgTagSection::Tag::Rewrite("Package", Package));
   if (Files.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Files", Files));
   if (ChecksumsSha1.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Checksums-Sha1", ChecksumsSha1));
   if (ChecksumsSha256.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Checksums-Sha256", ChecksumsSha256));
   if (ChecksumsSha512.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Checksums-Sha512", ChecksumsSha512));
   if (Directory != "./")
      Changes.push_back(pkgTagSection::Tag::Rewrite("Directory", Directory));
   Changes.push_back(pkgTagSection::Tag::Rewrite("Priority", BestPrio));
   Changes.push_back(pkgTagSection::Tag::Remove("Status"));

   // Rewrite the maintainer field if necessary
   bool MaintFailed;
   string NewMaint = OverItem->SwapMaint(Tags.FindS("Maintainer"), MaintFailed);
   if (MaintFailed == true)
   {
      if (NoOverride == false)
      {
	 NewLine(1);
	 ioprintf(c1out, _("  %s maintainer is %s not %s\n"), Package.c_str(),
	       Tags.FindS("Maintainer").c_str(), OverItem->OldMaint.c_str());
      }
   }
   if (NewMaint.empty() == false)
      Changes.push_back(pkgTagSection::Tag::Rewrite("Maintainer", NewMaint.c_str()));

   for (map<string,string>::const_iterator I = SOverItem->FieldOverride.begin();
        I != SOverItem->FieldOverride.end(); ++I)
      Changes.push_back(pkgTagSection::Tag::Rewrite(I->first, I->second));

   // Rewrite and store the fields.
   if (Tags.Write(*Output, TFRewriteSourceOrder, Changes) == false ||
	 Output->Write("\n", 1) == false)
      return false;

   Stats.Packages++;
   
   return true;
}
									/*}}}*/

// ContentsWriter::ContentsWriter - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
ContentsWriter::ContentsWriter(FileFd * const GivenOutput, string const &DB, string const &Arch) :
		    FTWScanner(GivenOutput, Arch), Db(DB), Stats(Db.Stats)

{
   SetExts(".deb");
}
									/*}}}*/
// ContentsWriter::DoPackage - Process a single package			/*{{{*/
// ---------------------------------------------------------------------
/* If Package is the empty string the control record will be parsed to
   determine what the package name is. */
bool ContentsWriter::DoPackage(string FileName, string Package)
{
   if (!Db.GetFileInfo(FileName,
	    Package.empty(), /* DoControl */
	    true, /* DoContents */
	    false, /* GenContentsOnly */
	    false, /* DoSource */
	    0, /* DoHashes */
	    false /* checkMtime */))
   {
      return false;
   }

   // Parse the package name
   if (Package.empty() == true)
   {
      Package = Db.Control.Section.FindS("Package");
   }

   Db.Contents.Add(Gen,Package);
   
   return Db.Finish();
}
									/*}}}*/
// ContentsWriter::ReadFromPkgs - Read from a packages file		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ContentsWriter::ReadFromPkgs(string const &PkgFile,string const &PkgCompress)
{
   MultiCompress Pkgs(PkgFile,PkgCompress,0,false);
   if (_error->PendingError() == true)
      return false;

   // Open the package file
   FileFd Fd;
   if (Pkgs.OpenOld(Fd) == false)
      return false;

   pkgTagFile Tags(&Fd);
   if (_error->PendingError() == true)
      return false;

   // Parse.
   pkgTagSection Section;
   while (Tags.Step(Section) == true)
   {
      string File = flCombine(Prefix,Section.FindS("FileName"));
      string Package = Section.FindS("Section");
      if (Package.empty() == false && Package.end()[-1] != '/')
      {
	 Package += '/';
	 Package += Section.FindS("Package");
      }
      else
	 Package += Section.FindS("Package");
	 
      DoPackage(File,Package);
      if (_error->empty() == false)
      {
	 _error->Error("Errors apply to file '%s'",File.c_str());
	 _error->DumpErrors();
      }
   }

   // Tidy the compressor
   Fd.Close();

   return true;
}

									/*}}}*/

// ReleaseWriter::ReleaseWriter - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
ReleaseWriter::ReleaseWriter(FileFd * const GivenOutput, string const &/*DB*/) : FTWScanner(GivenOutput)
{
   if (_config->FindB("APT::FTPArchive::Release::Default-Patterns", true) == true)
   {
      AddPattern("Packages");
      AddPattern("Packages.gz");
      AddPattern("Packages.bz2");
      AddPattern("Packages.lzma");
      AddPattern("Packages.xz");
      AddPattern("Translation-*");
      AddPattern("Sources");
      AddPattern("Sources.gz");
      AddPattern("Sources.bz2");
      AddPattern("Sources.lzma");
      AddPattern("Sources.xz");
      AddPattern("Release");
      AddPattern("Contents-*");
      AddPattern("Index");
      AddPattern("md5sum.txt");
   }
   AddPatterns(_config->FindVector("APT::FTPArchive::Release::Patterns"));

   time_t const now = time(NULL);

   setlocale(LC_TIME, "C");

   char datestr[128];
   if (strftime(datestr, sizeof(datestr), "%a, %d %b %Y %H:%M:%S UTC",
                gmtime(&now)) == 0)
   {
      datestr[0] = '\0';
   }

   time_t const validuntil = now + _config->FindI("APT::FTPArchive::Release::ValidTime", 0);
   char validstr[128];
   if (now == validuntil ||
       strftime(validstr, sizeof(validstr), "%a, %d %b %Y %H:%M:%S UTC",
                gmtime(&validuntil)) == 0)
   {
      validstr[0] = '\0';
   }

   setlocale(LC_TIME, "");

   map<string,string> Fields;
   Fields["Origin"] = "";
   Fields["Label"] = "";
   Fields["Suite"] = "";
   Fields["Version"] = "";
   Fields["Codename"] = "";
   Fields["Date"] = datestr;
   Fields["Valid-Until"] = validstr;
   Fields["Architectures"] = "";
   Fields["Components"] = "";
   Fields["Description"] = "";
   if (_config->FindB("APT::FTPArchive::DoByHash", true) == true)
      Fields["Acquire-By-Hash"] = "true";
   
   for(map<string,string>::const_iterator I = Fields.begin();
       I != Fields.end();
       ++I)
   {
      string Config = string("APT::FTPArchive::Release::") + (*I).first;
      string Value = _config->Find(Config, (*I).second.c_str());
      if (Value == "")
         continue;

      std::string const out = I->first + ": " + Value + "\n";
      Output->Write(out.c_str(), out.length());
   }

   ConfigToDoHashes(DoHashes, "APT::FTPArchive::Release");
}
									/*}}}*/
// ReleaseWriter::DoPackage - Process a single package			/*{{{*/
// ---------------------------------------------------------------------
bool ReleaseWriter::DoPackage(string FileName)
{
   // Strip the DirStrip prefix from the FileName and add the PathPrefix
   string NewFileName;
   if (DirStrip.empty() == false &&
       FileName.length() > DirStrip.length() &&
       stringcmp(FileName.begin(),FileName.begin() + DirStrip.length(),
		 DirStrip.begin(),DirStrip.end()) == 0)
   {
      NewFileName = string(FileName.begin() + DirStrip.length(),FileName.end());
      while (NewFileName[0] == '/')
         NewFileName = string(NewFileName.begin() + 1,NewFileName.end());
   }
   else 
      NewFileName = FileName;

   if (PathPrefix.empty() == false)
      NewFileName = flCombine(PathPrefix,NewFileName);

   FileFd fd(FileName, FileFd::ReadOnly);

   if (!fd.IsOpen())
   {
      return false;
   }

   CheckSums[NewFileName].size = fd.Size();

   Hashes hs(DoHashes);
   hs.AddFD(fd);
   CheckSums[NewFileName].Hashes = hs.GetHashStringList();
   fd.Close();

   // FIXME: wrong layer in the code(?)
   // FIXME2: symlink instead of create a copy
   if (_config->FindB("APT::FTPArchive::DoByHash", true) == true)
   {
      std::string Input = FileName;
      HashStringList hsl = hs.GetHashStringList();
      for(HashStringList::const_iterator h = hsl.begin();
          h != hsl.end(); ++h)
      {
         if (!h->usable())
            continue;
         if (flNotDir(FileName) == "Release" || flNotDir(FileName) == "InRelease")
            continue;

         std::string ByHashOutputFile = GenByHashFilename(Input, *h);
         std::string ByHashOutputDir = flNotFile(ByHashOutputFile);
         if(!CreateDirectory(flNotFile(Input), ByHashOutputDir))
            return _error->Warning("can not create dir %s", flNotFile(ByHashOutputFile).c_str());

         // write new hashes
         FileFd In(Input, FileFd::ReadOnly);
         FileFd Out(ByHashOutputFile, FileFd::WriteEmpty);
         if(!CopyFile(In, Out))
            return _error->Warning("failed to copy %s %s", Input.c_str(), ByHashOutputFile.c_str());
      }
   }

   return true;
}

									/*}}}*/
// ReleaseWriter::Finish - Output the checksums				/*{{{*/
// ---------------------------------------------------------------------
static void printChecksumTypeRecord(FileFd &Output, char const * const Type, map<string, ReleaseWriter::CheckSum> const &CheckSums)
{
   {
      std::string out;
      strprintf(out, "%s:\n", Type);
      Output.Write(out.c_str(), out.length());
   }
   for(map<string,ReleaseWriter::CheckSum>::const_iterator I = CheckSums.begin();
	 I != CheckSums.end(); ++I)
   {
      HashString const * const hs = I->second.Hashes.find(Type);
      if (hs == NULL)
	 continue;
      std::string out;
      strprintf(out, " %s %16llu %s\n",
	    hs->HashValue().c_str(),
	    (*I).second.size,
	    (*I).first.c_str());
      Output.Write(out.c_str(), out.length());
   }
}
void ReleaseWriter::Finish()
{
   if ((DoHashes & Hashes::MD5SUM) == Hashes::MD5SUM)
      printChecksumTypeRecord(*Output, "MD5Sum", CheckSums);
   if ((DoHashes & Hashes::SHA1SUM) == Hashes::SHA1SUM)
      printChecksumTypeRecord(*Output, "SHA1", CheckSums);
   if ((DoHashes & Hashes::SHA256SUM) == Hashes::SHA256SUM)
      printChecksumTypeRecord(*Output, "SHA256", CheckSums);
   if ((DoHashes & Hashes::SHA512SUM) == Hashes::SHA512SUM)
      printChecksumTypeRecord(*Output, "SHA512", CheckSums);

   // go by-hash cleanup
   map<string,ReleaseWriter::CheckSum>::const_iterator prev = CheckSums.begin();
   if (_config->FindB("APT::FTPArchive::DoByHash", true) == true)
   {
      for(map<string,ReleaseWriter::CheckSum>::const_iterator I = CheckSums.begin();
	 I != CheckSums.end(); ++I)
      {
         if (I->first == "Release" || I->first == "InRelease")
            continue;

         // keep iterating until we find a new subdir
         if(flNotFile(I->first) == flNotFile(prev->first))
            continue;

         // clean that subdir up
         int keepFiles = _config->FindI("APT::FTPArchive::By-Hash-Keep", 3);
         // calculate how many compressors are used (the amount of files
         // in that subdir generated for this run)
         keepFiles *= std::distance(prev, I);
         prev = I;

         HashStringList hsl = prev->second.Hashes;
         for(HashStringList::const_iterator h = hsl.begin();
             h != hsl.end(); ++h)
         {

            if (!h->usable())
               continue;

            std::string RealFilename = DirStrip+"/"+prev->first;
            std::string ByHashOutputFile = GenByHashFilename(RealFilename, *h);
            DeleteAllButMostRecent(flNotFile(ByHashOutputFile), keepFiles);
         }
      }
   }
}
