// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.cc,v 1.1 1998/08/09 00:51:36 jgg Exp $
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
debRecordParser::debRecordParser(FileFd &File) : Tags(File,4*1024)
{
}
									/*}}}*/
// RecordParser::Jump - Jump to a specific record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debRecordParser::Jump(pkgCache::VerFileIterator &Ver)
{
   return Tags.Jump(Section,Ver->Offset);
}
									/*}}}*/
