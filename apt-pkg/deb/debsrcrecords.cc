// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile-keys.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
									/*}}}*/

using std::max;
using std::string;

debSrcRecordParser::debSrcRecordParser(std::string const &File,pkgIndexFile const *Index)
   : Parser(Index), d(NULL), Tags(&Fd), iOffset(0), Buffer(NULL)
{
   if (File.empty() == false)
   {
      if (Fd.Open(File, FileFd::ReadOnly, FileFd::Extension))
	 Tags.Init(&Fd, 102400);
   }
}
std::string debSrcRecordParser::Package() const				/*{{{*/
{
   auto const name = Sect.Find(pkgTagSection::Key::Package);
   if (iIndex != nullptr || name.empty() == false)
      return name.to_string();
   return Sect.Find(pkgTagSection::Key::Source).to_string();
}
									/*}}}*/
// SrcRecordParser::Binaries - Return the binaries field		/*{{{*/
// ---------------------------------------------------------------------
/* This member parses the binaries field into a pair of class arrays and
   returns a list of strings representing all of the components of the
   binaries field. The returned array need not be freed and will be
   reused by the next Binaries function call. This function is commonly
   used during scanning to find the right package */
const char **debSrcRecordParser::Binaries()
{
   const char *Start, *End;
   if (Sect.Find(pkgTagSection::Key::Binary, Start, End) == false)
      return NULL;
   for (; isspace_ascii(*Start) != 0; ++Start);
   if (Start >= End)
      return NULL;

   StaticBinList.clear();
   free(Buffer);
   Buffer = strndup(Start, End - Start);

   char* bin = Buffer;
   do {
      char* binStartNext = strchrnul(bin, ',');
      // Found a comma, clean up any space before it
      if (binStartNext > Buffer) {
	 char* binEnd = binStartNext - 1;
	 for (; binEnd > Buffer && isspace_ascii(*binEnd) != 0; --binEnd)
	    *binEnd = 0;
      }
      StaticBinList.push_back(bin);
      if (*binStartNext != ',')
	 break;
      *binStartNext = '\0';
      for (bin = binStartNext + 1; isspace_ascii(*bin) != 0; ++bin)
         ;
   } while (*bin != '\0');
   StaticBinList.push_back(NULL);

   return &StaticBinList[0];
}
									/*}}}*/
// SrcRecordParser::BuildDepends - Return the Build-Depends information	/*{{{*/
// ---------------------------------------------------------------------
/* This member parses the build-depends information and returns a list of 
   package/version records representing the build dependency. The returned 
   array need not be freed and will be reused by the next call to this 
   function */
bool debSrcRecordParser::BuildDepends(std::vector<pkgSrcRecords::Parser::BuildDepRec> &BuildDeps,
					bool const &ArchOnly, bool const &StripMultiArch)
{
   BuildDeps.clear();

   pkgTagSection::Key const fields[] = {
      pkgTagSection::Key::Build_Depends,
      pkgTagSection::Key::Build_Depends_Indep,
      pkgTagSection::Key::Build_Conflicts,
      pkgTagSection::Key::Build_Conflicts_Indep,
      pkgTagSection::Key::Build_Depends_Arch,
      pkgTagSection::Key::Build_Conflicts_Arch,
   };
   for (unsigned short I = 0; I < sizeof(fields) / sizeof(fields[0]); ++I)
   {
      if (ArchOnly && (fields[I] == pkgTagSection::Key::Build_Depends_Indep || fields[I] == pkgTagSection::Key::Build_Conflicts_Indep))
	 continue;

      const char *Start, *Stop;
      if (Sect.Find(fields[I], Start, Stop) == false)
         continue;

      if (Start == Stop)
	 continue;

      while (1)
      {
	 // Strip off leading spaces (is done by ParseDepends, too) and
	 // superfluous commas (encountered in user-written dsc/control files)
	 do {
	    for (;Start != Stop && isspace_ascii(*Start) != 0; ++Start);
	 } while (*Start == ',' && ++Start != Stop);
	 if (Start == Stop)
	    break;

	 BuildDepRec rec;
	 Start = debListParser::ParseDepends(Start, Stop,
					     rec.Package, rec.Version, rec.Op, true, StripMultiArch, true);

	 if (Start == 0)
	    return _error->Error("Problem parsing dependency: %s", BuildDepType(I));
	 rec.Type = I;

	 // We parsed a package that was ignored (wrong architecture restriction
	 // or something).
	 if (rec.Package.empty())
	 {
	    // If this was the last or-group member, close the or-group with the previous entry
	    if (not BuildDeps.empty() && (BuildDeps.back().Op & pkgCache::Dep::Or) == pkgCache::Dep::Or && (rec.Op & pkgCache::Dep::Or) != pkgCache::Dep::Or)
	       BuildDeps.back().Op &= ~pkgCache::Dep::Or;
	 } else {
	    BuildDeps.emplace_back(std::move(rec));
	 }
      }
   }

   return true;
}
									/*}}}*/
// SrcRecordParser::Files - Return a list of files for this source	/*{{{*/
// ---------------------------------------------------------------------
/* This parses the list of files and returns it, each file is required to have
   a complete source package */
bool debSrcRecordParser::Files(std::vector<pkgSrcRecords::File> &List)
{
   List.clear();

   // Stash the / terminated directory prefix
   std::string Base = Sect.Find(pkgTagSection::Key::Directory).to_string();
   if (Base.empty() == false && Base[Base.length()-1] != '/')
      Base += '/';

   std::vector<std::string> const compExts = APT::Configuration::getCompressorExtensions();

   auto const &posix = std::locale::classic();
   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      // derive field from checksum type
      std::string checksumField("Checksums-");
      if (strcmp(*type, "MD5Sum") == 0)
	 checksumField = "Files"; // historic name for MD5 checksums
      else
	 checksumField.append(*type);

      string const Files = Sect.FindS(checksumField.c_str());
      if (Files.empty() == true)
	 continue;
      std::istringstream ss(Files);
      ss.imbue(posix);

      while (ss.good())
      {
	 std::string hash, path;
	 unsigned long long size;
	 if (iIndex == nullptr && checksumField == "Files")
	 {
	    std::string ignore;
	    ss >> hash >> size >> ignore >> ignore >> path;
	 }
	 else
	    ss >> hash >> size >> path;

	 if (ss.fail() || hash.empty() || path.empty())
	    return _error->Error("Error parsing file record in %s of source package %s", checksumField.c_str(), Package().c_str());

	 HashString const hashString(*type, hash);
	 if (Base.empty() == false)
	    path = Base + path;

	 // look if we have a record for this file already
	 std::vector<pkgSrcRecords::File>::iterator file = List.begin();
	 for (; file != List.end(); ++file)
	    if (file->Path == path)
	       break;

	 // we have it already, store the new hash and be done
	 if (file != List.end())
	 {
	    // an error here indicates that we have two different hashes for the same file
	    if (file->Hashes.push_back(hashString) == false)
	       return _error->Error("Error parsing checksum in %s of source package %s", checksumField.c_str(), Package().c_str());
	    continue;
	 }

	 // we haven't seen this file yet
	 pkgSrcRecords::File F;
	 F.Path = path;
	 F.FileSize = size;
	 F.Hashes.push_back(hashString);
	 F.Hashes.FileSize(F.FileSize);

	 // Try to guess what sort of file it is we are getting.
	 string::size_type Pos = F.Path.length()-1;
	 while (1)
	 {
	    string::size_type Tmp = F.Path.rfind('.',Pos);
	    if (Tmp == string::npos)
	       break;
	    if (F.Type == "tar") {
	       // source v3 has extension 'debian.tar.*' instead of 'diff.*'
	       if (string(F.Path, Tmp+1, Pos-Tmp) == "debian")
		  F.Type = "diff";
	       break;
	    }
	    F.Type = string(F.Path,Tmp+1,Pos-Tmp);

	    if (std::find(compExts.begin(), compExts.end(), std::string(".").append(F.Type)) != compExts.end() ||
		  F.Type == "tar")
	    {
	       Pos = Tmp-1;
	       continue;
	    }

	    break;
	 }
	 List.push_back(F);
      }
   }

   return true;
}
									/*}}}*/
// SrcRecordParser::~SrcRecordParser - Destructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
debSrcRecordParser::~debSrcRecordParser()
{
   // was allocated via strndup()
   free(Buffer);
}
									/*}}}*/


debDscRecordParser::debDscRecordParser(std::string const &DscFile, pkgIndexFile const *Index)
   : debSrcRecordParser("", Index)
{
   // support clear signed files
   if (OpenMaybeClearSignedFile(DscFile, Fd) == false)
   {
      _error->Error("Failed to open %s", DscFile.c_str());
      return;
   }

   // re-init to ensure the updated Fd is used
   Tags.Init(&Fd, pkgTagFile::SUPPORT_COMMENTS);
   // read the first (and only) record
   Step();

}
