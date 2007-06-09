#ifndef PKGLIB_METAINDEX_H
#define PKGLIB_METAINDEX_H


#include <string>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/vendor.h>
    
using std::string;

class pkgAcquire;
class pkgCacheGenerator;
class OpProgress;

class metaIndex
{
   protected:
   vector <pkgIndexFile *> *Indexes;
   const char *Type;
   string URI;
   string Dist;
   bool Trusted;

   public:

   
   // Various accessors
   virtual string GetURI() const {return URI;}
   virtual string GetDist() const {return Dist;}
   virtual const char* GetType() const {return Type;}

   // Interface for acquire
   virtual string ArchiveURI(string /*File*/) const = 0;
   virtual bool GetIndexes(pkgAcquire *Owner, bool GetAll=false) const = 0;
   
   virtual vector<pkgIndexFile *> *GetIndexFiles() = 0; 
   virtual bool IsTrusted() const = 0;

   virtual ~metaIndex() {};
};

#endif
