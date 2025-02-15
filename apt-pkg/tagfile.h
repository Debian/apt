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

#include <cstdint>
#include <cstdio>

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>


class FileFd;
class pkgTagSectionPrivate;
class pkgTagFilePrivate;

/** \class pkgTagSection parses a single deb822 stanza and provides various Find methods
 * to extract the included values. It can also be used to modify and write a
 * valid deb822 stanza optionally (re)ordering the fields inside the stanza.
 *
 * Beware: This class does \b NOT support (#-)comments in in- or output!
 * If the input contains comments they have to be stripped first like pkgTagFile
 * does with SUPPORT_COMMENTS flag set. */
class APT_PUBLIC pkgTagSection
{
   const char *Section;
   unsigned int AlphaIndexes[128];
   unsigned int BetaIndexes[128];

   std::unique_ptr<pkgTagSectionPrivate> const d;

   APT_HIDDEN bool FindInternal(unsigned int Pos,const char *&Start, const char *&End) const;
   APT_HIDDEN std::string_view FindInternal(unsigned int Pos) const;
   APT_HIDDEN std::string_view FindRawInternal(unsigned int Pos) const;
   APT_HIDDEN signed int FindIInternal(unsigned int Pos,signed long Default = 0) const;
   APT_HIDDEN bool FindBInternal(unsigned int Pos, bool Default = false) const;
   APT_HIDDEN unsigned long long FindULLInternal(unsigned int Pos, unsigned long long const &Default = 0) const;
   APT_HIDDEN bool FindFlagInternal(unsigned int Pos,uint8_t &Flags, uint8_t const Flag) const;
   APT_HIDDEN bool FindFlagInternal(unsigned int Pos,unsigned long &Flags, unsigned long Flag) const;

   protected:
   const char *Stop;

   public:

   inline bool operator ==(const pkgTagSection &rhs) {return Section == rhs.Section;};
   inline bool operator !=(const pkgTagSection &rhs) {return Section != rhs.Section;};

   // TODO: Remove internally
   std::string FindS(std::string_view sv) const { return std::string{Find(sv)}; }
   std::string FindRawS(std::string_view sv) const { return std::string{FindRaw(sv)}; };

   // Functions for lookup with a perfect hash function
   enum class Key;
#ifdef APT_COMPILING_APT
   bool Find(Key key,const char *&Start, const char *&End) const;
   bool Find(Key key,unsigned int &Pos) const;
   signed int FindI(Key key,signed long Default = 0) const;
   bool FindB(Key key, bool Default = false) const;
   unsigned long long FindULL(Key key, unsigned long long const &Default = 0) const;
   bool FindFlag(Key key,uint8_t &Flags, uint8_t const Flag) const;
   bool FindFlag(Key key,unsigned long &Flags, unsigned long Flag) const;
   bool Exists(Key key) const;
   std::string_view Find(Key key) const;
   std::string_view FindRaw(Key key) const;
#endif

   bool Find(std::string_view Tag,const char *&Start, const char *&End) const;
   bool Find(std::string_view Tag,unsigned int &Pos) const;
   std::string_view Find(std::string_view Tag) const;
   std::string_view FindRaw(std::string_view Tag) const;
   signed int FindI(std::string_view Tag,signed long Default = 0) const;
   bool FindB(std::string_view, bool Default = false) const;
   unsigned long long FindULL(std::string_view Tag, unsigned long long const &Default = 0) const;

   bool FindFlag(std::string_view Tag,uint8_t &Flags,
		 uint8_t const Flag) const;
   bool FindFlag(std::string_view Tag,unsigned long &Flags,
		 unsigned long Flag) const;
   bool Exists(std::string_view Tag) const;

   bool static FindFlag(uint8_t &Flags, uint8_t const Flag,
				const char* const Start, const char* const Stop);
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
    *  start were it encountered insufficient data the last time.
    *
    * @return \b true if section end was found, \b false otherwise.
    *  Beware that internal state will be inconsistent if \b false is returned!
    */
   [[nodiscard]] bool Scan(const char *Start, unsigned long MaxLength, bool const Restart = true);

   inline unsigned long size() const {return Stop - Section;};
   void Trim();
   virtual void TrimRecord(bool BeforeRecord, const char* &End);

   /** \brief amount of Tags in the current section
    *
    * Note: if a Tag is mentioned repeatedly it will be counted multiple
    * times, but only the last occurrence is available via Find methods.
    */
   unsigned int Count() const;

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

      static Tag Remove(std::string_view Name);
      static Tag Rename(std::string_view OldName, std::string_view NewName);
      static Tag Rewrite(std::string_view Name, std::string_view Data);
      private:
      Tag(ActionType const Action, std::string_view Name, std::string_view Data) :
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
#ifdef APT_COMPILING_APT
   enum WriteFlags
   {
      WRITE_DEFAULT = 0,
      WRITE_HUMAN = (1 << 0), /* write human readable output, may include highlighting */
   };
   bool Write(FileFd &File, WriteFlags flags, char const *const *const Order = NULL, std::vector<Tag> const &Rewrite = std::vector<Tag>()) const;
#endif
};


/** \class pkgTagFile reads and prepares a deb822 formatted file for parsing
 * via #pkgTagSection. The default mode tries to be as fast as possible and
 * assumes perfectly valid (machine generated) files like Packages. Support
 * for comments e.g. needs to be enabled explicitly. */
class APT_PUBLIC pkgTagFile
{
   std::unique_ptr<pkgTagFilePrivate> const d;

   APT_HIDDEN bool Fill();
   APT_HIDDEN bool Resize();
   APT_HIDDEN bool Resize(unsigned long long const newSize);

public:

   bool Step(pkgTagSection &Section);
   unsigned long Offset();
   bool Jump(pkgTagSection &Tag,unsigned long long Offset);

   enum Flags
   {
      STRICT = 0,
      SUPPORT_COMMENTS = 1 << 0,
   };

   void Init(FileFd * const F, pkgTagFile::Flags const Flags, unsigned long long Size = APT_BUFFER_SIZE);
   void Init(FileFd * const F,unsigned long long const Size = APT_BUFFER_SIZE);

   pkgTagFile(FileFd * const F, pkgTagFile::Flags const Flags, unsigned long long Size = APT_BUFFER_SIZE);
   pkgTagFile(FileFd * const F,unsigned long long Size = APT_BUFFER_SIZE);
   virtual ~pkgTagFile();
};

APT_PUBLIC extern const char **TFRewritePackageOrder;
APT_PUBLIC extern const char **TFRewriteSourceOrder;

#endif
