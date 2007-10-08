// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.h,v 1.20 2003/05/19 17:13:57 doogie Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This parser handles Debian package files (and others). Their form is
   RFC-822 type header fields in groups separated by a blank line.
   
   The parser reads the file and provides methods to step linearly
   over it or to jump to a pre-recorded start point and read that record.
   
   A second class is used to perform pre-parsing of the record. It works
   by indexing the start of each header field and providing lookup 
   functions for header fields.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_TAGFILE_H
#define PKGLIB_TAGFILE_H


#include <apt-pkg/fileutl.h>
#include <stdio.h>
    
class pkgTagSection
{
   const char *Section;
   const char *Stop;
   
   // We have a limit of 256 tags per section.
   unsigned int Indexes[256];
   unsigned int AlphaIndexes[0x100];
   
   unsigned int TagCount;
     
   public:
   
   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};
   
   bool Find(const char *Tag,const char *&Start, const char *&End) const;
   bool Find(const char *Tag,unsigned &Pos) const;
   string FindS(const char *Tag) const;
   signed int FindI(const char *Tag,signed long Default = 0) const ;
   bool FindFlag(const char *Tag,unsigned long &Flags,
		 unsigned long Flag) const;
   bool Scan(const char *Start,unsigned long MaxLength);
   inline unsigned long size() const {return Stop - Section;};
   void Trim();
   
   inline unsigned int Count() const {return TagCount;};
   inline void Get(const char *&Start,const char *&Stop,unsigned int I) const
                   {Start = Section + Indexes[I]; Stop = Section + Indexes[I+1];}
	    
   inline void GetSection(const char *&Start,const char *&Stop) const
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
   bool Done;
   unsigned long iOffset;
   unsigned long Size;

   bool Fill();
   bool Resize();

   public:

   bool Step(pkgTagSection &Section);
   inline unsigned long Offset() {return iOffset;};
   bool Jump(pkgTagSection &Tag,unsigned long Offset);

   pkgTagFile(FileFd *F,unsigned long Size = 32*1024);
   ~pkgTagFile();
};

/* This is the list of things to rewrite. The rewriter
   goes through and changes or adds each of these headers
   to suit. A zero forces the header to be erased, an empty string
   causes the old value to be used. (rewrite rule ignored) */
struct TFRewriteData
{
   const char *Tag;
   const char *Rewrite;
   const char *NewTag;
};
extern const char **TFRewritePackageOrder;
extern const char **TFRewriteSourceOrder;

bool TFRewrite(FILE *Output,pkgTagSection const &Tags,const char *Order[],
	       TFRewriteData *Rewrite);

#endif
