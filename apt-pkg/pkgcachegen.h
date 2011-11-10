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


#include <apt-pkg/pkgcache.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/macros.h>

#include <vector>

class pkgSourceList;
class OpProgress;
class MMap;
class pkgIndexFile;

class pkgCacheGenerator							/*{{{*/
{
   private:

   pkgCache::StringItem *UniqHash[26];
   map_ptrloc WriteStringInMap(std::string const &String) { return WriteStringInMap(String.c_str()); };
   map_ptrloc WriteStringInMap(const char *String);
   map_ptrloc WriteStringInMap(const char *String, const unsigned long &Len);
   map_ptrloc AllocateInMap(const unsigned long &size);

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
		   unsigned int const &Type, map_ptrloc* &OldDepLast);
   unsigned long NewVersion(pkgCache::VerIterator &Ver,const std::string &VerStr,unsigned long Next);
   map_ptrloc NewDescription(pkgCache::DescIterator &Desc,const std::string &Lang,const MD5SumValue &md5sum,map_ptrloc Next);

   public:

   unsigned long WriteUniqString(const char *S,unsigned int Size);
   inline unsigned long WriteUniqString(const std::string &S) {return WriteUniqString(S.c_str(),S.length());};

   void DropProgress() {Progress = 0;};
   bool SelectFile(const std::string &File,const std::string &Site,pkgIndexFile const &Index,
		   unsigned long Flags = 0);
   bool MergeList(ListParser &List,pkgCache::VerIterator *Ver = 0);
   inline pkgCache &GetCache() {return Cache;};
   inline pkgCache::PkgFileIterator GetCurFile() 
         {return pkgCache::PkgFileIterator(Cache,CurrentFile);};

   bool HasFileDeps() {return FoundFileDeps;};
   bool MergeFileProvides(ListParser &List);
   __deprecated bool FinishCache(OpProgress *Progress);

   static bool MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap = 0,bool AllowMem = false);
   static bool MakeOnlyStatusCache(OpProgress *Progress,DynamicMMap **OutMap);
   static DynamicMMap* CreateDynamicMMap(FileFd *CacheF, unsigned long Flags = 0);

   void ReMap(void const * const oldMap, void const * const newMap);

   pkgCacheGenerator(DynamicMMap *Map,OpProgress *Progress);
   ~pkgCacheGenerator();

   private:
   bool MergeListGroup(ListParser &List, std::string const &GrpName);
   bool MergeListPackage(ListParser &List, pkgCache::PkgIterator &Pkg);
   bool MergeListVersion(ListParser &List, pkgCache::PkgIterator &Pkg,
			 std::string const &Version, pkgCache::VerIterator* &OutVer);

   bool AddImplicitDepends(pkgCache::GrpIterator &G, pkgCache::PkgIterator &P,
			   pkgCache::VerIterator &V);
   bool AddImplicitDepends(pkgCache::VerIterator &V, pkgCache::PkgIterator &D);
};
									/*}}}*/
// This is the abstract package list parser class.			/*{{{*/
class pkgCacheGenerator::ListParser
{
   pkgCacheGenerator *Owner;
   friend class pkgCacheGenerator;
   
   // Some cache items
   pkgCache::VerIterator OldDepVer;
   map_ptrloc *OldDepLast;

   // Flag file dependencies
   bool FoundFileDeps;
      
   protected:

   inline unsigned long WriteUniqString(std::string S) {return Owner->WriteUniqString(S);};
   inline unsigned long WriteUniqString(const char *S,unsigned int Size) {return Owner->WriteUniqString(S,Size);};
   inline unsigned long WriteString(const std::string &S) {return Owner->WriteStringInMap(S);};
   inline unsigned long WriteString(const char *S,unsigned int Size) {return Owner->WriteStringInMap(S,Size);};
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
   virtual std::string Description() = 0;
   virtual std::string DescriptionLanguage() = 0;
   virtual MD5SumValue Description_md5() = 0;
   virtual unsigned short VersionHash() = 0;
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver) = 0;
   virtual unsigned long Offset() = 0;
   virtual unsigned long Size() = 0;
   
   virtual bool Step() = 0;
   
   inline bool HasFileDeps() {return FoundFileDeps;};
   virtual bool CollectFileProvides(pkgCache &Cache,
				    pkgCache::VerIterator &Ver) {return true;};

   ListParser() : FoundFileDeps(false) {};
   virtual ~ListParser() {};
};
									/*}}}*/

bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap = 0,bool AllowMem = false);
bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap);

#endif
