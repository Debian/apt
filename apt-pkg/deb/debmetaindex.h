#ifndef PKGLIB_DEBMETAINDEX_H
#define PKGLIB_DEBMETAINDEX_H

#include <apt-pkg/macros.h>
#include <apt-pkg/metaindex.h>

#include <map>
#include <string>
#include <vector>


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

   debReleaseIndex(std::string const &URI, std::string const &Dist, std::map<std::string,std::string> const &Options);
   debReleaseIndex(std::string const &URI, std::string const &Dist, bool const Trusted, std::map<std::string,std::string> const &Options);
   ~debReleaseIndex() override;

   [[nodiscard]] std::string ArchiveURI(std::string const &File) const override;
   bool GetIndexes(pkgAcquire *Owner, bool const &GetAll = false) override;
   [[nodiscard]] std::vector<IndexTarget> GetIndexTargets() const override;

   [[nodiscard]] std::string Describe() const override;
   pkgCache::RlsFileIterator FindInCache(pkgCache &Cache, bool ModifyCheck) const override;
   bool Merge(pkgCacheGenerator &Gen, OpProgress *Prog) const override;

   bool Load(std::string const &Filename, std::string *ErrorText) override;
   [[nodiscard]] metaIndex *UnloadedClone() const override;

   std::vector<pkgIndexFile *> *GetIndexFiles() override;

   bool SetTrusted(TriState const Trusted);
   bool SetCheckValidUntil(TriState const Trusted);
   bool SetValidUntilMin(time_t const Valid);
   bool SetValidUntilMax(time_t const Valid);
   bool SetCheckDate(TriState const CheckDate);
   bool SetDateMaxFuture(time_t const DateMaxFuture);
   bool SetSnapshot(std::string Snapshot);
   std::string GetSnapshotsServer() const; // As defined in the Release file
   bool SetSignedBy(std::string const &SignedBy);
   std::map<std::string, std::string> GetReleaseOptions();

   [[nodiscard]] bool IsTrusted() const override;
   [[nodiscard]] bool IsArchitectureSupported(std::string const &arch) const override;
   [[nodiscard]] bool IsArchitectureAllSupportedFor(IndexTarget const &target) const override;
   [[nodiscard]] bool HasSupportForComponent(std::string const &component) const override;

   [[nodiscard]] time_t GetNotBefore() const override APT_PURE;

   void AddComponent(std::string const &sourcesEntry,
	 bool const isSrc, std::string const &Name,
	 std::vector<std::string> const &Targets,
	 std::vector<std::string> const &Architectures,
	 std::vector<std::string> Languages,
	 bool const usePDiffs, std::string const &useByHash);
};

#endif
