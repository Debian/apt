// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <ctype.h>
#include <stddef.h>
#include <time.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// Global list of Items supported
static pkgSourceList::Type *ItmList[10];
pkgSourceList::Type **pkgSourceList::Type::GlobalList = ItmList;
unsigned long pkgSourceList::Type::GlobalListLen = 0;

static std::vector<std::string> FindMultiValue(pkgTagSection &Tags, char const *const Field) /*{{{*/
{
   auto values = Tags.FindS(Field);
   // we ignore duplicate spaces by removing empty values
   std::replace_if(values.begin(), values.end(), isspace_ascii, ' ');
   auto vect = VectorizeString(values, ' ');
   vect.erase(std::remove_if(vect.begin(), vect.end(), [](std::string const &s) { return s.empty(); }), vect.end());
   return vect;
}
									/*}}}*/

// Type::Type - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* Link this to the global list of items*/
pkgSourceList::Type::Type(char const * const pName, char const * const pLabel) : Name(pName), Label(pLabel)
{
   ItmList[GlobalListLen] = this;
   ++GlobalListLen;
}
pkgSourceList::Type::~Type() {}
									/*}}}*/
// Type::GetType - Get a specific meta for a given type			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::Type *pkgSourceList::Type::GetType(const char *Type)
{
   for (unsigned I = 0; I != GlobalListLen; ++I)
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

   URI = ::URI{SubstVar(URI, "$(ARCH)", _config->Find("APT::Architecture"))};

   // Make sure that the URI is / postfixed
   if (URI.back() != '/')
      URI.push_back('/');

   return true;
}
									/*}}}*/
bool pkgSourceList::Type::ParseStanza(vector<metaIndex *> &List,	/*{{{*/
                                      pkgTagSection &Tags,
                                      unsigned int const i,
                                      FileFd &Fd)
{
   map<string, string> Options;

   string Enabled = Tags.FindS("Enabled");
   if (Enabled.empty() == false && StringToBool(Enabled) == false)
      return true;

   std::map<char const * const, std::pair<char const * const, bool> > mapping;
#define APT_PLUSMINUS(X, Y) \
   mapping.insert(std::make_pair(X, std::make_pair(Y, true))); \
   mapping.insert(std::make_pair(X "-Add", std::make_pair(Y "+", true))); \
   mapping.insert(std::make_pair(X "-Remove", std::make_pair(Y "-", true)))
   APT_PLUSMINUS("Architectures", "arch");
   APT_PLUSMINUS("Languages", "lang");
   APT_PLUSMINUS("Targets", "target");
#undef APT_PLUSMINUS
   mapping.insert(std::make_pair("Trusted", std::make_pair("trusted", false)));
   mapping.insert(std::make_pair("Check-Valid-Until", std::make_pair("check-valid-until", false)));
   mapping.insert(std::make_pair("Valid-Until-Min", std::make_pair("valid-until-min", false)));
   mapping.insert(std::make_pair("Valid-Until-Max", std::make_pair("valid-until-max", false)));
   mapping.insert(std::make_pair("Check-Date", std::make_pair("check-date", false)));
   mapping.insert(std::make_pair("Date-Max-Future", std::make_pair("date-max-future", false)));
   mapping.insert(std::make_pair("Signed-By", std::make_pair("signed-by", false)));
   mapping.insert(std::make_pair("PDiffs", std::make_pair("pdiffs", false)));
   mapping.insert(std::make_pair("By-Hash", std::make_pair("by-hash", false)));

   for (std::map<char const * const, std::pair<char const * const, bool> >::const_iterator m = mapping.begin(); m != mapping.end(); ++m)
      if (Tags.Exists(m->first))
      {
	 if (m->second.second)
	 {
	    auto const values = FindMultiValue(Tags, m->first);
	    Options[m->second.first] = APT::String::Join(values, ",");
	 }
	 else
	    Options[m->second.first] = Tags.FindS(m->first);
      }

   {
      std::string entry;
      strprintf(entry, "%s:%i", Fd.Name().c_str(), i);
      Options["sourceslist-entry"] = entry;
   }

   // now create one item per suite/section
   auto const list_uris = FindMultiValue(Tags, "URIs");
   auto const list_comp = FindMultiValue(Tags, "Components");
   auto list_suite = FindMultiValue(Tags, "Suites");
   {
      auto const nativeArch = _config->Find("APT::Architecture");
      std::transform(list_suite.begin(), list_suite.end(), list_suite.begin(),
		     [&](std::string const &suite) { return SubstVar(suite, "$(ARCH)", nativeArch); });
   }

   if (list_uris.empty())
      // TRANSLATOR: %u is a line number, the first %s is a filename of a file with the extension "second %s" and the third %s is a unique identifier for bugreports
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), i, "sources", Fd.Name().c_str(), "URI");

   if (list_suite.empty())
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), i, "sources", Fd.Name().c_str(), "Suite");

   for (auto URI : list_uris)
   {
      if (FixupURI(URI) == false)
	 return _error->Error(_("Malformed entry %u in %s file %s (%s)"), i, "sources", Fd.Name().c_str(), "URI parse");

      for (auto const &S : list_suite)
      {
	 if (likely(S.empty() == false) && S[S.size() - 1] == '/')
	 {
	    if (list_comp.empty() == false)
	       return _error->Error(_("Malformed entry %u in %s file %s (%s)"), i, "sources", Fd.Name().c_str(), "absolute Suite Component");
	    if (CreateItem(List, URI, S, "", Options) == false)
	       return false;
	 }
	 else
	 {
	    if (list_comp.empty())
	       return _error->Error(_("Malformed entry %u in %s file %s (%s)"), i, "sources", Fd.Name().c_str(), "Component");

	    for (auto const &C : list_comp)
	       if (CreateItem(List, URI, S, C, Options) == false)
		  return false;
	 }
      }
   }
   return true;
}
									/*}}}*/
// Type::ParseLine - Parse a single line				/*{{{*/
// ---------------------------------------------------------------------
/* This is a generic one that is the 'usual' format for sources.list
   Weird types may override this. */
bool pkgSourceList::Type::ParseLine(vector<metaIndex *> &List,
				    const char *Buffer,
				    unsigned int const CurLine,
				    string const &File) const
{
   for (;Buffer != 0 && isspace(*Buffer); ++Buffer); // Skip whitespaces

   // Parse option field if it exists
   // e.g.: [ option1=value1 option2=value2 ]
   map<string, string> Options;
   {
      std::string entry;
      strprintf(entry, "%s:%i", File.c_str(), CurLine);
      Options["sourceslist-entry"] = entry;
   }
   if (Buffer != 0 && Buffer[0] == '[')
   {
      ++Buffer; // ignore the [
      for (;Buffer != 0 && isspace(*Buffer); ++Buffer); // Skip whitespaces
      while (*Buffer != ']')
      {
	 // get one option, e.g. option1=value1
	 string option;
	 if (ParseQuoteWord(Buffer,option) == false)
	    return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "[option] unparsable");

	 if (option.length() < 3)
	    return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "[option] too short");

	 // accept options even if the last has no space before the ]-end marker
	 if (option.at(option.length()-1) == ']')
	 {
	    for (; *Buffer != ']'; --Buffer);
	    option.resize(option.length()-1);
	 }

	 size_t const needle = option.find('=');
	 if (needle == string::npos)
	    return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "[option] not assignment");

	 string const key = string(option, 0, needle);
	 string const value = string(option, needle + 1, option.length());

	 if (key.empty() == true)
	    return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "[option] no key");

	 if (value.empty() == true)
	    return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "[option] no value");

	 Options[key] = value;
      }
      ++Buffer; // ignore the ]
      for (;Buffer != 0 && isspace(*Buffer); ++Buffer); // Skip whitespaces
   }

   string URI;
   string Dist;
   string Section;

   if (ParseQuoteWord(Buffer,URI) == false)
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "URI");
   if (ParseQuoteWord(Buffer,Dist) == false)
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "Suite");

   if (FixupURI(URI) == false)
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "URI parse");

   // Check for an absolute dists specification.
   if (Dist.empty() == false && Dist[Dist.size() - 1] == '/')
   {
      if (ParseQuoteWord(Buffer,Section) == true)
	 return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "absolute Suite Component");
      Dist = SubstVar(Dist,"$(ARCH)",_config->Find("APT::Architecture"));
      return CreateItem(List, URI, Dist, Section, Options);
   }

   // Grab the rest of the dists
   if (ParseQuoteWord(Buffer,Section) == false)
      return _error->Error(_("Malformed entry %u in %s file %s (%s)"), CurLine, "list", File.c_str(), "Component");

   do
   {
      if (CreateItem(List, URI, Dist, Section, Options) == false)
	 return false;
   }
   while (ParseQuoteWord(Buffer,Section) == true);

   return true;
}
									/*}}}*/
// SourceList::pkgSourceList - Constructors				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::pkgSourceList() : d(NULL)
{
}
									/*}}}*/
// SourceList::~pkgSourceList - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::~pkgSourceList()
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); ++I)
      delete *I;
   SrcList.clear();
   for (auto  F = VolatileFiles.begin(); F != VolatileFiles.end(); ++F)
      delete (*F);
   VolatileFiles.clear();
}
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   Reset();
   // CNC:2003-11-28 - Entries in sources.list have priority over
   //                  entries in sources.list.d.
   string Main = _config->FindFile("Dir::Etc::sourcelist", "/dev/null");
   string Parts = _config->FindDir("Dir::Etc::sourceparts", "/dev/null");

   _error->PushToStack();
   if (RealFileExists(Main) == true)
      ReadAppend(Main);
   else if (DirectoryExists(Parts) == false && APT::String::Endswith(Parts, "/dev/null") == false)
      // Only warn if there are no sources.list.d.
      _error->WarningE("DirectoryExists", _("Unable to read %s"), Parts.c_str());

   if (DirectoryExists(Parts) == true)
      ReadSourceDir(Parts);
   else if (Main.empty() == false && RealFileExists(Main) == false &&
	 APT::String::Endswith(Parts, "/dev/null") == false)
      // Only warn if there is no sources.list file.
      _error->WarningE("RealFileExists", _("Unable to read %s"), Main.c_str());

   for (auto && file: _config->FindVector("APT::Sources::With"))
      AddVolatileFile(file, nullptr);

   auto good = _error->PendingError() == false;
   _error->MergeWithStack();
   return good;
}
									/*}}}*/
// SourceList::Reset - Clear the sourcelist contents			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgSourceList::Reset()
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); ++I)
      delete *I;
   SrcList.clear();
}
									/*}}}*/
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string const &File)
{
   Reset();
   return ReadAppend(File);
}
									/*}}}*/
// SourceList::ReadAppend - Parse a sourcelist file			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadAppend(string const &File)
{
   if (flExtension(File) == "sources")
      return ParseFileDeb822(File);
   else
      return ParseFileOldStyle(File);
}
									/*}}}*/
// SourceList::ReadFileOldStyle - Read Traditional style sources.list 	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ParseFileOldStyle(std::string const &File)
{
   FileFd Fd;
   if (OpenConfigurationFileFd(File, Fd) == false)
      return false;

   std::string Buffer;
   for (unsigned int CurLine = 1; Fd.ReadLine(Buffer); ++CurLine)
   {
      // remove comments
      size_t curpos = 0;
      while ((curpos = Buffer.find('#', curpos)) != std::string::npos)
      {
	 size_t const openbrackets = std::count(Buffer.begin(), Buffer.begin() + curpos, '[');
	 size_t const closedbrackets = std::count(Buffer.begin(), Buffer.begin() + curpos, ']');
	 if (openbrackets > closedbrackets)
	 {
	    // a # in an option, unlikely, but oh well, it was supported so stick to it
	    ++curpos;
	    continue;
	 }
	 Buffer.erase(curpos);
	 break;
      }
      // remove spaces before/after
      curpos = Buffer.find_first_not_of(" \t\r");
      if (curpos != 0)
	 Buffer.erase(0, curpos);
      curpos = Buffer.find_last_not_of(" \t\r");
      if (curpos != std::string::npos)
	 Buffer.erase(curpos + 1);

      if (Buffer.empty())
	 continue;

      // Grok it
      std::string const LineType = Buffer.substr(0, Buffer.find_first_of(" \t\v"));
      if (LineType.empty() || LineType == Buffer)
	 return _error->Error(_("Malformed line %u in source list %s (type)"),CurLine,File.c_str());

      Type *Parse = Type::GetType(LineType.c_str());
      if (Parse == 0)
	 return _error->Error(_("Type '%s' is not known on line %u in source list %s"),LineType.c_str(),CurLine,File.c_str());

      if (Parse->ParseLine(SrcList, Buffer.c_str() + LineType.length(), CurLine, File) == false)
	 return false;
   }
   return true;
}
									/*}}}*/
// SourceList::ParseFileDeb822 - Parse deb822 style sources.list 	/*{{{*/
// ---------------------------------------------------------------------
/* Returns: the number of stanzas parsed*/
bool pkgSourceList::ParseFileDeb822(string const &File)
{
   // see if we can read the file
   FileFd Fd;
   if (OpenConfigurationFileFd(File, Fd) == false)
      return false;
   pkgTagFile Sources(&Fd, pkgTagFile::SUPPORT_COMMENTS);
   if (Fd.IsOpen() == false || Fd.Failed())
      return _error->Error(_("Malformed stanza %u in source list %s (type)"),0,File.c_str());

   // read step by step
   pkgTagSection Tags;
   unsigned int i = 0;
   while (Sources.Step(Tags) == true)
   {
      ++i;
      if(Tags.Exists("Types") == false)
	 return _error->Error(_("Malformed stanza %u in source list %s (type)"),i,File.c_str());

      for (auto const &type : FindMultiValue(Tags, "Types"))
      {
	 Type *Parse = Type::GetType(type.c_str());
	 if (Parse == 0)
	 {
	    _error->Error(_("Type '%s' is not known on stanza %u in source list %s"), type.c_str(), i, Fd.Name().c_str());
	    return false;
	 }

	 if (!Parse->ParseStanza(SrcList, Tags, i, Fd))
	    return false;
      }
   }
   return true;
}
									/*}}}*/
// SourceList::FindIndex - Get the index associated with a file		/*{{{*/
static bool FindInIndexFileContainer(std::vector<pkgIndexFile *> const &Cont, pkgCache::PkgFileIterator const &File, pkgIndexFile *&Found)
{
   auto const J = std::find_if(Cont.begin(), Cont.end(), [&File](pkgIndexFile const * const J) {
	 return J->FindInCache(*File.Cache()) == File;
   });
   if (J != Cont.end())
   {
      Found = (*J);
      return true;
   }
   return false;
}
bool pkgSourceList::FindIndex(pkgCache::PkgFileIterator File,
			      pkgIndexFile *&Found) const
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); ++I)
      if (FindInIndexFileContainer(*(*I)->GetIndexFiles(), File, Found))
	 return true;

   return FindInIndexFileContainer(VolatileFiles, File, Found);
}
									/*}}}*/
// SourceList::GetIndexes - Load the index files into the downloader	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::GetIndexes(pkgAcquire *Owner, bool GetAll) const
{
   for (const_iterator I = SrcList.begin(); I != SrcList.end(); ++I)
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
bool pkgSourceList::ReadSourceDir(string const &Dir)
{
   std::vector<std::string> const ext = {"list", "sources"};
   // Read the files
   bool good = true;
   for (auto const &I : GetListOfFilesInDir(Dir, ext, true))
      good = ReadAppend(I) && good;
   return good;
}
									/*}}}*/
// GetLastModified()						/*{{{*/
// ---------------------------------------------------------------------
/* */
time_t pkgSourceList::GetLastModifiedTime()
{
   vector<string> List;

   string Main = _config->FindFile("Dir::Etc::sourcelist");
   string Parts = _config->FindDir("Dir::Etc::sourceparts");

   // go over the parts
   if (DirectoryExists(Parts) == true)
      List = GetListOfFilesInDir(Parts, "list", true);

   // calculate the time
   std::vector<time_t> modtimes;
   modtimes.reserve(1 + List.size());
   modtimes.push_back(GetModificationTime(Main));
   std::transform(List.begin(), List.end(), std::back_inserter(modtimes), GetModificationTime);
   auto const maxmtime = std::max_element(modtimes.begin(), modtimes.end());
   return *maxmtime;
}
									/*}}}*/
std::vector<pkgIndexFile*> pkgSourceList::GetVolatileFiles() const	/*{{{*/
{
   return VolatileFiles;
}
									/*}}}*/
void pkgSourceList::AddVolatileFile(pkgIndexFile * const File)		/*{{{*/
{
   if (File != nullptr)
      VolatileFiles.push_back(File);
}
									/*}}}*/
static bool fileNameMatches(std::string const &filename, std::string const &idxtype)/*{{{*/
{
   for (auto && type: APT::Configuration::getCompressionTypes())
   {
      if (type == "uncompressed")
      {
	 if (filename == idxtype || APT::String::Endswith(filename, '_' + idxtype))
	    return true;
      }
      else if (filename == idxtype + '.' + type ||
	    APT::String::Endswith(filename, '_' + idxtype + '.' + type))
	 return true;
   }
   return false;
}
									/*}}}*/
bool pkgSourceList::AddVolatileFile(std::string const &File, std::vector<std::string> * const VolatileCmdL)/*{{{*/
{
   // Note: FileExists matches directories and links, too!
   if (File.empty() || FileExists(File) == false)
      return false;

   std::string const ext = flExtension(File);
   // udeb is not included as installing it is usually a mistake rather than intended
   if (ext == "deb" || ext == "ddeb")
      AddVolatileFile(new debDebPkgFileIndex(File));
   else if (ext == "dsc")
      AddVolatileFile(new debDscFileIndex(File));
   else if (FileExists(flCombine(File, "debian/control")))
      AddVolatileFile(new debDscFileIndex(flCombine(File, "debian/control")));
   else if (ext == "changes")
   {
      debDscRecordParser changes(File, nullptr);
      std::vector<pkgSrcRecords::File> fileslst;
      if (changes.Files(fileslst) == false || fileslst.empty())
	 return false;
      auto const basedir = flNotFile(File);
      for (auto && file: fileslst)
      {
	 auto const name = flCombine(basedir, file.Path);
	 AddVolatileFile(name, VolatileCmdL);
	 if (file.Hashes.VerifyFile(name) == false)
	    return _error->Error("The file %s does not match with the hashes in the %s file!", name.c_str(), File.c_str());
      }
      return true;
   }
   else
   {
      auto const filename = flNotDir(File);
      auto const Target = IndexTarget(File, filename, File, "file:" + File, false, true, {
	 { "FILENAME", File },
	 { "REPO_URI", "file:" + flAbsPath(flNotFile(File)) + '/' },
	 { "COMPONENT", "volatile-packages-file" },
      });
      if (fileNameMatches(filename, "Packages"))
	 AddVolatileFile(new debPackagesIndex(Target, true));
      else if (fileNameMatches(filename, "Sources"))
	 AddVolatileFile(new debSourcesIndex(Target, true));
      else
	 return false;
   }

   if (VolatileCmdL != nullptr)
      VolatileCmdL->push_back(File);
   return true;
}
bool pkgSourceList::AddVolatileFile(std::string const &File)
{
   return AddVolatileFile(File, nullptr);
}
									/*}}}*/
void pkgSourceList::AddVolatileFiles(CommandLine &CmdL, std::vector<std::string> * const VolatileCmdL)/*{{{*/
{
   std::remove_if(CmdL.FileList + 1, CmdL.FileList + 1 + CmdL.FileSize(), [&](char const * const I) {
      if (I != nullptr && (I[0] == '/' || (I[0] == '.' && (I[1] == '\0' || (I[1] == '.' && (I[2] == '\0' || I[2] == '/')) || I[1] == '/'))))
      {
	 if (AddVolatileFile(I, VolatileCmdL))
	    ;
	 else
	    _error->Error(_("Unsupported file %s given on commandline"), I);
	 return true;
      }
      return false;
   });
}
									/*}}}*/
