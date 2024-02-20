// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SourceList - Manage a list of sources
   
   The Source List class provides access to a list of sources. It 
   can read them from a file and generate a list of all the distinct
   sources.
   
   All sources have a type associated with them that defines the layout
   of the archive. The exact format of the file is documented in
   files.sgml.

   The types are mapped through a list of type definitions which handle
   the actual construction of the back end type. After loading a source 
   list all you have is a list of package index files that have the ability
   to be Acquired.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SOURCELIST_H
#define PKGLIB_SOURCELIST_H

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <ctime>

#include <map>
#include <string>
#include <vector>


class FileFd;
class pkgTagSection;
class pkgAcquire;
class pkgIndexFile;
class metaIndex;
class CommandLine;

class APT_PUBLIC pkgSourceList
{
   void * const d;
   std::vector<pkgIndexFile*> VolatileFiles;
   public:

   // List of supported source list types
   class Type
   {
      public:

      // Global list of Items supported
      static Type **GlobalList;
      static unsigned long GlobalListLen;
      static Type *GetType(const char *Type) APT_PURE;

      char const * const Name;
      char const * const Label;

      bool FixupURI(std::string &URI) const;
      virtual bool ParseStanza(std::vector<metaIndex *> &List,
                               pkgTagSection &Tags,
                               unsigned int const stanza_n,
                               FileFd &Fd);
      virtual bool ParseLine(std::vector<metaIndex *> &List,
			     const char *Buffer,
			     unsigned int const CurLine,std::string const &File) const;
      virtual bool CreateItem(std::vector<metaIndex *> &List,std::string const &URI,
			      std::string const &Dist,std::string const &Section,
			      std::map<std::string, std::string> const &Options) const = 0;
      Type(char const * const Name, char const * const Label);
      virtual ~Type();
   };

   typedef std::vector<metaIndex *>::const_iterator const_iterator;

   protected:

   std::vector<metaIndex *> SrcList;

   private:
   APT_HIDDEN bool ParseFileDeb822(std::string const &File);
   APT_HIDDEN bool ParseFileOldStyle(std::string const &File);

   public:

   bool ReadMainList();
   bool Read(std::string const &File);

   // CNC:2003-03-03
   void Reset();
   bool ReadAppend(std::string const &File);
   bool ReadSourceDir(std::string const &Dir);
   
   // List accessors
   inline const_iterator begin() const {return SrcList.begin();};
   inline const_iterator end() const {return SrcList.end();};
   inline unsigned int size() const {return SrcList.size();};
   inline bool empty() const {return SrcList.empty();};

   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const;
   bool GetIndexes(pkgAcquire *Owner, bool GetAll=false) const;
   
   // query last-modified time
   time_t GetLastModifiedTime();

   /** \brief add file for parsing, but not to the cache
    *
    *  pkgIndexFiles originating from pkgSourcesList are included in
    *  srcpkgcache, the status files added via #AddStatusFiles are
    *  included in pkgcache, but these files here are not included in
    *  any cache to have the possibility of having a file included just
    *  for a single run like a local .deb/.dsc file.
    *
    *  The volatile files do not count as "normal" sourceslist entries,
    *  can't be iterated over with #begin and #end and can't be
    *  downloaded, but they can be found via #FindIndex.
    *
    *  @param File is an index file; pointer-ownership is transferred
    */
   void AddVolatileFile(pkgIndexFile * const File);
   bool AddVolatileFile(std::string const &File);
   bool AddVolatileFile(std::string const &File, std::vector<std::string> * const VolatileCmdL);
   void AddVolatileFiles(CommandLine &CmdL, std::vector<std::string> * const VolatileCmdL);

   /** @return list of files registered with #AddVolatileFile */
   std::vector<pkgIndexFile*> GetVolatileFiles() const;

   pkgSourceList();
   virtual ~pkgSourceList();
};

#endif
