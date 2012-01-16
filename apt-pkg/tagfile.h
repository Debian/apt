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

#include <stdio.h>

#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#endif

class FileFd;

class pkgTagSection
{
   const char *Section;
   // We have a limit of 256 tags per section.
   unsigned int Indexes[256];
   unsigned int AlphaIndexes[0x100];
   unsigned int TagCount;
   // dpointer placeholder (for later in case we need it)
   void *d;

   /* This very simple hash function for the last 8 letters gives
      very good performance on the debian package files */
   inline static unsigned long AlphaHash(const char *Text, const char *End = 0)
   {
      unsigned long Res = 0;
      for (; Text != End && *Text != ':' && *Text != 0; Text++)
	 Res = ((unsigned long)(*Text) & 0xDF) ^ (Res << 1);
      return Res & 0xFF;
   }

   protected:
   const char *Stop;

   public:
   
   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};
   
   bool Find(const char *Tag,const char *&Start, const char *&End) const;
   bool Find(const char *Tag,unsigned &Pos) const;
   std::string FindS(const char *Tag) const;
   signed int FindI(const char *Tag,signed long Default = 0) const ;
   unsigned long long FindULL(const char *Tag, unsigned long long const &Default = 0) const;
   bool FindFlag(const char *Tag,unsigned long &Flags,
		 unsigned long Flag) const;
   bool static const FindFlag(unsigned long &Flags, unsigned long Flag,
				const char* Start, const char* Stop);
   bool Scan(const char *Start,unsigned long MaxLength);
   inline unsigned long size() const {return Stop - Section;};
   void Trim();
   virtual void TrimRecord(bool BeforeRecord, const char* &End);
   
   inline unsigned int Count() const {return TagCount;};
   inline bool Exists(const char* const Tag) {return AlphaIndexes[AlphaHash(Tag)] != 0;}
 
   inline void Get(const char *&Start,const char *&Stop,unsigned int I) const
                   {Start = Section + Indexes[I]; Stop = Section + Indexes[I+1];}
	    
   inline void GetSection(const char *&Start,const char *&Stop) const
   {
      Start = Section;
      Stop = this->Stop;
   };
   
   pkgTagSection() : Section(0), TagCount(0), Stop(0) {};
   virtual ~pkgTagSection() {};
};

class pkgTagFilePrivate;
class pkgTagFile
{
   pkgTagFilePrivate *d;

   bool Fill();
   bool Resize();

   public:

   bool Step(pkgTagSection &Section);
   unsigned long Offset();
   bool Jump(pkgTagSection &Tag,unsigned long long Offset);

   pkgTagFile(FileFd *F,unsigned long long Size = 32*1024);
   virtual ~pkgTagFile();
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
