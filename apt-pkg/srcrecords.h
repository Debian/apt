// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: srcrecords.h,v 1.8.2.1 2003/12/26 16:27:34 mdz Exp $
/* ######################################################################
   
   Source Package Records - Allows access to source package records
   
   Parses and allows access to the list of source records and searching by
   source name on that list.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SRCRECORDS_H
#define PKGLIB_SRCRECORDS_H

#include <apt-pkg/macros.h>
#include <apt-pkg/hashes.h>

#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
using std::vector;
#endif

class pkgSourceList;
class pkgIndexFile;
class pkgSrcRecords
{
   public:

#if __GNUC__ >= 4
	// ensure that con- & de-structor don't trigger this warning
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
   // Describes a single file
   struct File
   {
      APT_DEPRECATED std::string MD5Hash;
      APT_DEPRECATED unsigned long Size;
      std::string Path;
      std::string Type;
   };
   struct File2 : public File
   {
      unsigned long long FileSize;
      HashStringList Hashes;
   };
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

   // Abstract parser for each source record
   class Parser
   {
      protected:
      
      const pkgIndexFile *iIndex;
      
      public:

      enum BuildDep {BuildDepend=0x0,BuildDependIndep=0x1,
	             BuildConflict=0x2,BuildConflictIndep=0x3};

      struct BuildDepRec 
      {
	 std::string Package;
	 std::string Version;
	 unsigned int Op;
	 unsigned char Type;
      };
	
      inline const pkgIndexFile &Index() const {return *iIndex;};
      
      virtual bool Restart() = 0;
      virtual bool Step() = 0;
      virtual bool Jump(unsigned long const &Off) = 0;
      virtual unsigned long Offset() = 0;
      virtual std::string AsStr() = 0;
      
      virtual std::string Package() const = 0;
      virtual std::string Version() const = 0;
      virtual std::string Maintainer() const = 0;
      virtual std::string Section() const = 0;
      virtual const char **Binaries() = 0;   // Ownership does not transfer

      //FIXME: Add a parameter to specify which architecture to use for [wildcard] matching
      virtual bool BuildDepends(std::vector<BuildDepRec> &BuildDeps, bool const &ArchOnly, bool const &StripMultiArch = true) = 0;
      static const char *BuildDepType(unsigned char const &Type) APT_PURE;

      virtual bool Files(std::vector<pkgSrcRecords::File> &F) = 0;
      bool Files2(std::vector<pkgSrcRecords::File2> &F);
      
      Parser(const pkgIndexFile *Index) : iIndex(Index) {};
      virtual ~Parser() {};
   };
   
   private:
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;
   
   // The list of files and the current parser pointer
   std::vector<Parser*> Files;
   std::vector<Parser *>::iterator Current;
   
   public:

   // Reset the search
   bool Restart();

   // Step to the next SourcePackage and return pointer to the 
   // next SourceRecord. The pointer is owned by libapt.
   const Parser* Step();

   // Locate a package by name and return pointer to the Parser.
   // The pointer is owned by libapt.
   Parser* Find(const char *Package,bool const &SrcOnly = false);
   
   pkgSrcRecords(pkgSourceList &List);
   virtual ~pkgSrcRecords();
};

#endif
