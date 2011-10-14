// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgrecords.h,v 1.6 2001/03/13 06:51:46 jgg Exp $
/* ######################################################################
   
   Package Records - Allows access to complete package description records
                     directly from the file.
   
   The package record system abstracts the actual parsing of the 
   package files. This is different than the generators parser in that
   it is used to access information not generate information. No 
   information touched by the generator should be parable from here as
   it can always be retreived directly from the cache.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PKGRECORDS_H
#define PKGLIB_PKGRECORDS_H


#include <apt-pkg/pkgcache.h>
#include <vector>

class pkgRecords							/*{{{*/
{
   public:
   class Parser;
   
   private:
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;
   
   pkgCache &Cache;
   std::vector<Parser *>Files;

    public:
   // Lookup function
   Parser &Lookup(pkgCache::VerFileIterator const &Ver);
   Parser &Lookup(pkgCache::DescFileIterator const &Desc);

   // Construct destruct
   pkgRecords(pkgCache &Cache);
   ~pkgRecords();
};
									/*}}}*/
class pkgRecords::Parser						/*{{{*/
{
   protected:
   
   virtual bool Jump(pkgCache::VerFileIterator const &Ver) = 0;
   virtual bool Jump(pkgCache::DescFileIterator const &Desc) = 0;
   
   public:
   friend class pkgRecords;
   
   // These refer to the archive file for the Version
   virtual std::string FileName() {return std::string();};
   virtual std::string MD5Hash() {return std::string();};
   virtual std::string SHA1Hash() {return std::string();};
   virtual std::string SHA256Hash() {return std::string();};
   virtual std::string SHA512Hash() {return std::string();};
   virtual std::string SourcePkg() {return std::string();};
   virtual std::string SourceVer() {return std::string();};

   // These are some general stats about the package
   virtual std::string Maintainer() {return std::string();};
   virtual std::string ShortDesc() {return std::string();};
   virtual std::string LongDesc() {return std::string();};
   virtual std::string Name() {return std::string();};
   virtual std::string Homepage() {return std::string();}

   // An arbitrary custom field
   virtual std::string RecordField(const char *fieldName) { return std::string();};

   // The record in binary form
   virtual void GetRec(const char *&Start,const char *&Stop) {Start = Stop = 0;};
   
   virtual ~Parser() {};
};
									/*}}}*/
#endif
