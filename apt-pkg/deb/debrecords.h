// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.h,v 1.2 1998/10/08 04:55:04 jgg Exp $
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

   string FindTag(const char *Tag);

   protected:
   
   virtual bool Jump(pkgCache::VerFileIterator &Ver);
   
   public:

   // These refer to the archive file for the Version
   virtual string FileName();
   virtual string MD5Hash();
   
   // These are some general stats about the package
   virtual string Maintainer();
   virtual string ShortDesc();
   virtual string LongDesc();
   
   debRecordParser(FileFd &File);
};


#endif
