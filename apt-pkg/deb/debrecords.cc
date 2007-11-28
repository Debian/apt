// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.cc,v 1.10 2001/03/13 06:51:46 jgg Exp $
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/debrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <langinfo.h>
									/*}}}*/

// RecordParser::debRecordParser - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debRecordParser::debRecordParser(string FileName,pkgCache &Cache) : 
                  File(FileName,FileFd::ReadOnly), 
                  Tags(&File,Cache.Head().MaxVerFileSize + 200)
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
// RecordParser::MD5Hash - Return the archive hash			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::MD5Hash()
{
   return Section.FindS("MD5Sum");
}
									/*}}}*/
// RecordParser::SHA1Hash - Return the archive hash			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::SHA1Hash()
{
   return Section.FindS("SHA1");
}
									/*}}}*/
// RecordParser::SHA1Hash - Return the archive hash			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::SHA256Hash()
{
   return Section.FindS("SHA256");
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
// RecordParser::ShortDesc - Return a 1 line description		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::ShortDesc()
{
   string Res = LongDesc();
   string::size_type Pos = Res.find('\n');
   if (Pos == string::npos)
      return Res;
   return string(Res,0,Pos);
}
									/*}}}*/
// RecordParser::LongDesc - Return a longer description			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::LongDesc()
{
  string orig, dest;
  char *codeset = nl_langinfo(CODESET);

  if (!Section.FindS("Description").empty())
     orig = Section.FindS("Description").c_str();
  else 
     orig = Section.FindS(("Description-" + pkgIndexFile::LanguageCode()).c_str()).c_str();

  if (strcmp(codeset,"UTF-8") != 0) {
     UTF8ToCodeset(codeset, orig, &dest);
     orig = dest;
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
