// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/debrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <langinfo.h>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

// RecordParser::debRecordParser - Constructor				/*{{{*/
debRecordParser::debRecordParser(string FileName,pkgCache &Cache) :
   debRecordParserBase(), d(NULL), File(FileName, FileFd::ReadOnly, FileFd::Extension),
   Tags(&File, std::max(Cache.Head().MaxVerFileSize, Cache.Head().MaxDescFileSize) + 200)
{
}
									/*}}}*/
// RecordParser::Jump - Jump to a specific record			/*{{{*/
bool debRecordParser::Jump(pkgCache::VerFileIterator const &Ver)
{
   if (Ver.end() == true)
      return false;
   return Tags.Jump(Section,Ver->Offset);
}
bool debRecordParser::Jump(pkgCache::DescFileIterator const &Desc)
{
   if (Desc.end() == true)
      return false;
   return Tags.Jump(Section,Desc->Offset);
}
									/*}}}*/
debRecordParser::~debRecordParser() {}

debRecordParserBase::debRecordParserBase() : Parser(), d(NULL) {}
// RecordParserBase::FileName - Return the archive filename on the site	/*{{{*/
string debRecordParserBase::FileName()
{
   return Section.FindS("Filename");
}
									/*}}}*/
// RecordParserBase::Name - Return the package name			/*{{{*/
string debRecordParserBase::Name()
{
   string Result = Section.FindS("Package");

   // Normalize mixed case package names to lower case, like dpkg does
   // See Bug#807012 for details
   std::transform(Result.begin(), Result.end(), Result.begin(), tolower_ascii);

   return Result;
}
									/*}}}*/
// RecordParserBase::Homepage - Return the package homepage		/*{{{*/
string debRecordParserBase::Homepage()
{
   return Section.FindS("Homepage");
}
									/*}}}*/
// RecordParserBase::Hashes - return the available archive hashes	/*{{{*/
HashStringList debRecordParserBase::Hashes() const
{
   HashStringList hashes;
   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      std::string const hash = Section.FindS(*type);
      if (hash.empty() == false)
	 hashes.push_back(HashString(*type, hash));
   }
   auto const size = Section.FindULL("Size", 0);
   if (size != 0)
      hashes.FileSize(size);
   return hashes;
}
									/*}}}*/
// RecordParserBase::Maintainer - Return the maintainer email		/*{{{*/
string debRecordParserBase::Maintainer()
{
   return Section.FindS("Maintainer");
}
									/*}}}*/
// RecordParserBase::RecordField - Return the value of an arbitrary field	/*{{*/
string debRecordParserBase::RecordField(const char *fieldName)
{
   return Section.FindS(fieldName);
}
									/*}}}*/
// RecordParserBase::ShortDesc - Return a 1 line description		/*{{{*/
string debRecordParserBase::ShortDesc(std::string const &lang)
{
   string const Res = LongDesc(lang);
   if (Res.empty() == true)
      return "";
   string::size_type const Pos = Res.find('\n');
   if (Pos == string::npos)
      return Res;
   return string(Res,0,Pos);
}
									/*}}}*/
// RecordParserBase::LongDesc - Return a longer description		/*{{{*/
string debRecordParserBase::LongDesc(std::string const &lang)
{
   string orig;
   if (lang.empty() == true)
   {
      std::vector<string> const lang = APT::Configuration::getLanguages();
      for (std::vector<string>::const_iterator l = lang.begin();
	    l != lang.end(); ++l)
      {
	 std::string const tagname = "Description-" + *l;
	 orig = Section.FindS(tagname.c_str());
	 if (orig.empty() == false)
	    break;
	 else if (*l == "en")
	 {
	    orig = Section.FindS("Description");
	    if (orig.empty() == false)
	       break;
	 }
      }
      if (orig.empty() == true)
	 orig = Section.FindS("Description");
   }
   else
   {
      std::string const tagname = "Description-" + lang;
      orig = Section.FindS(tagname.c_str());
      if (orig.empty() == true && lang == "en")
	 orig = Section.FindS("Description");
   }

   char const * const codeset = nl_langinfo(CODESET);
   if (strcmp(codeset,"UTF-8") != 0) {
      string dest;
      UTF8ToCodeset(codeset, orig, &dest);
      return dest;
   }

   return orig;
}
									/*}}}*/

static const char * const SourceVerSeparators = " ()";
// RecordParserBase::SourcePkg - Return the source package name if any	/*{{{*/
string debRecordParserBase::SourcePkg()
{
   string Res = Section.FindS("Source");
   string::size_type Pos = Res.find_first_of(SourceVerSeparators);
   if (Pos == string::npos)
      return Res;
   return string(Res,0,Pos);
}
									/*}}}*/
// RecordParserBase::SourceVer - Return the source version number if present	/*{{{*/
string debRecordParserBase::SourceVer()
{
   string Pkg = Section.FindS("Source");
   string::size_type Pos = Pkg.find_first_of(SourceVerSeparators);
   if (Pos == string::npos)
      return "";

   string::size_type VerStart = Pkg.find_first_not_of(SourceVerSeparators, Pos);
   if(VerStart == string::npos)
     return "";

   string::size_type VerEnd = Pkg.find_first_of(SourceVerSeparators, VerStart);
   if(VerEnd == string::npos)
     // Corresponds to the case of, e.g., "foo (1.2" without a closing
     // paren.  Be liberal and guess what it means.
     return string(Pkg, VerStart);
   else
     return string(Pkg, VerStart, VerEnd - VerStart);
}
									/*}}}*/
// RecordParserBase::GetRec - Return the whole record			/*{{{*/
void debRecordParserBase::GetRec(const char *&Start,const char *&Stop)
{
   Section.GetSection(Start,Stop);
}
									/*}}}*/
debRecordParserBase::~debRecordParserBase() {}

bool debDebFileRecordParser::LoadContent()
{
   // load content only once
   if (controlContent.empty() == false)
      return true;

   std::ostringstream content;
   if (debDebPkgFileIndex::GetContent(content, debFileName) == false)
      return false;
   // add two newlines to make sure the scanner finds the section,
   // which is usually done by pkgTagFile automatically if needed.
   content << "\n\n";

   controlContent = content.str();
   if (Section.Scan(controlContent.c_str(), controlContent.length()) == false)
      return _error->Error(_("Unable to parse package file %s (%d)"), debFileName.c_str(), 3);
   return true;
}
bool debDebFileRecordParser::Jump(pkgCache::VerFileIterator const &) { return LoadContent(); }
bool debDebFileRecordParser::Jump(pkgCache::DescFileIterator const &) { return LoadContent(); }
std::string debDebFileRecordParser::FileName() { return debFileName; }

debDebFileRecordParser::debDebFileRecordParser(std::string FileName) : debRecordParserBase(), d(NULL), debFileName(FileName) {}
debDebFileRecordParser::~debDebFileRecordParser() {}
