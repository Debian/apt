// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: srcrecords.h,v 1.1 1999/04/04 01:17:29 jgg Exp $
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
   
   class Parser
   {
      FileFd *File;
     
      public:

      virtual bool Restart() = 0;
      virtual bool Step() = 0;
      virtual bool Jump(unsigned long Off) = 0;
      virtual unsigned long Offset() = 0;
      
      virtual string Package() = 0;
      virtual string Version() = 0;
      virtual string Maintainer() = 0;
      virtual string Section() = 0;
      virtual const char **Binaries() = 0;
      
      Parser(FileFd *File) : File(File) {};
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
