// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   apt-ftparchive - Efficient work-alike for dpkg-scanpackages

   Let contents be disabled from the conf
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-main.h>
#include <apt-private/private-output.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "apt-ftparchive.h"
#include "cachedb.h"
#include "multicompress.h"
#include "override.h"
#include "writer.h"

#include <apti18n.h>
									/*}}}*/

using namespace std;
unsigned Quiet = 0;

static struct timeval GetTimevalFromSteadyClock()			/*{{{*/
{
   auto const Time = std::chrono::steady_clock::now().time_since_epoch();
   auto const Time_sec = std::chrono::duration_cast<std::chrono::seconds>(Time);
   auto const Time_usec = std::chrono::duration_cast<std::chrono::microseconds>(Time - Time_sec);
   return { Time_sec.count(), Time_usec.count() };
}
									/*}}}*/
static auto GetTimeDeltaSince(struct timeval StartTime)			/*{{{*/
{
   auto const NewTime = GetTimevalFromSteadyClock();
   std::chrono::duration<double> Delta =
      std::chrono::seconds(NewTime.tv_sec - StartTime.tv_sec) +
      std::chrono::microseconds(NewTime.tv_usec - StartTime.tv_usec);
   return llround(Delta.count());
}
									/*}}}*/

// struct PackageMap - List of all package files in the config file	/*{{{*/
// ---------------------------------------------------------------------
/* */
struct PackageMap
{
   // General Stuff
   string BaseDir;
   string InternalPrefix;
   string FLFile;
   string PkgExt;
   string SrcExt;
   
   // Stuff for the Package File
   string PkgFile;
   string BinCacheDB;
   string SrcCacheDB;
   string BinOverride;
   string ExtraOverride;

   // We generate for this given arch
   string Arch;
   bool IncludeArchAll;
   
   // Stuff for the Source File
   string SrcFile;
   string SrcOverride;
   string SrcExtraOverride;

   // Translation master file
   bool LongDesc;
   TranslationWriter *TransWriter;

   // Contents 
   string Contents;
   string ContentsHead;
   
   // Random things
   string Tag;
   string PkgCompress;
   string CntCompress;
   string SrcCompress;
   string PathPrefix;
   unsigned int DeLinkLimit;
   mode_t Permissions;
   
   bool ContentsDone;
   bool PkgDone;
   bool SrcDone;
   time_t ContentsMTime;
   
   struct ContentsCompare : public binary_function<PackageMap,PackageMap,bool>
   {
      inline bool operator() (const PackageMap &x,const PackageMap &y)
      {return x.ContentsMTime < y.ContentsMTime;};
   };
    
   struct DBCompare : public binary_function<PackageMap,PackageMap,bool>
   {
      inline bool operator() (const PackageMap &x,const PackageMap &y)
      {return x.BinCacheDB < y.BinCacheDB;};
   };  

   struct SrcDBCompare : public binary_function<PackageMap,PackageMap,bool>
   {
      inline bool operator() (const PackageMap &x,const PackageMap &y)
      {return x.SrcCacheDB < y.SrcCacheDB;};
   };
   
   void GetGeneral(Configuration &Setup,Configuration &Block);
   bool GenPackages(Configuration &Setup,struct CacheDB::Stats &Stats);
   bool GenSources(Configuration &Setup,struct CacheDB::Stats &Stats);
   bool GenContents(Configuration &Setup,
		    vector<PackageMap>::iterator Begin,
		    vector<PackageMap>::iterator End,
		    unsigned long &Left);
   
   PackageMap() : IncludeArchAll(true), LongDesc(true), TransWriter(NULL),
		  DeLinkLimit(0), Permissions(1), ContentsDone(false),
		  PkgDone(false), SrcDone(false), ContentsMTime(0) {};
};
									/*}}}*/

// PackageMap::GetGeneral - Common per-section definitions		/*{{{*/
// ---------------------------------------------------------------------
/* */
void PackageMap::GetGeneral(Configuration &Setup,Configuration &Block)
{
   PathPrefix = Block.Find("PathPrefix");
   
   if (Block.FindB("External-Links",true) == false)
      DeLinkLimit = Setup.FindI("Default::DeLinkLimit", std::numeric_limits<unsigned int>::max());
   else
      DeLinkLimit = 0;
   
   PkgCompress = Block.Find("Packages::Compress",
			    Setup.Find("Default::Packages::Compress",". gzip").c_str());
   CntCompress = Block.Find("Contents::Compress",
			    Setup.Find("Default::Contents::Compress",". gzip").c_str());
   SrcCompress = Block.Find("Sources::Compress",
			    Setup.Find("Default::Sources::Compress",". gzip").c_str());
   
   SrcExt = Block.Find("Sources::Extensions",
		       Setup.Find("Default::Sources::Extensions",".dsc").c_str());
   PkgExt = Block.Find("Packages::Extensions",
		       Setup.Find("Default::Packages::Extensions",".deb").c_str());
   
   Permissions = Setup.FindI("Default::FileMode",0644);

   if (FLFile.empty() == false)
      FLFile = flCombine(Setup.Find("Dir::FileListDir"),FLFile);
   
   if (Contents == " ")
      Contents= string();   
}
									/*}}}*/
// PackageMap::GenPackages - Actually generate a Package file		/*{{{*/
// ---------------------------------------------------------------------
/* This generates the Package File described by this object. */
bool PackageMap::GenPackages(Configuration &Setup,struct CacheDB::Stats &Stats)
{   
   if (PkgFile.empty() == true)
      return true;
   
   string ArchiveDir = Setup.FindDir("Dir::ArchiveDir");
   string OverrideDir = Setup.FindDir("Dir::OverrideDir");
   string CacheDir = Setup.FindDir("Dir::CacheDir");

   struct timeval StartTime = GetTimevalFromSteadyClock();

   PkgDone = true;
   
   // Create a package writer object.
   MultiCompress Comp(flCombine(ArchiveDir,PkgFile),
		      PkgCompress,Permissions);
   PackagesWriter Packages(&Comp.Input, TransWriter, flCombine(CacheDir,BinCacheDB),
			   flCombine(OverrideDir,BinOverride),
			   flCombine(OverrideDir,ExtraOverride),
			   Arch, IncludeArchAll);
   if (PkgExt.empty() == false && Packages.SetExts(PkgExt) == false)
      return _error->Error(_("Package extension list is too long"));
   if (_error->PendingError() == true)
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());
   
   Packages.PathPrefix = PathPrefix;
   Packages.DirStrip = ArchiveDir;
   Packages.InternalPrefix = flCombine(ArchiveDir,InternalPrefix);

   Packages.LongDescription = LongDesc;

   Packages.Stats.DeLinkBytes = Stats.DeLinkBytes;
   Packages.DeLinkLimit = DeLinkLimit;

   if (_error->PendingError() == true)
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());
   
   c0out << ' ' << BaseDir << ":" << flush;
   
   // Do recursive directory searching
   if (FLFile.empty() == true)
   {
      if (Packages.RecursiveScan(flCombine(ArchiveDir,BaseDir)) == false)
	 return false;
   }
   else
   {
      if (Packages.LoadFileList(ArchiveDir,FLFile) == false)
	 return false;
   }
   
   Packages.Output = 0;      // Just in case
   
   // Finish compressing
   unsigned long long Size;
   if (Comp.Finalize(Size) == false)
   {
      c0out << endl;
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());
   }
   
   if (Size != 0)
      c0out << " New "
             << SizeToStr(Size) << "B ";
   else
      c0out << ' ';

   c0out << Packages.Stats.Packages << " files " <<
/*      SizeToStr(Packages.Stats.MD5Bytes) << "B/" << */
      SizeToStr(Packages.Stats.Bytes) << "B " <<
      TimeToStr(GetTimeDeltaSince(StartTime)) << endl;

   if(_config->FindB("APT::FTPArchive::ShowCacheMisses", false) == true)
     c0out << " Misses in Cache: " << Packages.Stats.Misses<< endl;
   
   Stats.Add(Packages.Stats);
   Stats.DeLinkBytes = Packages.Stats.DeLinkBytes;
   
   return !_error->PendingError();
}

									/*}}}*/
// PackageMap::GenSources - Actually generate a Source file		/*{{{*/
// ---------------------------------------------------------------------
/* This generates the Sources File described by this object. */
bool PackageMap::GenSources(Configuration &Setup,struct CacheDB::Stats &Stats)
{   
   if (SrcFile.empty() == true)
      return true;
   
   string ArchiveDir = Setup.FindDir("Dir::ArchiveDir");
   string OverrideDir = Setup.FindDir("Dir::OverrideDir");
   string CacheDir = Setup.FindDir("Dir::CacheDir");

   struct timeval StartTime = GetTimevalFromSteadyClock();
   SrcDone = true;
   
   // Create a package writer object.
   MultiCompress Comp(flCombine(ArchiveDir,SrcFile),
		      SrcCompress,Permissions);
   SourcesWriter Sources(&Comp.Input, flCombine(CacheDir, SrcCacheDB),
			 flCombine(OverrideDir,BinOverride),
			 flCombine(OverrideDir,SrcOverride),
			 flCombine(OverrideDir,SrcExtraOverride));
   if (SrcExt.empty() == false && Sources.SetExts(SrcExt) == false)
      return _error->Error(_("Source extension list is too long"));
   if (_error->PendingError() == true)
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());
   
   Sources.PathPrefix = PathPrefix;
   Sources.DirStrip = ArchiveDir;
   Sources.InternalPrefix = flCombine(ArchiveDir,InternalPrefix);

   Sources.DeLinkLimit = DeLinkLimit;
   Sources.Stats.DeLinkBytes = Stats.DeLinkBytes;

   if (_error->PendingError() == true)
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());

   c0out << ' ' << BaseDir << ":" << flush;
   
   // Do recursive directory searching
   if (FLFile.empty() == true)
   {
      if (Sources.RecursiveScan(flCombine(ArchiveDir,BaseDir))== false)
	 return false;
   }   
   else
   {
      if (Sources.LoadFileList(ArchiveDir,FLFile) == false)
	 return false;
   }
   Sources.Output = 0;      // Just in case
   
   // Finish compressing
   unsigned long long Size;
   if (Comp.Finalize(Size) == false)
   {
      c0out << endl;
      return _error->Error(_("Error processing directory %s"),BaseDir.c_str());
   }
      
   if (Size != 0)
      c0out << " New "
             << SizeToStr(Size) << "B ";
   else
      c0out << ' ';
   
   c0out << Sources.Stats.Packages << " pkgs in " <<
      TimeToStr(GetTimeDeltaSince(StartTime)) << endl;

   if(_config->FindB("APT::FTPArchive::ShowCacheMisses", false) == true)
     c0out << " Misses in Cache: " << Sources.Stats.Misses << endl;

   Stats.Add(Sources.Stats);
   Stats.DeLinkBytes = Sources.Stats.DeLinkBytes;
   
   return !_error->PendingError();
}
									/*}}}*/
// PackageMap::GenContents - Actually generate a Contents file		/*{{{*/
// ---------------------------------------------------------------------
/* This generates the contents file partially described by this object.
   It searches the given iterator range for other package files that map
   into this contents file and includes their data as well when building. */
bool PackageMap::GenContents(Configuration &Setup,
			     vector<PackageMap>::iterator Begin,
			     vector<PackageMap>::iterator End,
			     unsigned long &Left)
{
   if (Contents.empty() == true)
      return true;
   
   if (Left == 0)
      return true;
   
   string ArchiveDir = Setup.FindDir("Dir::ArchiveDir");
   string CacheDir = Setup.FindDir("Dir::CacheDir");
   string OverrideDir = Setup.FindDir("Dir::OverrideDir");

   struct timeval StartTime = GetTimevalFromSteadyClock();

   // Create a package writer object.
   MultiCompress Comp(flCombine(ArchiveDir,this->Contents),
		      CntCompress,Permissions);
   Comp.UpdateMTime = Setup.FindI("Default::ContentsAge",10)*24*60*60;
   ContentsWriter Contents(&Comp.Input, "", Arch, IncludeArchAll);
   if (PkgExt.empty() == false && Contents.SetExts(PkgExt) == false)
      return _error->Error(_("Package extension list is too long"));
   if (_error->PendingError() == true)
      return false;

   if (_error->PendingError() == true)
      return false;

   // Write the header out.
   if (ContentsHead.empty() == false)
   {
      FileFd Head(flCombine(OverrideDir,ContentsHead),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return false;

      unsigned long long Size = Head.Size();
      unsigned char Buf[4096];
      while (Size != 0)
      {
	 unsigned long long ToRead = Size;
	 if (Size > sizeof(Buf))
	    ToRead = sizeof(Buf);

	 if (Head.Read(Buf,ToRead) == false)
	    return false;

	 if (Comp.Input.Write(Buf, ToRead) == false)
	    return _error->Errno("fwrite",_("Error writing header to contents file"));

	 Size -= ToRead;
      }
   }

   /* Go over all the package file records and parse all the package
      files associated with this contents file into one great big honking
      memory structure, then dump the sorted version */
   c0out << ' ' << this->Contents << ":" << flush;
   for (vector<PackageMap>::iterator I = Begin; I != End; ++I)
   {
      if (I->Contents != this->Contents)
	 continue;
      
      Contents.Prefix = ArchiveDir;
      Contents.ReadyDB(flCombine(CacheDir,I->BinCacheDB));
      Contents.ReadFromPkgs(flCombine(ArchiveDir,I->PkgFile),
			    I->PkgCompress);
      
      I->ContentsDone = true;	    
   }
   
   Contents.Finish();
   
   // Finish compressing
   unsigned long long Size;
   if (Comp.Finalize(Size) == false || _error->PendingError() == true)
   {
      c0out << endl;
      return _error->Error(_("Error processing contents %s"),
			   this->Contents.c_str());
   }
   
   if (Size != 0)
   {
      c0out << " New " << SizeToStr(Size) << "B ";
      if (Left > Size)
	 Left -= Size;
      else
	 Left = 0;
   }
   else
      c0out << ' ';
   
   if(_config->FindB("APT::FTPArchive::ShowCacheMisses", false) == true)
     c0out << " Misses in Cache: " << Contents.Stats.Misses<< endl;

   c0out << Contents.Stats.Packages << " files " <<
      SizeToStr(Contents.Stats.Bytes) << "B " <<
      TimeToStr(GetTimeDeltaSince(StartTime)) << endl;
   
   return true;
}
									/*}}}*/

// LoadTree - Load a 'tree' section from the Generate Config		/*{{{*/
// ---------------------------------------------------------------------
/* This populates the PkgList with all the possible permutations of the
   section/arch lists. */
static void LoadTree(vector<PackageMap> &PkgList, std::vector<TranslationWriter*> &TransList, Configuration &Setup)
{   
   // Load the defaults
   string DDir = Setup.Find("TreeDefault::Directory",
			    "$(DIST)/$(SECTION)/binary-$(ARCH)/");
   string DSDir = Setup.Find("TreeDefault::SrcDirectory",
			    "$(DIST)/$(SECTION)/source/");
   string DPkg = Setup.Find("TreeDefault::Packages",
			    "$(DIST)/$(SECTION)/binary-$(ARCH)/Packages");
   string DTrans = Setup.Find("TreeDefault::Translation",
			    "$(DIST)/$(SECTION)/i18n/Translation-en");
   string DIPrfx = Setup.Find("TreeDefault::InternalPrefix",
			    "$(DIST)/$(SECTION)/");
   string DContents = Setup.Find("TreeDefault::Contents",
			    "$(DIST)/$(SECTION)/Contents-$(ARCH)");
   string DContentsH = Setup.Find("TreeDefault::Contents::Header","");
   string DBCache = Setup.Find("TreeDefault::BinCacheDB",
			       "packages-$(ARCH).db");
   string SrcDBCache = Setup.Find("TreeDefault::SrcCacheDB",
			       "sources-$(SECTION).db");
   string DSources = Setup.Find("TreeDefault::Sources",
				"$(DIST)/$(SECTION)/source/Sources");
   string DFLFile = Setup.Find("TreeDefault::FileList", "");
   string DSFLFile = Setup.Find("TreeDefault::SourceFileList", "");

   mode_t const Permissions = Setup.FindI("Default::FileMode",0644);

   bool const LongDescription = Setup.FindB("Default::LongDescription",
					_config->FindB("APT::FTPArchive::LongDescription", true));
   string const TranslationCompress = Setup.Find("Default::Translation::Compress",". gzip").c_str();
   bool const ConfIncludeArchAllExists = _config->Exists("APT::FTPArchive::IncludeArchitectureAll");
   bool const ConfIncludeArchAll = _config->FindB("APT::FTPArchive::IncludeArchitectureAll", true);

   // Process 'tree' type sections
   const Configuration::Item *Top = Setup.Tree("tree");
   for (Top = (Top == 0?0:Top->Child); Top != 0;)
   {
      Configuration Block(Top);
      string Dist = Top->Tag;

      // Parse the sections
      string Tmp = Block.Find("Sections");
      const char *Sections = Tmp.c_str();
      string Section;
      while (ParseQuoteWord(Sections,Section) == true)
      {
	 struct SubstVar Vars[] = {{"$(DIST)",&Dist},
					 {"$(SECTION)",&Section},
					 {"$(ARCH)",nullptr},
					 {nullptr, nullptr}};
	 mode_t const Perms = Block.FindI("FileMode", Permissions);
	 bool const LongDesc = Block.FindB("LongDescription", LongDescription);
	 TranslationWriter *TransWriter = nullptr;

	 std::string Tmp2 = Block.Find("Architectures");
	 std::transform(Tmp2.begin(), Tmp2.end(), Tmp2.begin(), ::tolower);
	 std::vector<std::string> const Archs = VectorizeString(Tmp2, ' ');
	 bool IncludeArchAll;
	 if (ConfIncludeArchAllExists == true)
	    IncludeArchAll = ConfIncludeArchAll;
	 else
	    IncludeArchAll = std::find(Archs.begin(), Archs.end(), "all") == Archs.end();
	 for (auto const& Arch: Archs)
	 {
	    if (Arch.empty()) continue;
	    Vars[2].Contents = &Arch;
	    PackageMap Itm;
	    Itm.Permissions = Perms;
	    Itm.BinOverride = SubstVar(Block.Find("BinOverride"),Vars);
	    Itm.InternalPrefix = SubstVar(Block.Find("InternalPrefix",DIPrfx.c_str()),Vars);

	    if (Arch == "source")
	    {
	       Itm.SrcOverride = SubstVar(Block.Find("SrcOverride"),Vars);
	       Itm.BaseDir = SubstVar(Block.Find("SrcDirectory",DSDir.c_str()),Vars);
	       Itm.SrcFile = SubstVar(Block.Find("Sources",DSources.c_str()),Vars);
	       Itm.Tag = SubstVar("$(DIST)/$(SECTION)/source",Vars);
	       Itm.FLFile = SubstVar(Block.Find("SourceFileList",DSFLFile.c_str()),Vars);
	       Itm.SrcExtraOverride = SubstVar(Block.Find("SrcExtraOverride"),Vars);
	       Itm.SrcCacheDB = SubstVar(Block.Find("SrcCacheDB",SrcDBCache.c_str()),Vars);
	    }
	    else
	    {
	       Itm.BinCacheDB = SubstVar(Block.Find("BinCacheDB",DBCache.c_str()),Vars);
	       Itm.BaseDir = SubstVar(Block.Find("Directory",DDir.c_str()),Vars);
	       Itm.PkgFile = SubstVar(Block.Find("Packages",DPkg.c_str()),Vars);
	       Itm.Tag = SubstVar("$(DIST)/$(SECTION)/$(ARCH)",Vars);
	       Itm.Arch = Arch;
	       Itm.IncludeArchAll = IncludeArchAll;
	       Itm.LongDesc = LongDesc;
	       if (TransWriter == NULL && DTrans.empty() == false && LongDesc == false && DTrans != "/dev/null")
	       {
		  string const TranslationFile = flCombine(Setup.FindDir("Dir::ArchiveDir"),
			SubstVar(Block.Find("Translation", DTrans.c_str()), Vars));
		  string const TransCompress = Block.Find("Translation::Compress", TranslationCompress);
		  TransWriter = new TranslationWriter(TranslationFile, TransCompress, Perms);
		  TransList.push_back(TransWriter);
	       }
	       Itm.TransWriter = TransWriter;
	       Itm.Contents = SubstVar(Block.Find("Contents",DContents.c_str()),Vars);
	       Itm.ContentsHead = SubstVar(Block.Find("Contents::Header",DContentsH.c_str()),Vars);
	       Itm.FLFile = SubstVar(Block.Find("FileList",DFLFile.c_str()),Vars);
	       Itm.ExtraOverride = SubstVar(Block.Find("ExtraOverride"),Vars);
	    }

	    Itm.GetGeneral(Setup,Block);
	    PkgList.push_back(Itm);
	 }
      }

      Top = Top->Next;
   }
}
									/*}}}*/
static void UnloadTree(std::vector<TranslationWriter*> const &Trans)		/*{{{*/
{
   for (std::vector<TranslationWriter*>::const_reverse_iterator T = Trans.rbegin(); T != Trans.rend(); ++T)
      delete *T;
}
									/*}}}*/
// LoadBinDir - Load a 'bindirectory' section from the Generate Config	/*{{{*/
// ---------------------------------------------------------------------
/* */
static void LoadBinDir(vector<PackageMap> &PkgList,Configuration &Setup)
{
   mode_t const Permissions = Setup.FindI("Default::FileMode",0644);

   // Process 'bindirectory' type sections
   const Configuration::Item *Top = Setup.Tree("bindirectory");
   for (Top = (Top == 0?0:Top->Child); Top != 0;)
   {
      Configuration Block(Top);
      
      PackageMap Itm;
      Itm.PkgFile = Block.Find("Packages");
      Itm.SrcFile = Block.Find("Sources");
      Itm.BinCacheDB = Block.Find("BinCacheDB");
      Itm.SrcCacheDB = Block.Find("SrcCacheDB");
      Itm.BinOverride = Block.Find("BinOverride");
      Itm.ExtraOverride = Block.Find("ExtraOverride");
      Itm.SrcExtraOverride = Block.Find("SrcExtraOverride");
      Itm.SrcOverride = Block.Find("SrcOverride");
      Itm.BaseDir = Top->Tag;
      Itm.FLFile = Block.Find("FileList");
      Itm.InternalPrefix = Block.Find("InternalPrefix",Top->Tag.c_str());
      Itm.Contents = Block.Find("Contents");
      Itm.ContentsHead = Block.Find("Contents::Header");
      Itm.Permissions = Block.FindI("FileMode", Permissions);
      
      Itm.GetGeneral(Setup,Block);
      PkgList.push_back(Itm);
      
      Top = Top->Next;
   }      
}
									/*}}}*/

static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
    _("Usage: apt-ftparchive [options] command\n"
      "Commands: packages binarypath [overridefile [pathprefix]]\n"
      "          sources srcpath [overridefile [pathprefix]]\n"
      "          contents path\n"
      "          release path\n"
      "          generate config [groups]\n"
      "          clean config\n"
      "\n"
      "apt-ftparchive generates index files for Debian archives. It supports\n"
      "many styles of generation from fully automated to functional replacements\n"
      "for dpkg-scanpackages and dpkg-scansources\n"
      "\n"
      "apt-ftparchive generates Package files from a tree of .debs. The\n"
      "Package file contains the contents of all the control fields from\n"
      "each package as well as the MD5 hash and filesize. An override file\n"
      "is supported to force the value of Priority and Section.\n"
      "\n"
      "Similarly apt-ftparchive generates Sources files from a tree of .dscs.\n"
      "The --source-override option can be used to specify a src override file\n"
      "\n"
      "The 'packages' and 'sources' command should be run in the root of the\n"
      "tree. BinaryPath should point to the base of the recursive search and \n"
      "override file should contain the override flags. Pathprefix is\n"
      "appended to the filename fields if present. Example usage from the \n"
      "Debian archive:\n"
      "   apt-ftparchive packages dists/potato/main/binary-i386/ > \\\n"
      "               dists/potato/main/binary-i386/Packages\n"
      "\n"
      "Options:\n"
      "  -h    This help text\n"
      "  --md5 Control MD5 generation\n"
      "  -s=?  Source override file\n"
      "  -q    Quiet\n"
      "  -d=?  Select the optional caching database\n"
      "  --no-delink Enable delinking debug mode\n"
      "  --contents  Control contents file generation\n"
      "  -c=?  Read this configuration file\n"
      "  -o=?  Set an arbitrary configuration option") << endl;
   return true;
}
									/*}}}*/
// SimpleGenPackages - Generate a Packages file for a directory tree	/*{{{*/
// ---------------------------------------------------------------------
/* This emulates dpkg-scanpackages's command line interface. 'mostly' */
static bool SimpleGenPackages(CommandLine &CmdL)
{
   if (CmdL.FileSize() < 2)
      return ShowHelp(CmdL);
   
   string Override;
   if (CmdL.FileSize() >= 3)
      Override = CmdL.FileList[2];
   
   // Create a package writer object.
   PackagesWriter Packages(NULL, NULL, _config->Find("APT::FTPArchive::DB"),
			   Override, "", _config->Find("APT::FTPArchive::Architecture"),
			   _config->FindB("APT::FTPArchive::IncludeArchitectureAll", true));
   if (_error->PendingError() == true)
      return false;
   
   if (CmdL.FileSize() >= 4)
      Packages.PathPrefix = CmdL.FileList[3];
   
   // Do recursive directory searching
   if (Packages.RecursiveScan(CmdL.FileList[1]) == false)
      return false;

   // Give some stats if asked for
   if(_config->FindB("APT::FTPArchive::ShowCacheMisses", false) == true)
     c0out << " Misses in Cache: " << Packages.Stats.Misses<< endl;

   return true;
}
									/*}}}*/
// SimpleGenContents - Generate a Contents listing			/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool SimpleGenContents(CommandLine &CmdL)
{
   if (CmdL.FileSize() < 2)
      return ShowHelp(CmdL);
   
   // Create a package writer object.
   ContentsWriter Contents(NULL, _config->Find("APT::FTPArchive::DB"), _config->Find("APT::FTPArchive::Architecture"));
   if (_error->PendingError() == true)
      return false;
   
   // Do recursive directory searching
   if (Contents.RecursiveScan(CmdL.FileList[1]) == false)
      return false;

   Contents.Finish();
   
   return true;
}
									/*}}}*/
// SimpleGenSources - Generate a Sources file for a directory tree	/*{{{*/
// ---------------------------------------------------------------------
/* This emulates dpkg-scanpackages's command line interface. 'mostly' */
static bool SimpleGenSources(CommandLine &CmdL)
{
   if (CmdL.FileSize() < 2)
      return ShowHelp(CmdL);
   
   string Override;
   if (CmdL.FileSize() >= 3)
      Override = CmdL.FileList[2];
   
   string SOverride;
   if (Override.empty() == false)
      SOverride = Override + ".src";
   
   SOverride = _config->Find("APT::FTPArchive::SourceOverride",
			     SOverride.c_str());
       
   // Create a package writer object.
   SourcesWriter Sources(NULL, _config->Find("APT::FTPArchive::DB"),Override,SOverride);
   if (_error->PendingError() == true)
      return false;
   
   if (CmdL.FileSize() >= 4)
      Sources.PathPrefix = CmdL.FileList[3];
   
   // Do recursive directory searching
   if (Sources.RecursiveScan(CmdL.FileList[1]) == false)
      return false;

   // Give some stats if asked for
   if(_config->FindB("APT::FTPArchive::ShowCacheMisses", false) == true)
     c0out << " Misses in Cache: " << Sources.Stats.Misses<< endl;

   return true;
}
									/*}}}*/
// SimpleGenRelease - Generate a Release file for a directory tree	/*{{{*/
// ---------------------------------------------------------------------
static bool SimpleGenRelease(CommandLine &CmdL)
{
   if (CmdL.FileSize() < 2)
      return ShowHelp(CmdL);

   string Dir = CmdL.FileList[1];

   ReleaseWriter Release(NULL, "");
   Release.DirStrip = Dir;

   if (_error->PendingError() == true)
      return false;

   if (Release.RecursiveScan(Dir) == false)
      return false;

   Release.Finish();

   return true;
}

									/*}}}*/
// DoGeneratePackagesAndSources - Helper for Generate                   /*{{{*/
// ---------------------------------------------------------------------
static bool DoGeneratePackagesAndSources(Configuration &Setup,
					 vector<PackageMap> &PkgList,
					 struct CacheDB::Stats &SrcStats,
					 struct CacheDB::Stats &Stats,
					 CommandLine &CmdL)
{
   if (CmdL.FileSize() <= 2)
   {
      for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); ++I)
	 if (I->GenPackages(Setup,Stats) == false)
	    _error->DumpErrors();
      for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); ++I)
	 if (I->GenSources(Setup,SrcStats) == false)
	    _error->DumpErrors();
   }
   else
   {
      // Make a choice list out of the package list..
      RxChoiceList *List = new RxChoiceList[2*PkgList.size()+1];
      RxChoiceList *End = List;
      for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); ++I)
      {
	 End->UserData = &(*I);
	 End->Str = I->BaseDir.c_str();
	 End++;
	 
	 End->UserData = &(*I);
	 End->Str = I->Tag.c_str();
	 End++;	 
      }
      End->Str = 0;
     
      // Regex it
      if (RegexChoice(List,CmdL.FileList + 2,CmdL.FileList + CmdL.FileSize()) == 0)
      {
	 delete [] List;
	 return _error->Error(_("No selections matched"));
      }
      _error->DumpErrors();
      
      // Do the generation for Packages
      for (End = List; End->Str != 0; ++End)
      {
	 if (End->Hit == false)
	    continue;
	 
	 PackageMap * const I = static_cast<PackageMap *>(End->UserData);
	 if (I->PkgDone == true)
	    continue;
	 if (I->GenPackages(Setup,Stats) == false)
	    _error->DumpErrors();
      }
      
      // Do the generation for Sources
      for (End = List; End->Str != 0; ++End)
      {
	 if (End->Hit == false)
	    continue;
	 
	 PackageMap * const I = static_cast<PackageMap *>(End->UserData);
	 if (I->SrcDone == true)
	    continue;
	 if (I->GenSources(Setup,SrcStats) == false)
	    _error->DumpErrors();
      }
      
      delete [] List;
   }
   return true;
}

                                                                        /*}}}*/
// DoGenerateContents - Helper for Generate to generate the Contents    /*{{{*/
// ---------------------------------------------------------------------
static bool DoGenerateContents(Configuration &Setup,
			       vector<PackageMap> &PkgList,
			       CommandLine &CmdL)
{
   c1out << "Packages done, Starting contents." << endl;

   // Sort the contents file list by date
   string ArchiveDir = Setup.FindDir("Dir::ArchiveDir");
   for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); ++I)
   {
      struct stat A;
      if (MultiCompress::GetStat(flCombine(ArchiveDir,I->Contents),
				 I->CntCompress,A) == false)
	 time(&I->ContentsMTime);
      else
	 I->ContentsMTime = A.st_mtime;
   }
   stable_sort(PkgList.begin(),PkgList.end(),PackageMap::ContentsCompare());
   
   /* Now for Contents.. The process here is to do a make-like dependency
      check. Each contents file is verified to be newer than the package files
      that describe the debs it indexes. Since the package files contain 
      hashes of the .debs this means they have not changed either so the 
      contents must be up to date. */
   unsigned long MaxContentsChange = Setup.FindI("Default::MaxContentsChange",
                                                 std::numeric_limits<unsigned int>::max())*1024;
   for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); ++I)
   {
      // This record is not relevant
      if (I->ContentsDone == true ||
	  I->Contents.empty() == true)
	 continue;

      // Do not do everything if the user specified sections.
      if (CmdL.FileSize() > 2 && I->PkgDone == false)
	 continue;

      struct stat A,B;
      if (MultiCompress::GetStat(flCombine(ArchiveDir,I->Contents),I->CntCompress,A) == true)
      {
	 if (MultiCompress::GetStat(flCombine(ArchiveDir,I->PkgFile),I->PkgCompress,B) == false)
	 {
	    _error->Warning(_("Some files are missing in the package file group `%s'"),I->PkgFile.c_str());
	    continue;
	 }
	 
	 if (A.st_mtime > B.st_mtime)
	    continue;
      }
      
      if (I->GenContents(Setup,PkgList.begin(),PkgList.end(),
			 MaxContentsChange) == false)
	 _error->DumpErrors();
      
      // Hit the limit?
      if (MaxContentsChange == 0)
      {
	 c1out << "Hit contents update byte limit" << endl;
	 break;
      }      
   }

   return true;
}

									/*}}}*/
// Generate - Full generate, using a config file			/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool Generate(CommandLine &CmdL)
{
   struct CacheDB::Stats SrcStats;
   if (CmdL.FileSize() < 2)
      return ShowHelp(CmdL);

   struct timeval StartTime = GetTimevalFromSteadyClock();
   struct CacheDB::Stats Stats;
   
   // Read the configuration file.
   Configuration Setup;
   if (ReadConfigFile(Setup,CmdL.FileList[1],true) == false)
      return false;

   vector<PackageMap> PkgList;
   std::vector<TranslationWriter*> TransList;
   LoadTree(PkgList, TransList, Setup);
   LoadBinDir(PkgList,Setup);

   // Sort by cache DB to improve IO locality.
   stable_sort(PkgList.begin(),PkgList.end(),PackageMap::DBCompare());
   stable_sort(PkgList.begin(),PkgList.end(),PackageMap::SrcDBCompare());

   // Generate packages
   if (_config->FindB("APT::FTPArchive::ContentsOnly", false) == false)
   {
      if(DoGeneratePackagesAndSources(Setup, PkgList, SrcStats, Stats, CmdL) == false)
      {
	 UnloadTree(TransList);
         return false;
      }
   } else {
      c1out << "Skipping Packages/Sources generation" << endl;
   }

   // do Contents if needed
   if (_config->FindB("APT::FTPArchive::Contents", true) == true)
      if (DoGenerateContents(Setup, PkgList, CmdL) == false)
      {
	 UnloadTree(TransList);
	 return false;
      }

   c1out << "Done. " << SizeToStr(Stats.Bytes) << "B in " << Stats.Packages
         << " archives. Took " << TimeToStr(GetTimeDeltaSince(StartTime)) << endl;

   UnloadTree(TransList);
   return true;
}

                                                                        /*}}}*/
// Clean - Clean out the databases					/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool Clean(CommandLine &CmdL)
{
   if (CmdL.FileSize() != 2)
      return ShowHelp(CmdL);

   // Read the configuration file.
   Configuration Setup;
   if (ReadConfigFile(Setup,CmdL.FileList[1],true) == false)
      return false;
   // we don't need translation creation here
   Setup.Set("TreeDefault::Translation", "/dev/null");

   vector<PackageMap> PkgList;
   std::vector<TranslationWriter*> TransList;
   LoadTree(PkgList, TransList, Setup);
   LoadBinDir(PkgList,Setup);

   // Sort by cache DB to improve IO locality.
   stable_sort(PkgList.begin(),PkgList.end(),PackageMap::DBCompare());
   stable_sort(PkgList.begin(),PkgList.end(),PackageMap::SrcDBCompare());

   string CacheDir = Setup.FindDir("Dir::CacheDir");

   for (vector<PackageMap>::iterator I = PkgList.begin(); I != PkgList.end(); )
   {
      if(I->BinCacheDB != "")
         c0out << I->BinCacheDB << endl;
      if(I->SrcCacheDB != "")
         c0out << I->SrcCacheDB << endl;
      CacheDB DB(flCombine(CacheDir,I->BinCacheDB));
      CacheDB DB_SRC(flCombine(CacheDir,I->SrcCacheDB));
      if (DB.Clean() == false)
	 _error->DumpErrors();
      if (DB_SRC.Clean() == false)
	 _error->DumpErrors();

      I = std::find_if(I, PkgList.end(),
	    [&](PackageMap const &PM) { return PM.BinCacheDB != I->BinCacheDB || PM.SrcCacheDB != I->SrcCacheDB;
      });
   }

   return true;
}
									/*}}}*/

static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {"packages",&SimpleGenPackages, nullptr},
      {"contents",&SimpleGenContents, nullptr},
      {"sources",&SimpleGenSources, nullptr},
      {"release",&SimpleGenRelease, nullptr},
      {"generate",&Generate, nullptr},
      {"clean",&Clean, nullptr},
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc, const char *argv[])					/*{{{*/
{
   // Parse the command line and initialize the package library
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_FTPARCHIVE, &_config, NULL, argc, argv, ShowHelp, &GetCommands);

   _config->CndSet("quiet",0);
   Quiet = _config->FindI("quiet",0);
   InitOutput(clog.rdbuf());

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/
