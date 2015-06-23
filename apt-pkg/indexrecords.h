// -*- mode: cpp; mode: fold -*-
#ifndef PKGLIB_INDEXRECORDS_H
#define PKGLIB_INDEXRECORDS_H

#include <apt-pkg/hashes.h>

#include <map>
#include <vector>
#include <ctime>
#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#endif
#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/pkgcache.h>
#endif

class indexRecords
{
   APT_HIDDEN bool parseSumData(const char *&Start, const char *End, std::string &Name,
		     std::string &Hash, unsigned long long &Size);
   public:
   struct checkSum;
   std::string ErrorText;

   private:
   enum APT_HIDDEN { ALWAYS_TRUSTED, NEVER_TRUSTED, CHECK_TRUST } Trusted;
   // dpointer (for later)
   void * d;

   protected:
   std::string Dist;
   std::string Suite;
   std::string ExpectedDist;
   time_t Date;
   time_t ValidUntil;
   bool SupportsAcquireByHash;

   std::map<std::string,checkSum *> Entries;

   public:
#if APT_PKG_ABI >= 413
   indexRecords(const std::string &ExpectedDist = "");
#else
   indexRecords();
   indexRecords(const std::string ExpectedDist);
#endif

   // Lookup function
   virtual checkSum *Lookup(const std::string MetaKey);
   /** \brief tests if a checksum for this file is available */
   bool Exists(std::string const &MetaKey) const;
   std::vector<std::string> MetaKeys();

   virtual bool Load(std::string Filename);
   virtual bool CheckDist(const std::string MaybeDist) const;

   std::string GetDist() const;
   std::string GetSuite() const;
   bool GetSupportsAcquireByHash() const;
   time_t GetValidUntil() const;
   time_t GetDate() const;
   std::string GetExpectedDist() const;

   /** \brief check if source is marked as always trusted */
   bool IsAlwaysTrusted() const;
   /** \brief check if source is marked as never trusted */
   bool IsNeverTrusted() const;

   /** \brief sets an explicit trust value
    *
    * \b true means that the source should always be considered trusted,
    * while \b false marks a source as always untrusted, even if we have
    * a valid signature and everything.
    */
   void SetTrusted(bool const Trusted);

   virtual ~indexRecords();
};

APT_IGNORE_DEPRECATED_PUSH
struct indexRecords::checkSum
{
   std::string MetaKeyFilename;
   HashStringList Hashes;
   unsigned long long Size;

   APT_DEPRECATED HashString Hash;
};
APT_IGNORE_DEPRECATED_POP

#endif
