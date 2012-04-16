// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debrecords.h,v 1.8 2001/03/13 06:51:46 jgg Exp $
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
   
   This provides display-type parsing for the Packages file. This is 
   different than the the list parser which provides cache generation
   services. There should be no overlap between these two.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBRECORDS_H
#define PKGLIB_DEBRECORDS_H

#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#endif

class debRecordParser : public pkgRecords::Parser
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   FileFd File;
   pkgTagFile Tags;
   pkgTagSection Section;
   
   protected:
   
   virtual bool Jump(pkgCache::VerFileIterator const &Ver);
   virtual bool Jump(pkgCache::DescFileIterator const &Desc);
   
   public:

   // These refer to the archive file for the Version
   virtual std::string FileName();
   virtual std::string MD5Hash();
   virtual std::string SHA1Hash();
   virtual std::string SHA256Hash();
   virtual std::string SHA512Hash();
   virtual std::string SourcePkg();
   virtual std::string SourceVer();
   
   // These are some general stats about the package
   virtual std::string Maintainer();
   virtual std::string ShortDesc();
   virtual std::string LongDesc();
   virtual std::string Name();
   virtual std::string Homepage();

   // An arbitrary custom field
   virtual std::string RecordField(const char *fieldName);

   virtual void GetRec(const char *&Start,const char *&Stop);
   
   debRecordParser(std::string FileName,pkgCache &Cache);
   virtual ~debRecordParser() {};
};

#endif
