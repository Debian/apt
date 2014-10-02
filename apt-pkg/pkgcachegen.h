// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.h,v 1.19 2002/07/08 03:13:30 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   Each archive source has it's own list parser that is instantiated by
   the caller to provide data for the generator. 
   
   Parts of the cache are created by this generator class while other
   parts are created by the list parser. The list parser is responsible
   for creating version, depends and provides structures, and some of
   their contents
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PKGCACHEGEN_H
#define PKGLIB_PKGCACHEGEN_H

#include <apt-pkg/md5.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/macros.h>

#include <vector>
#include <string>
#include <map>

class FileFd;
class pkgSourceList;
class OpProgress;
class pkgIndexFile;

class pkgCacheGenerator							/*{{{*/
{
   private:
   APT_HIDDEN map_stringitem_t WriteStringInMap(std::string const &String) { return WriteStringInMap(String.c_str()); };
   APT_HIDDEN map_stringitem_t WriteStringInMap(const char *String);
   APT_HIDDEN map_stringitem_t WriteStringInMap(const char *String, const unsigned long &Len);
   APT_HIDDEN map_pointer_t AllocateInMap(const unsigned long &size);

   std::map<std::string,map_stringitem_t> strMixed;
   std::map<std::string,map_stringitem_t> strSections;
   std::map<std::string,map_stringitem_t> strPkgNames;
   std::map<std::string,map_stringitem_t> strVersions;

   public:
   
   class ListParser;
   friend class ListParser;

   template<typename Iter> class Dynamic {
      public:
      static std::vector<Iter*> toReMap;
      Dynamic(Iter &I) {
	 toReMap.push_back(&I);
      }

      ~Dynamic() {
	 toReMap.pop_back();
      }
   };

   protected:

   DynamicMMap &Map;
   pkgCache Cache;
   OpProgress *Progress;
   
   std::string PkgFileName;
   pkgCache::PackageFile *CurrentFile;

   // Flag file dependencies
   bool FoundFileDeps;
   
   bool NewGroup(pkgCache::GrpIterator &Grp,const std::string &Name);
   bool NewPackage(pkgCache::PkgIterator &Pkg,const std::string &Name, const std::string &Arch);
   bool NewFileVer(pkgCache::VerIterator &Ver,ListParser &List);
   bool NewFileDesc(pkgCache::DescIterator &Desc,ListParser &List);
   bool NewDepends(pkgCache::PkgIterator &Pkg, pkgCache::VerIterator &Ver,
		   std::string const &Version, unsigned int const &Op,
		   unsigned int const &Type, map_pointer_t* &OldDepLast);
   bool NewDepends(pkgCache::PkgIterator &Pkg, pkgCache::VerIterator &Ver,
		   map_pointer_t const Version, unsigned int const &Op,
		   unsigned int const &Type, map_pointer_t* &OldDepLast);
   map_pointer_t NewVersion(pkgCache::VerIterator &Ver,const std::string &VerStr,map_pointer_t const Next) APT_DEPRECATED
   { return NewVersion(Ver, VerStr, 0, 0, Next); }
   map_pointer_t NewVersion(pkgCache::VerIterator &Ver,const std::string &VerStr,
			    map_pointer_t const ParentPkg, unsigned short const Hash,
			    map_pointer_t const Next);
   map_pointer_t NewDescription(pkgCache::DescIterator &Desc,const std::string &Lang,const MD5SumValue &md5sum,map_stringitem_t const idxmd5str);

   public:

   enum StringType { MIXED, PKGNAME, VERSIONNUMBER, SECTION };
   map_stringitem_t StoreString(StringType const type, const char * S, unsigned int const Size);
   inline map_stringitem_t StoreString(enum StringType const type, const std::string &S) {return StoreString(type, S.c_str(),S.length());};

   void DropProgress() {Progress = 0;};
   bool SelectFile(const std::string &File,const std::string &Site,pkgIndexFile const &Index,
		   unsigned long Flags = 0);
   bool MergeList(ListParser &List,pkgCache::VerIterator *Ver = 0);
   inline pkgCache &GetCache() {return Cache;};
   inline pkgCache::PkgFileIterator GetCurFile() 
         {return pkgCache::PkgFileIterator(Cache,CurrentFile);};

   bool HasFileDeps() {return FoundFileDeps;};
   bool MergeFileProvides(ListParser &List);
   bool FinishCache(OpProgress *Progress) APT_DEPRECATED APT_CONST;

   static bool MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap = 0,bool AllowMem = false);
   static bool MakeOnlyStatusCache(OpProgress *Progress,DynamicMMap **OutMap);
   static DynamicMMap* CreateDynamicMMap(FileFd *CacheF, unsigned long Flags = 0);

   void ReMap(void const * const oldMap, void const * const newMap);

   pkgCacheGenerator(DynamicMMap *Map,OpProgress *Progress);
   ~pkgCacheGenerator();

   private:
   APT_HIDDEN bool MergeListGroup(ListParser &List, std::string const &GrpName);
   APT_HIDDEN bool MergeListPackage(ListParser &List, pkgCache::PkgIterator &Pkg);
   APT_HIDDEN bool MergeListVersion(ListParser &List, pkgCache::PkgIterator &Pkg,
			 std::string const &Version, pkgCache::VerIterator* &OutVer);

   APT_HIDDEN bool AddImplicitDepends(pkgCache::GrpIterator &G, pkgCache::PkgIterator &P,
			   pkgCache::VerIterator &V);
   APT_HIDDEN bool AddImplicitDepends(pkgCache::VerIterator &V, pkgCache::PkgIterator &D);

   APT_HIDDEN bool AddNewDescription(ListParser &List, pkgCache::VerIterator &Ver,
	 std::string const &lang, MD5SumValue const &CurMd5, map_stringitem_t &md5idx);
};
									/*}}}*/
// This is the abstract package list parser class.			/*{{{*/
class pkgCacheGenerator::ListParser
{
   pkgCacheGenerator *Owner;
   friend class pkgCacheGenerator;
   
   // Some cache items
   pkgCache::VerIterator OldDepVer;
   map_pointer_t *OldDepLast;

   // Flag file dependencies
   bool FoundFileDeps;
      
   protected:

   inline map_stringitem_t StoreString(pkgCacheGenerator::StringType const type, std::string const &S) {return Owner->StoreString(type, S);};
   inline map_stringitem_t StoreString(pkgCacheGenerator::StringType const type, const char *S,unsigned int Size) {return Owner->StoreString(type, S, Size);};

   inline map_stringitem_t WriteString(const std::string &S) {return Owner->WriteStringInMap(S);};
   inline map_stringitem_t WriteString(const char *S,unsigned int Size) {return Owner->WriteStringInMap(S,Size);};
   bool NewDepends(pkgCache::VerIterator &Ver,const std::string &Package, const std::string &Arch,
		   const std::string &Version,unsigned int Op,
		   unsigned int Type);
   bool NewProvides(pkgCache::VerIterator &Ver,const std::string &PkgName,
		    const std::string &PkgArch, const std::string &Version);
   
   public:
   
   // These all operate against the current section
   virtual std::string Package() = 0;
   virtual std::string Architecture() = 0;
   virtual bool ArchitectureAll() = 0;
   virtual std::string Version() = 0;
   virtual bool NewVersion(pkgCache::VerIterator &Ver) = 0;
   virtual std::string Description(std::string const &lang) = 0;
   virtual std::vector<std::string> AvailableDescriptionLanguages() = 0;
   virtual MD5SumValue Description_md5() = 0;
   virtual unsigned short VersionHash() = 0;
   /** compare currently parsed version with given version
    *
    * \param Hash of the currently parsed version
    * \param Ver to compare with
    */
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   virtual
#endif
      APT_PURE bool SameVersion(unsigned short const Hash, pkgCache::VerIterator const &Ver);
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver) = 0;
   virtual map_filesize_t Offset() = 0;
   virtual map_filesize_t Size() = 0;
   
   virtual bool Step() = 0;
   
   inline bool HasFileDeps() {return FoundFileDeps;};
   virtual bool CollectFileProvides(pkgCache &/*Cache*/,
				    pkgCache::VerIterator &/*Ver*/) {return true;};

   ListParser() : Owner(NULL), OldDepLast(NULL), FoundFileDeps(false) {};
   virtual ~ListParser() {};
};
									/*}}}*/

bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap = 0,bool AllowMem = false);
bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap);

#endif
