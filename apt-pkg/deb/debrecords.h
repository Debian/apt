// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.h,v 1.1 1998/08/09 00:51:36 jgg Exp $
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
   
   This provides display-type parsing for the Packages file. This is 
   different than the the list parser which provides cache generation
   services. There should be no overlap between these two.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_DEBRECORDS_H
#define PKGLIB_DEBRECORDS_H

#ifdef __GNUG__
#pragma interface "apt-pkg/debrecords.h"
#endif 

#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/tagfile.h>

class debRecordParser : public pkgRecords::Parser
{
   pkgTagFile Tags;
   pkgTagSection Section;
   
   public:
   
   virtual bool Jump(pkgCache::VerFileIterator &Ver);
   
   debRecordParser(FileFd &File);
};


#endif
