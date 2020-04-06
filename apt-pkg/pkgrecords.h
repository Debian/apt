// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Package Records - Allows access to complete package description records
                     directly from the file.
   
   The package record system abstracts the actual parsing of the 
   package files. This is different than the generators parser in that
   it is used to access information not generate information. No 
   information touched by the generator should be parable from here as
   it can always be retrieved directly from the cache.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PKGRECORDS_H
#define PKGLIB_PKGRECORDS_H

#include <apt-pkg/hashes.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <string>
#include <vector>

class APT_PUBLIC pkgRecords							/*{{{*/
{
   public:
   class Parser;
   
   private:
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;
   
   pkgCache &Cache;
   std::vector<Parser *>Files;

    public:
   // Lookup function
   Parser &Lookup(pkgCache::VerFileIterator const &Ver);
   Parser &Lookup(pkgCache::DescFileIterator const &Desc);

   // Construct destruct
   explicit pkgRecords(pkgCache &Cache);
   virtual ~pkgRecords();
};
									/*}}}*/
class APT_PUBLIC pkgRecords::Parser						/*{{{*/
{
   protected:
   
   virtual bool Jump(pkgCache::VerFileIterator const &Ver) = 0;
   virtual bool Jump(pkgCache::DescFileIterator const &Desc) = 0;
   
   public:
   friend class pkgRecords;
   
   // These refer to the archive file for the Version
   virtual std::string FileName() {return std::string();};
   virtual std::string SourcePkg() {return std::string();};
   virtual std::string SourceVer() {return std::string();};

   /** return all known hashes in this record.
    *
    * For authentication proposes packages come with hashsums which
    * this method is supposed to parse and return so that clients can
    * choose the hash to be used.
    */
   virtual HashStringList Hashes() const { return HashStringList(); };

   // These are some general stats about the package
   virtual std::string Maintainer() {return std::string();};
   /** return short description in language from record.
    *
    * @see #LongDesc
    */
   virtual std::string ShortDesc(std::string const &/*lang*/) {return std::string();};
   /** return long description in language from record.
    *
    * If \b lang is empty the "best" available language will be
    * returned as determined by the APT::Languages configuration.
    * If a (requested) language can't be found in this record an empty
    * string will be returned.
    */
   virtual std::string LongDesc(std::string const &/*lang*/) {return std::string();};
   std::string ShortDesc() {return ShortDesc("");};
   std::string LongDesc() {return LongDesc("");};

   virtual std::string Name() {return std::string();};
   virtual std::string Homepage() {return std::string();}

   // An arbitrary custom field
   virtual std::string RecordField(const char * /*fieldName*/) { return std::string();};

   // The record in binary form
   virtual void GetRec(const char *&Start,const char *&Stop) {Start = Stop = 0;};

   Parser();
   virtual ~Parser();

   private:
   void * const d;
   APT_HIDDEN std::string GetHashFromHashes(char const * const type) const
   {
      HashStringList const hashes = Hashes();
      HashString const * const hs = hashes.find(type);
      return hs != NULL ? hs->HashValue() : "";
   };
};
									/*}}}*/
#endif
