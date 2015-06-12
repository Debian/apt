#ifndef PKGLIB_METAINDEX_H
#define PKGLIB_METAINDEX_H

#include <apt-pkg/indexfile.h>
#include <apt-pkg/init.h>

#include <stddef.h>

#include <string>
#include <vector>

#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/pkgcache.h>
class pkgCacheGenerator;
class OpProgress;
#endif
#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/vendor.h>
using std::string;
#endif

class pkgAcquire;
class IndexTarget;
class pkgCacheGenerator;
class OpProgress;

class metaIndex
{
   protected:
   std::vector <pkgIndexFile *> *Indexes;
   const char *Type;
   std::string URI;
   std::string Dist;
   bool Trusted;

   public:

   // Various accessors
   virtual std::string GetURI() const {return URI;}
   virtual std::string GetDist() const {return Dist;}
   virtual const char* GetType() const {return Type;}

   // interface to to query it
   /** \return the path of the local file (or "" if its not available) */
   virtual std::string LocalFileName() const;

   // Interface for acquire
   virtual std::string ArchiveURI(std::string const& File) const = 0;
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) const = 0;
   virtual std::vector<IndexTarget> GetIndexTargets() const = 0;
   virtual std::vector<pkgIndexFile *> *GetIndexFiles() = 0;
   virtual bool IsTrusted() const = 0;

   virtual std::string Describe() const;
   virtual pkgCache::RlsFileIterator FindInCache(pkgCache &Cache, bool const ModifyCheck) const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;

   metaIndex(std::string const &URI, std::string const &Dist,
             char const * const Type);
   virtual ~metaIndex();
};

#endif
