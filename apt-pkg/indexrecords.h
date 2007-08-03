// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexrecords.h,v 1.1.2.1 2003/12/24 23:09:17 mdz Exp $
									/*}}}*/
#ifndef PKGLIB_INDEXRECORDS_H
#define PKGLIB_INDEXRECORDS_H


#include <apt-pkg/pkgcache.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>

#include <map>
#include <vector>

class indexRecords
{
   bool parseSumData(const char *&Start, const char *End, string &Name,
		     string &Hash, size_t &Size);
   public:
   struct checkSum;
   string ErrorText;
   
   protected:
   string Dist;
   string Suite;
   string ExpectedDist;
   std::map<string,checkSum *> Entries;

   public:

   indexRecords();
   indexRecords(const string ExpectedDist);

   // Lookup function
   virtual const checkSum *Lookup(const string MetaKey);
   std::vector<std::string> MetaKeys();

   virtual bool Load(string Filename);
   string GetDist() const;
   virtual bool CheckDist(const string MaybeDist) const;
   string GetExpectedDist() const;
   virtual ~indexRecords(){};
};

struct indexRecords::checkSum
{
   string MetaKeyFilename;
   HashString Hash;
   size_t Size;      
};

#endif
