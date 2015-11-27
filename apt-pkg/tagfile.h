// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <stdint.h>

#include <string>
#include <vector>
#include <list>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#endif

class FileFd;
class pkgTagSectionPrivate;

class pkgTagSection
{
   const char *Section;
   unsigned int AlphaIndexes[0x100];

   pkgTagSectionPrivate * const d;

   protected:
   const char *Stop;

   public:

   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};

   bool Find(const char *Tag,const char *&Start, const char *&End) const;
   bool Find(const char *Tag,unsigned int &Pos) const;
   std::string FindS(const char *Tag) const;
   std::string FindRawS(const char *Tag) const;
   signed int FindI(const char *Tag,signed long Default = 0) const;
   bool FindB(const char *Tag, bool const &Default = false) const;
   unsigned long long FindULL(const char *Tag, unsigned long long const &Default = 0) const;
   bool FindFlag(const char * const Tag,uint8_t &Flags,
		 uint8_t const Flag) const;
   bool static FindFlag(uint8_t &Flags, uint8_t const Flag,
				const char* const Start, const char* const Stop);
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

   void Get(const char *&Start,const char *&Stop,unsigned int I) const;

   inline void GetSection(const char *&Start,const char *&Stop) const
   {
      Start = Section;
      Stop = this->Stop;
   };

   pkgTagSection();
   virtual ~pkgTagSection();

   struct Tag
   {
      enum ActionType { REMOVE, RENAME, REWRITE } Action;
      std::string Name;
      std::string Data;

      static Tag Remove(std::string const &Name);
      static Tag Rename(std::string const &OldName, std::string const &NewName);
      static Tag Rewrite(std::string const &Name, std::string const &Data);
      private:
      Tag(ActionType const Action, std::string const &Name, std::string const &Data) :
	 Action(Action), Name(Name), Data(Data) {}
   };

   /** Write this section (with optional rewrites) to a file
    *
    * @param File to write the section to
    * @param Order in which tags should appear in the file
    * @param Rewrite is a set of tags to be renamed, rewritten and/or removed
    * @return \b true if successful, otherwise \b false
    */
   bool Write(FileFd &File, char const * const * const Order = NULL, std::vector<Tag> const &Rewrite = std::vector<Tag>()) const;
};


/* For user generated file the parser should be a bit more relaxed in exchange
   for being a bit slower to allow comments and new lines all over the place */
class pkgUserTagSection : public pkgTagSection
{
   virtual void TrimRecord(bool BeforeRecord, const char* &End) APT_OVERRIDE;
};

class pkgTagFilePrivate;
class pkgTagFile
{
   pkgTagFilePrivate * const d;

   APT_HIDDEN bool Fill();
   APT_HIDDEN bool Resize();
   APT_HIDDEN bool Resize(unsigned long long const newSize);

   public:

   bool Step(pkgTagSection &Section);
   unsigned long Offset();
   bool Jump(pkgTagSection &Tag,unsigned long long Offset);

   void Init(FileFd * const F,unsigned long long const Size = 32*1024);

   pkgTagFile(FileFd * const F,unsigned long long Size = 32*1024);
   virtual ~pkgTagFile();
};

extern const char **TFRewritePackageOrder;
extern const char **TFRewriteSourceOrder;

APT_IGNORE_DEPRECATED_PUSH
struct APT_DEPRECATED_MSG("Use pkgTagSection::Tag and pkgTagSection::Write() instead") TFRewriteData
{
   const char *Tag;
   const char *Rewrite;
   const char *NewTag;
};
APT_DEPRECATED_MSG("Use pkgTagSection::Tag and pkgTagSection::Write() instead") bool TFRewrite(FILE *Output,pkgTagSection const &Tags,const char *Order[],
	       TFRewriteData *Rewrite);
APT_IGNORE_DEPRECATED_POP

#endif
