// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: srcrecords.h,v 1.3 1999/04/07 05:30:18 jgg Exp $
/* ######################################################################
   
   Source Package Records - Allows access to source package records
   
   Parses and allows access to the list of source records and searching by
   source name on that list.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SRCRECORDS_H
#define PKGLIB_SRCRECORDS_H

#ifdef __GNUG__
#pragma interface "apt-pkg/srcrecords.h"
#endif 

#include <apt-pkg/fileutl.h>
#include <apt-pkg/sourcelist.h>

class pkgSrcRecords
{
   public:

   // Describes a single file
   struct File
   {
      string MD5Hash;
      unsigned long Size;
      string Path;
   };
   
   // Abstract parser for each source record
   class Parser
   {
      FileFd *File;
      pkgSourceList::const_iterator SrcItem;
      
      public:

      inline pkgSourceList::const_iterator Source() const {return SrcItem;};
      
      virtual bool Restart() = 0;
      virtual bool Step() = 0;
      virtual bool Jump(unsigned long Off) = 0;
      virtual unsigned long Offset() = 0;
      
      virtual string Package() = 0;
      virtual string Version() = 0;
      virtual string Maintainer() = 0;
      virtual string Section() = 0;
      virtual const char **Binaries() = 0;
      virtual bool Files(vector<File> &F) = 0;
      
      Parser(FileFd *File,pkgSourceList::const_iterator SrcItem) : File(File), 
             SrcItem(SrcItem) {};
      virtual ~Parser() {delete File;};
   };
   
   private:
   
   // The list of files and the current parser pointer
   Parser **Files;
   Parser **Current;
   
   public:

   // Reset the search
   bool Restart();

   // Locate a package by name
   Parser *Find(const char *Package,bool SrcOnly = false);
   
   pkgSrcRecords(pkgSourceList &List);
   ~pkgSrcRecords();
};


#endif
