#ifndef PKGLIB_DEBMETAINDEX_H
#define PKGLIB_DEBMETAINDEX_H

#include <apt-pkg/metaindex.h>
#include <apt-pkg/macros.h>

#include <map>
#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/sourcelist.h>
#endif
#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/init.h>
#endif

class pkgAcquire;
class pkgIndexFile;
class debDebPkgFileIndex;
class IndexTarget;
class pkgCacheGenerator;
class OpProgress;
class debReleaseIndexPrivate;

class APT_HIDDEN debReleaseIndex : public metaIndex
{
   debReleaseIndexPrivate * const d;

   public:

   APT_HIDDEN std::string MetaIndexInfo(const char *Type) const;
   APT_HIDDEN std::string MetaIndexFile(const char *Types) const;
   APT_HIDDEN std::string MetaIndexURI(const char *Type) const;

   debReleaseIndex(std::string const &URI, std::string const &Dist);
   debReleaseIndex(std::string const &URI, std::string const &Dist, bool const Trusted);
   virtual ~debReleaseIndex();

   virtual std::string ArchiveURI(std::string const &File) const {return URI + File;};
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) const;
   virtual std::vector<IndexTarget> GetIndexTargets() const;

   virtual std::string Describe() const;
   virtual pkgCache::RlsFileIterator FindInCache(pkgCache &Cache, bool const ModifyCheck) const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;

   virtual std::string LocalFileName() const;

   virtual std::vector <pkgIndexFile *> *GetIndexFiles();

   enum APT_HIDDEN TriState {
      TRI_YES, TRI_DONTCARE, TRI_NO, TRI_UNSET
   };
   bool SetTrusted(TriState const Trusted);

   virtual bool IsTrusted() const;

   void AddComponent(bool const isSrc, std::string const &Name,
	 std::vector<std::string> const &Targets,
	 std::vector<std::string> const &Architectures,
	 std::vector<std::string> Languages);
};

class APT_HIDDEN debDebFileMetaIndex : public metaIndex
{
 private:
    void * const d;
   std::string DebFile;
   debDebPkgFileIndex *DebIndex;
 public:
   virtual std::string ArchiveURI(std::string const& /*File*/) const {
      return DebFile;
   }
   virtual bool GetIndexes(pkgAcquire* /*Owner*/, const bool& /*GetAll=false*/) const {
      return true;
   }
   virtual std::vector<IndexTarget> GetIndexTargets() const {
      return std::vector<IndexTarget>();
   }
   virtual std::vector<pkgIndexFile *> *GetIndexFiles() {
      return Indexes;
   }
   virtual bool IsTrusted() const {
      return true;
   }
   debDebFileMetaIndex(std::string const &DebFile);
   virtual ~debDebFileMetaIndex();

};

#endif
