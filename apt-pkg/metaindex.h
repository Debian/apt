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
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   // returns the path of the local file (or "" if its not available)
   virtual std::string LocalFileName() const {return "";};
#endif

   // Interface for acquire
   virtual std::string ArchiveURI(std::string const& File) const = 0;
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) const = 0;
   virtual std::vector<pkgIndexFile *> *GetIndexFiles() = 0; 
   virtual bool IsTrusted() const = 0;

   metaIndex(std::string const &URI, std::string const &Dist, 
             char const * const Type) 
      : Indexes(NULL), Type(Type), URI(URI), Dist(Dist)
   {
      /* nothing */
   }

   virtual ~metaIndex() 
   {
      if (Indexes == 0)
	 return;
      for (std::vector<pkgIndexFile *>::iterator I = (*Indexes).begin();
           I != (*Indexes).end(); ++I)
	 delete *I;
      delete Indexes;
   }
};

#endif
