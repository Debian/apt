// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexrecords.h,v 1.1.2.1 2003/12/24 23:09:17 mdz Exp $
									/*}}}*/
#ifndef PKGLIB_INDEXRECORDS_H
#define PKGLIB_INDEXRECORDS_H


#include <apt-pkg/pkgcache.h>
#include <apt-pkg/hashes.h>

#include <map>
#include <vector>
#include <ctime>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#endif

class indexRecords
{
   bool parseSumData(const char *&Start, const char *End, std::string &Name,
		     std::string &Hash, unsigned long long &Size);
   public:
   struct checkSum;
   std::string ErrorText;
   
   protected:
   std::string Dist;
   std::string Suite;
   std::string ExpectedDist;
   time_t ValidUntil;

   std::map<std::string,checkSum *> Entries;

   public:

   indexRecords();
   indexRecords(const std::string ExpectedDist);

   // Lookup function
   virtual const checkSum *Lookup(const std::string MetaKey);
   /** \brief tests if a checksum for this file is available */
   bool Exists(std::string const &MetaKey) const;
   std::vector<std::string> MetaKeys();

   virtual bool Load(std::string Filename);
   std::string GetDist() const;
   time_t GetValidUntil() const;
   virtual bool CheckDist(const std::string MaybeDist) const;
   std::string GetExpectedDist() const;
   virtual ~indexRecords(){};
};

struct indexRecords::checkSum
{
   std::string MetaKeyFilename;
   HashString Hash;
   unsigned long long Size;
};

#endif
