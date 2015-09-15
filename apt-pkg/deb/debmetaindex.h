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
class IndexTarget;
class pkgCacheGenerator;
class OpProgress;
class debReleaseIndexPrivate;

class APT_HIDDEN debReleaseIndex : public metaIndex
{
   debReleaseIndexPrivate * const d;

   APT_HIDDEN bool parseSumData(const char *&Start, const char *End, std::string &Name,
		     std::string &Hash, unsigned long long &Size);
   public:

   APT_HIDDEN std::string MetaIndexInfo(const char *Type) const;
   APT_HIDDEN std::string MetaIndexFile(const char *Types) const;
   APT_HIDDEN std::string MetaIndexURI(const char *Type) const;

   debReleaseIndex(std::string const &URI, std::string const &Dist);
   debReleaseIndex(std::string const &URI, std::string const &Dist, bool const Trusted);
   virtual ~debReleaseIndex();

   virtual std::string ArchiveURI(std::string const &File) const APT_OVERRIDE {return URI + File;};
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) APT_OVERRIDE;
   virtual std::vector<IndexTarget> GetIndexTargets() const APT_OVERRIDE;

   virtual std::string Describe() const APT_OVERRIDE;
   virtual pkgCache::RlsFileIterator FindInCache(pkgCache &Cache, bool const ModifyCheck) const APT_OVERRIDE;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const APT_OVERRIDE;

   virtual bool Load(std::string const &Filename, std::string * const ErrorText) APT_OVERRIDE;
   virtual metaIndex * UnloadedClone() const APT_OVERRIDE;

   virtual std::vector <pkgIndexFile *> *GetIndexFiles() APT_OVERRIDE;

   bool SetTrusted(TriState const Trusted);
   bool SetCheckValidUntil(TriState const Trusted);
   bool SetValidUntilMin(time_t const Valid);
   bool SetValidUntilMax(time_t const Valid);
   bool SetSignedBy(std::string const &SignedBy);

   virtual bool IsTrusted() const APT_OVERRIDE;

   void AddComponent(std::string const &sourcesEntry,
	 bool const isSrc, std::string const &Name,
	 std::vector<std::string> const &Targets,
	 std::vector<std::string> const &Architectures,
	 std::vector<std::string> Languages,
	 bool const usePDiffs, std::string const &useByHash);
};

#endif
