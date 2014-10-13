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

#include <apt-pkg/macros.h>

#include <stdio.h>

#include <string>
#include <vector>
#include <list>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#endif

class FileFd;

class pkgTagSection
{
   const char *Section;
   struct TagData {
      unsigned int StartTag;
      unsigned int EndTag;
      unsigned int StartValue;
      unsigned int NextInBucket;

      TagData(unsigned int const StartTag) : StartTag(StartTag), EndTag(0), StartValue(0), NextInBucket(0) {}
   };
   std::vector<TagData> Tags;
   unsigned int LookupTable[0x100];

   // dpointer placeholder (for later in case we need it)
   void *d;

   protected:
   const char *Stop;

   public:
   
   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};
   
   bool Find(const char *Tag,const char *&Start, const char *&End) const;
   bool Find(const char *Tag,unsigned int &Pos) const;
   std::string FindS(const char *Tag) const;
   signed int FindI(const char *Tag,signed long Default = 0) const;
   bool FindB(const char *Tag, bool const &Default = false) const;
   unsigned long long FindULL(const char *Tag, unsigned long long const &Default = 0) const;
   bool FindFlag(const char *Tag,unsigned long &Flags,
		 unsigned long Flag) const;
   bool static FindFlag(unsigned long &Flags, unsigned long Flag,
				const char* Start, const char* Stop);

   /** \brief searches the boundaries of the current section
    *
    * While parameter Start marks the beginning of the section, this method
    * will search for the first double newline in the data stream which marks
    * the end of the section. It also does a first pass over the content of
    * the section parsing it as encountered for processing later on by Find
    *
    * @param Start is the beginning of the section
    * @param MaxLength is the size of valid data in the stream pointed to by Start
    * @param Restart if enabled internal state will be cleared, otherwise it is
    *  assumed that now more data is available in the stream and the parsing will
    *  start were it encountered insufficent data the last time.
    *
    * @return \b true if section end was found, \b false otherwise.
    *  Beware that internal state will be inconsistent if \b false is returned!
    */
   APT_MUSTCHECK bool Scan(const char *Start, unsigned long MaxLength, bool const Restart = true);
   inline unsigned long size() const {return Stop - Section;};
   void Trim();
   virtual void TrimRecord(bool BeforeRecord, const char* &End);

   /** \brief amount of Tags in the current section
    *
    * Note: if a Tag is mentioned repeatly it will be counted multiple
    * times, but only the last occurrence is available via Find methods.
    */
   unsigned int Count() const;
   bool Exists(const char* const Tag) const;

   inline void Get(const char *&Start,const char *&Stop,unsigned int I) const
                   {Start = Section + Tags[I].StartTag; Stop = Section + Tags[I+1].StartTag;}

   inline void GetSection(const char *&Start,const char *&Stop) const
   {
      Start = Section;
      Stop = this->Stop;
   };
   
   pkgTagSection();
   virtual ~pkgTagSection();
};

class pkgTagFilePrivate;
class pkgTagFile
{
   pkgTagFilePrivate *d;

   APT_HIDDEN bool Fill();
   APT_HIDDEN bool Resize();
   APT_HIDDEN bool Resize(unsigned long long const newSize);

   public:

   bool Step(pkgTagSection &Section);
   unsigned long Offset();
   bool Jump(pkgTagSection &Tag,unsigned long long Offset);

   void Init(FileFd *F,unsigned long long Size = 32*1024);

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
