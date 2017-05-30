// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.cc,v 1.6 2004/03/17 05:58:54 mdz Exp $
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/deblistparser.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/gpgv.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
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
   auto const name = Sect.FindS("Package");
   if (iIndex == nullptr)
      return name.empty() ? Sect.FindS("Source") : name;
   else
      return name;
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
   if (Sect.Find("Binary", Start, End) == false)
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
   unsigned int I;
   const char *Start, *Stop;
   BuildDepRec rec;
   const char *fields[] = {"Build-Depends",
                           "Build-Depends-Indep",
			   "Build-Conflicts",
			   "Build-Conflicts-Indep",
			   "Build-Depends-Arch",
			   "Build-Conflicts-Arch"};

   BuildDeps.clear();

   for (I = 0; I < 6; I++)
   {
      if (ArchOnly && (I == 1 || I == 3))
         continue;

      if (Sect.Find(fields[I], Start, Stop) == false)
         continue;
      
      while (1)
      {
         Start = debListParser::ParseDepends(Start, Stop, 
		     rec.Package,rec.Version,rec.Op,true,StripMultiArch,true);
	 
         if (Start == 0) 
            return _error->Error("Problem parsing dependency: %s", fields[I]);
	 rec.Type = I;

	 // We parsed a package that was ignored (wrong architecture restriction
	 // or something).
	 if (rec.Package == "") {
	    // If we are in an OR group, we need to set the "Or" flag of the
	    // previous entry to our value.
	    if (BuildDeps.size() > 0 && (BuildDeps[BuildDeps.size() - 1].Op & pkgCache::Dep::Or) == pkgCache::Dep::Or) {
	       BuildDeps[BuildDeps.size() - 1].Op &= ~pkgCache::Dep::Or;
	       BuildDeps[BuildDeps.size() - 1].Op |= (rec.Op & pkgCache::Dep::Or);
	    }
	 } else {
   	    BuildDeps.push_back(rec);
	 }
	 
   	 if (Start == Stop) 
	    break;
      }	 
   }
   
   return true;
}
									/*}}}*/
// SrcRecordParser::Files - Return a list of files for this source	/*{{{*/
// ---------------------------------------------------------------------
/* This parses the list of files and returns it, each file is required to have
   a complete source package */
bool debSrcRecordParser::Files(std::vector<pkgSrcRecords::File> &F)
{
   std::vector<pkgSrcRecords::File2> F2;
   if (Files2(F2) == false)
      return false;
   for (std::vector<pkgSrcRecords::File2>::const_iterator f2 = F2.begin(); f2 != F2.end(); ++f2)
   {
      pkgSrcRecords::File2 f;
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      f.MD5Hash = f2->MD5Hash;
      f.Size = f2->Size;
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
      f.Path = f2->Path;
      f.Type = f2->Type;
      F.push_back(f);
   }
   return true;
}
bool debSrcRecordParser::Files2(std::vector<pkgSrcRecords::File2> &List)
{
   List.clear();

   // Stash the / terminated directory prefix
   string Base = Sect.FindS("Directory");
   if (Base.empty() == false && Base[Base.length()-1] != '/')
      Base += '/';

   std::vector<std::string> const compExts = APT::Configuration::getCompressorExtensions();

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

      // Iterate over the entire list grabbing each triplet
      const char *C = Files.c_str();
      while (*C != 0)
      {
	 string hash, size, path;

	 // Parse each of the elements
	 if (ParseQuoteWord(C, hash) == false ||
	       ParseQuoteWord(C, size) == false ||
	       ParseQuoteWord(C, path) == false)
	    return _error->Error("Error parsing file record in %s of source package %s", checksumField.c_str(), Package().c_str());

	 if (iIndex == nullptr && checksumField == "Files")
	 {
	    // the Files field has a different format than the rest in deb-changes files
	    std::string ignore;
	    if (ParseQuoteWord(C, ignore) == false ||
		  ParseQuoteWord(C, path) == false)
	       return _error->Error("Error parsing file record in %s of source package %s", checksumField.c_str(), Package().c_str());
	 }

	 HashString const hashString(*type, hash);
	 if (Base.empty() == false)
	    path = Base + path;

	 // look if we have a record for this file already
	 std::vector<pkgSrcRecords::File2>::iterator file = List.begin();
	 for (; file != List.end(); ++file)
	    if (file->Path == path)
	       break;

	 // we have it already, store the new hash and be done
	 if (file != List.end())
	 {
	    if (checksumField == "Files")
	       APT_IGNORE_DEPRECATED(file->MD5Hash = hash;)
	    // an error here indicates that we have two different hashes for the same file
	    if (file->Hashes.push_back(hashString) == false)
	       return _error->Error("Error parsing checksum in %s of source package %s", checksumField.c_str(), Package().c_str());
	    continue;
	 }

	 // we haven't seen this file yet
	 pkgSrcRecords::File2 F;
	 F.Path = path;
	 F.FileSize = strtoull(size.c_str(), NULL, 10);
	 F.Hashes.push_back(hashString);
	 F.Hashes.FileSize(F.FileSize);

	 APT_IGNORE_DEPRECATED_PUSH
	 F.Size = F.FileSize;
	 if (checksumField == "Files")
	    F.MD5Hash = hash;
	 APT_IGNORE_DEPRECATED_POP

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
