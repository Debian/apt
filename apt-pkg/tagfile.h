// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.h,v 1.12 1998/12/07 07:26:23 jgg Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This parser handles Debian package files (and others). Their form is
   RFC-822 type header fields in groups seperated by a blank line.
   
   The parser reads the file and provides methods to step linearly
   over it or to jump to a pre-recorded start point and read that record.
   
   A second class is used to perform pre-parsing of the record. It works
   by indexing the start of each header field and providing lookup 
   functions for header fields.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_TAGFILE_H
#define PKGLIB_TAGFILE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/tagfile.h"
#endif 

#include <apt-pkg/fileutl.h>

class pkgTagSection
{
   const char *Section;
   const char *Stop;
   
   // We have a limit of 256 tags per section.
   unsigned short Indexes[256];
   unsigned short AlphaIndexes[26 + 26*26];
   
   unsigned int TagCount;
     
   public:
   
   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};
   
   bool Find(const char *Tag,const char *&Start, const char *&End);
   string FindS(const char *Tag);
   unsigned int FindI(const char *Tag);   
   bool Scan(const char *Start,unsigned long MaxLength);
   inline unsigned long size() {return Stop - Section;};

   inline unsigned int Count() {return TagCount;};
   inline void Get(const char *&Start,const char *&Stop,unsigned int I)
                   {Start = Section + Indexes[I]; Stop = Section + Indexes[I+1];}
	    
   inline void GetSection(const char *&Start,const char *&Stop)
   {
      Start = Section;
      Stop = this->Stop;
   };
   
   pkgTagSection() : Section(0), Stop(0) {};
};

class pkgTagFile
{
   FileFd &Fd;
   char *Buffer;
   char *Start;
   char *End;
   unsigned long Left;
   unsigned long iOffset;
   unsigned long Size;
   
   bool Fill();
   
   public:

   bool Step(pkgTagSection &Section);
   inline unsigned long Offset() {return iOffset;};
   bool Jump(pkgTagSection &Tag,unsigned long Offset);
   
   pkgTagFile(FileFd &F,unsigned long Size = 32*1024);
};

#endif
