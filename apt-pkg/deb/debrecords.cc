// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.cc,v 1.6 1999/03/29 19:28:52 jgg Exp $
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/debrecords.h"
#endif
#include <apt-pkg/debrecords.h>
#include <apt-pkg/error.h>
									/*}}}*/

// RecordParser::debRecordParser - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debRecordParser::debRecordParser(FileFd &File,pkgCache &Cache) : 
                   Tags(File,Cache.Head().MaxVerFileSize + 20)
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
									/*}}}*/
// RecordParser::FileName - Return the archive filename on the site	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::FileName()
{
   return Section.FindS("Filename");
}
									/*}}}*/
// RecordParser::MD5Hash - Return the archive hash			/*{{{*/
// ---------------------------------------------------------------------
/* */
string debRecordParser::MD5Hash()
{
   return Section.FindS("MD5sum");
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
   string Res = Section.FindS("Description");
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
   return Section.FindS("Description");
}
									/*}}}*/
