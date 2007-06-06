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


#include <string>
#include <vector>    

using std::string;
using std::vector;

class pkgSourceList;
class pkgIndexFile;
class pkgSrcRecords
{
   public:

   // Describes a single file
   struct File
   {
      string MD5Hash;
      unsigned long Size;
      string Path;
      string Type;
   };
   
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
         string Package;
	 string Version;
	 unsigned int Op;
	 unsigned char Type;
      };
	
      inline const pkgIndexFile &Index() const {return *iIndex;};
      
      virtual bool Restart() = 0;
      virtual bool Step() = 0;
      virtual bool Jump(unsigned long Off) = 0;
      virtual unsigned long Offset() = 0;
      virtual string AsStr() = 0;
      
      virtual string Package() const = 0;
      virtual string Version() const = 0;
      virtual string Maintainer() const = 0;
      virtual string Section() const = 0;
      virtual const char **Binaries() = 0;   // Ownership does not transfer

      virtual bool BuildDepends(vector<BuildDepRec> &BuildDeps, bool ArchOnly) = 0;
      static const char *BuildDepType(unsigned char Type);

      virtual bool Files(vector<pkgSrcRecords::File> &F) = 0;
      
      Parser(const pkgIndexFile *Index) : iIndex(Index) {};
      virtual ~Parser() {};
   };
   
   private:
   
   // The list of files and the current parser pointer
   vector<Parser*> Files;
   vector<Parser *>::iterator Current;
   
   public:

   // Reset the search
   bool Restart();

   // Locate a package by name
   Parser *Find(const char *Package,bool SrcOnly = false);
   
   pkgSrcRecords(pkgSourceList &List);
   ~pkgSrcRecords();
};

#endif
