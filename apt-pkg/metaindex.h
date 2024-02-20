#ifndef PKGLIB_METAINDEX_H
#define PKGLIB_METAINDEX_H

#include <apt-pkg/indexfile.h>
#include <apt-pkg/init.h>

#include <cstddef>

#include <string>
#include <vector>


class pkgAcquire;
class IndexTarget;
class pkgCacheGenerator;
class OpProgress;

class metaIndexPrivate;

class APT_PUBLIC metaIndex
{
public:
   struct checkSum
   {
      std::string MetaKeyFilename;
      HashStringList Hashes;
      unsigned long long Size;
   };

   enum APT_HIDDEN TriState {
      TRI_YES, TRI_DONTCARE, TRI_NO, TRI_UNSET
   };

   enum class APT_HIDDEN Flag
   {
      DEB822 = 0x01,
   };

   private:
   metaIndexPrivate * const d;
protected:
   std::vector <pkgIndexFile *> *Indexes;
   // parsed from the sources.list
   const char *Type;
   std::string URI;
   std::string Dist;
   TriState Trusted;
   std::string SignedBy;

   // parsed from a file
   std::string Suite;
   std::string Codename;
   std::string Origin;
   std::string Label;
   std::string Version;
   signed short DefaultPin;
   std::string ReleaseNotes;
   time_t Date;
   time_t ValidUntil;
   bool SupportsAcquireByHash;
   std::map<std::string, checkSum *> Entries;
   TriState LoadedSuccessfully;

public:
   // Various accessors
   std::string GetURI() const;
   std::string GetDist() const;
   const char* GetType() const;
   TriState GetTrusted() const;
   std::string GetSignedBy() const;

   std::string GetOrigin() const;
   std::string GetLabel() const;
   std::string GetVersion() const;
   std::string GetCodename() const;
   std::string GetSuite() const;
   std::string GetReleaseNotes() const;
   signed short GetDefaultPin() const;
   bool GetSupportsAcquireByHash() const;
   time_t GetValidUntil() const;
   time_t GetDate() const;
   virtual time_t GetNotBefore() const = 0;
#ifdef APT_COMPILING_APT
   bool HasFlag(Flag flag) const;
#endif
   void SetFlag(Flag flag) APT_HIDDEN;

   std::string GetExpectedDist() const;
   bool CheckDist(std::string const &MaybeDist) const;

   // Interface for acquire
   virtual std::string Describe() const;
   virtual std::string ArchiveURI(std::string const& File) const = 0;
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) = 0;
   virtual std::vector<IndexTarget> GetIndexTargets() const = 0;
   virtual std::vector<pkgIndexFile *> *GetIndexFiles() = 0;
   virtual bool IsTrusted() const = 0;
   virtual bool Load(std::string const &Filename, std::string * const ErrorText) = 0;
   /** @return a new metaIndex object based on this one, but without information from #Load */
   virtual metaIndex * UnloadedClone() const = 0;
   // the given metaIndex is potentially invalid after this call and should be deleted
   void swapLoad(metaIndex * const OldMetaIndex);

   // Lookup functions for parsed Hashes
   checkSum *Lookup(std::string const &MetaKey) const;
   /** \brief tests if a checksum for this file is available */
   bool Exists(std::string const &MetaKey) const;
   std::vector<std::string> MetaKeys() const;
   TriState GetLoadedSuccessfully() const;

   // Interfaces for pkgCacheGen
   virtual pkgCache::RlsFileIterator FindInCache(pkgCache &Cache, bool const ModifyCheck) const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;


   metaIndex(std::string const &URI, std::string const &Dist,
             char const * const Type);
   virtual ~metaIndex();

   virtual bool IsArchitectureSupported(std::string const &arch) const;
   virtual bool IsArchitectureAllSupportedFor(IndexTarget const &target) const;
   virtual bool HasSupportForComponent(std::string const &component) const;
};

#endif
