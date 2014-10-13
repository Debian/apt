// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.cc,v 1.10 2001/03/13 06:51:46 jgg Exp $
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/tagfile.h>

#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include <langinfo.h>
									/*}}}*/

using std::string;

// RecordParser::debRecordParser - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debRecordParser::debRecordParser(string FileName,pkgCache &Cache) : 
                  File(FileName,FileFd::ReadOnly, FileFd::Extension),
                  Tags(&File, std::max(Cache.Head().MaxVerFileSize, 
				       Cache.Head().MaxDescFileSize) + 200)
{
}
									/*}}}*/
// RecordParser::Jump - Jump to a specific record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debRecordParser::Jump(pkgCache::VerFileIterator const &Ver)
{
   return Tags.Jump(Section,Ver->Offset);
}
bool debRecordParser::Jump(pkgCache::DescFileIterator const &Desc)
{
   return Tags.Jump(Section,Desc->Offset);
}
									/*}}}*/
// RecordParser::FileName - Return the archive filename on the site	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::FileName()
{
   return Section.FindS("Filename");
}
									/*}}}*/
// RecordParser::Name - Return the package name				/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::Name()
{
   return Section.FindS("Package");
}
									/*}}}*/
// RecordParser::Homepage - Return the package homepage		       	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::Homepage()
{
   return Section.FindS("Homepage");
}
									/*}}}*/
// RecordParser::Hashes - return the available archive hashes		/*{{{*/
HashStringList debRecordParser::Hashes() const
{
   HashStringList hashes;
   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      std::string const hash = Section.FindS(*type);
      if (hash.empty() == false)
	 hashes.push_back(HashString(*type, hash));
   }
   return hashes;
}
									/*}}}*/
// RecordParser::Maintainer - Return the maintainer email		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::Maintainer()
{
   return Section.FindS("Maintainer");
}
									/*}}}*/
// RecordParser::RecordField - Return the value of an arbitrary field       /*{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::RecordField(const char *fieldName)
{
   return Section.FindS(fieldName);
}

                                                                        /*}}}*/
// RecordParser::ShortDesc - Return a 1 line description		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::ShortDesc(std::string const &lang)
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
// RecordParser::LongDesc - Return a longer description			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::LongDesc(std::string const &lang)
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

static const char *SourceVerSeparators = " ()";

// RecordParser::SourcePkg - Return the source package name if any	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::SourcePkg()
{
   string Res = Section.FindS("Source");
   string::size_type Pos = Res.find_first_of(SourceVerSeparators);
   if (Pos == string::npos)
      return Res;
   return string(Res,0,Pos);
}
									/*}}}*/
// RecordParser::SourceVer - Return the source version number if present	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::SourceVer()
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
// RecordParser::GetRec - Return the whole record			/*{{{*/
// ---------------------------------------------------------------------
/* */
void debRecordParser::GetRec(const char *&Start,const char *&Stop)
{
   Section.GetSection(Start,Stop);
}
									/*}}}*/

debRecordParser::~debRecordParser() {};
