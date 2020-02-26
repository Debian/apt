// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This uses a rotating buffer to load the package information into.
   The scanner runs over it and isolates and indexes a single section.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile-keys.h>
#include <apt-pkg/tagfile.h>

#include <list>

#include <string>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

using std::string;
using APT::StringView;

class APT_HIDDEN pkgTagFilePrivate					/*{{{*/
{
public:
   void Reset(FileFd * const pFd, unsigned long long const pSize, pkgTagFile::Flags const pFlags)
   {
      if (Buffer != NULL)
	 free(Buffer);
      Buffer = NULL;
      Fd = pFd;
      Flags = pFlags;
      Start = NULL;
      End = NULL;
      Done = false;
      iOffset = 0;
      Size = pSize;
      isCommentedLine = false;
      chunks.clear();
   }

   pkgTagFilePrivate(FileFd * const pFd, unsigned long long const Size, pkgTagFile::Flags const pFlags) : Buffer(NULL)
   {
      Reset(pFd, Size, pFlags);
   }
   FileFd * Fd;
   pkgTagFile::Flags Flags;
   char *Buffer;
   char *Start;
   char *End;
   bool Done;
   unsigned long long iOffset;
   unsigned long long Size;
   bool isCommentedLine;
   struct FileChunk
   {
      bool const good;
      size_t length;
      FileChunk(bool const pgood, size_t const plength) noexcept : good(pgood), length(plength) {}
   };
   std::list<FileChunk> chunks;

   ~pkgTagFilePrivate()
   {
      if (Buffer != NULL)
	 free(Buffer);
   }
};
									/*}}}*/
class APT_HIDDEN pkgTagSectionPrivate					/*{{{*/
{
public:
   pkgTagSectionPrivate()
   {
   }
   struct TagData {
      unsigned int StartTag;
      unsigned int EndTag;
      unsigned int StartValue;
      unsigned int NextInBucket;

      explicit TagData(unsigned int const StartTag) : StartTag(StartTag), EndTag(0), StartValue(0), NextInBucket(0) {}
   };
   std::vector<TagData> Tags;
};
									/*}}}*/

static unsigned long BetaHash(const char *Text, size_t Length)		/*{{{*/
{
   /* This very simple hash function for the last 8 letters gives
      very good performance on the debian package files */
   if (Length > 8)
   {
    Text += (Length - 8);
    Length = 8;
   }
   unsigned long Res = 0;
   for (size_t i = 0; i < Length; ++i)
      Res = ((unsigned long)(Text[i]) & 0xDF) ^ (Res << 1);
   return Res & 0x7F;
}
									/*}}}*/

// TagFile::pkgTagFile - Constructor					/*{{{*/
pkgTagFile::pkgTagFile(FileFd * const pFd,pkgTagFile::Flags const pFlags, unsigned long long const Size)
   : d(new pkgTagFilePrivate(pFd, Size + 4, pFlags))
{
   Init(pFd, pFlags, Size);
}
pkgTagFile::pkgTagFile(FileFd * const pFd,unsigned long long const Size)
   : pkgTagFile(pFd, pkgTagFile::STRICT, Size)
{
}
void pkgTagFile::Init(FileFd * const pFd, pkgTagFile::Flags const pFlags, unsigned long long Size)
{
   /* The size is increased by 4 because if we start with the Size of the
      filename we need to try to read 1 char more to see an EOF faster, 1
      char the end-pointer can be on and maybe 2 newlines need to be added
      to the end of the file -> 4 extra chars */
   Size += 4;
   d->Reset(pFd, Size, pFlags);

   if (d->Fd->IsOpen() == false)
      d->Start = d->End = d->Buffer = 0;
   else
      d->Buffer = (char*)malloc(sizeof(char) * Size);

   if (d->Buffer == NULL)
      d->Done = true;
   else
      d->Done = false;

   d->Start = d->End = d->Buffer;
   d->iOffset = 0;
   if (d->Done == false)
      Fill();
}
void pkgTagFile::Init(FileFd * const pFd,unsigned long long Size)
{
   Init(pFd, pkgTagFile::STRICT, Size);
}
									/*}}}*/
// TagFile::~pkgTagFile - Destructor					/*{{{*/
pkgTagFile::~pkgTagFile()
{
   delete d;
}
									/*}}}*/
// TagFile::Offset - Return the current offset in the buffer		/*{{{*/
APT_PURE unsigned long pkgTagFile::Offset()
{
   return d->iOffset;
}
									/*}}}*/
// TagFile::Resize - Resize the internal buffer				/*{{{*/
// ---------------------------------------------------------------------
/* Resize the internal buffer (double it in size). Fail if a maximum size
 * size is reached.
 */
bool pkgTagFile::Resize()
{
   // fail is the buffer grows too big
   if(d->Size > 1024*1024+1)
      return false;

   return Resize(d->Size * 2);
}
bool pkgTagFile::Resize(unsigned long long const newSize)
{
   unsigned long long const EndSize = d->End - d->Start;

   // get new buffer and use it
   char* const newBuffer = static_cast<char*>(realloc(d->Buffer, sizeof(char) * newSize));
   if (newBuffer == NULL)
      return false;
   d->Buffer = newBuffer;
   d->Size = newSize;

   // update the start/end pointers to the new buffer
   d->Start = d->Buffer;
   d->End = d->Start + EndSize;
   return true;
}
									/*}}}*/
// TagFile::Step - Advance to the next section				/*{{{*/
// ---------------------------------------------------------------------
/* If the Section Scanner fails we refill the buffer and try again. 
 * If that fails too, double the buffer size and try again until a
 * maximum buffer is reached.
 */
bool pkgTagFile::Step(pkgTagSection &Tag)
{
   if(Tag.Scan(d->Start,d->End - d->Start) == false)
   {
      do
      {
	 if (Fill() == false)
	    return false;

	 if(Tag.Scan(d->Start,d->End - d->Start, false))
	    break;

	 if (Resize() == false)
	    return _error->Error(_("Unable to parse package file %s (%d)"),
		  d->Fd->Name().c_str(), 1);

      } while (Tag.Scan(d->Start,d->End - d->Start, false) == false);
   }

   size_t tagSize = Tag.size();
   d->Start += tagSize;

   if ((d->Flags & pkgTagFile::SUPPORT_COMMENTS) == 0)
      d->iOffset += tagSize;
   else
   {
      auto first = d->chunks.begin();
      for (; first != d->chunks.end(); ++first)
      {
	 if (first->good == false)
	    d->iOffset += first->length;
	 else
	 {
	    if (tagSize < first->length)
	    {
	       first->length -= tagSize;
	       d->iOffset += tagSize;
	       break;
	    }
	    else
	    {
	       tagSize -= first->length;
	       d->iOffset += first->length;
	    }
	 }
      }
      d->chunks.erase(d->chunks.begin(), first);
   }

   if ((d->Flags & pkgTagFile::SUPPORT_COMMENTS) == 0 || Tag.Count() != 0)
   {
      Tag.Trim();
      return true;
   }
   return Step(Tag);
}
									/*}}}*/
// TagFile::Fill - Top up the buffer					/*{{{*/
// ---------------------------------------------------------------------
/* This takes the bit at the end of the buffer and puts it at the start
   then fills the rest from the file */
static bool FillBuffer(pkgTagFilePrivate * const d)
{
   unsigned long long Actual = 0;
   // See if only a bit of the file is left
   unsigned long long const dataSize = d->Size - ((d->End - d->Buffer) + 1);
   if (d->Fd->Read(d->End, dataSize, &Actual) == false)
      return false;
   if (Actual != dataSize)
      d->Done = true;
   d->End += Actual;
   return true;
}
static void RemoveCommentsFromBuffer(pkgTagFilePrivate * const d)
{
   // look for valid comments in the buffer
   char * good_start = nullptr, * bad_start = nullptr;
   char * current = d->Start;
   if (d->isCommentedLine == false)
   {
      if (d->Start == d->Buffer)
      {
	 // the start of the buffer is a newline as a record can't start
	 // in the middle of a line by definition.
	 if (*d->Start == '#')
	 {
	    d->isCommentedLine = true;
	    ++current;
	    if (current > d->End)
	       d->chunks.emplace_back(false, 1);
	 }
      }
      if (d->isCommentedLine == false)
	 good_start = d->Start;
      else
	 bad_start = d->Start;
   }
   else
      bad_start = d->Start;

   std::vector<std::pair<char*, size_t>> good_parts;
   while (current <= d->End)
   {
      size_t const restLength = (d->End - current);
      if (d->isCommentedLine == false)
      {
	 current = static_cast<char*>(memchr(current, '#', restLength));
	 if (current == nullptr)
	 {
	    size_t const goodLength = d->End - good_start;
	    d->chunks.emplace_back(true, goodLength);
	    if (good_start != d->Start)
	       good_parts.push_back(std::make_pair(good_start, goodLength));
	    break;
	 }
	 bad_start = current;
	 --current;
	 // ensure that this is really a comment and not a '#' in the middle of a line
	 if (*current == '\n')
	 {
	    size_t const goodLength = (current - good_start) + 1;
	    d->chunks.emplace_back(true, goodLength);
	    good_parts.push_back(std::make_pair(good_start, goodLength));
	    good_start = nullptr;
	    d->isCommentedLine = true;
	 }
	 current += 2;
      }
      else // the current line is a comment
      {
	 current = static_cast<char*>(memchr(current, '\n', restLength));
	 if (current == nullptr)
	 {
	    d->chunks.emplace_back(false, (d->End - bad_start));
	    break;
	 }
	 ++current;
	 // is the next line a comment, too?
	 if (current >= d->End || *current != '#')
	 {
	    d->chunks.emplace_back(false, (current - bad_start));
	    good_start = current;
	    bad_start = nullptr;
	    d->isCommentedLine = false;
	 }
	 ++current;
      }
   }

   if (good_parts.empty() == false)
   {
      // we found comments, so move later parts over them
      current = d->Start;
      for (auto const &good: good_parts)
      {
	 memmove(current, good.first, good.second);
	 current += good.second;
      }
      d->End = current;
   }

   if (d->isCommentedLine == true)
   {
      // deal with a buffer containing only comments
      // or an (unfinished) comment at the end
      if (good_parts.empty() == true)
	 d->End = d->Start;
      else
	 d->Start = d->End;
   }
   else
   {
      // the buffer was all comment, but ended with the buffer
      if (good_parts.empty() == true && good_start >= d->End)
	 d->End = d->Start;
      else
	 d->Start = d->End;
   }
}
bool pkgTagFile::Fill()
{
   unsigned long long const EndSize = d->End - d->Start;
   if (EndSize != 0)
   {
      memmove(d->Buffer,d->Start,EndSize);
      d->Start = d->End = d->Buffer + EndSize;
   }
   else
      d->Start = d->End = d->Buffer;

   unsigned long long Actual = 0;
   while (d->Done == false && d->Size > (Actual + 1))
   {
      if (FillBuffer(d) == false)
	 return false;
      if ((d->Flags & pkgTagFile::SUPPORT_COMMENTS) != 0)
	 RemoveCommentsFromBuffer(d);
      Actual = d->End - d->Buffer;
   }
   d->Start = d->Buffer;

   if (d->Done == true)
   {
      if (EndSize <= 3 && Actual == 0)
	 return false;
      if (d->Size - (d->End - d->Buffer) < 4)
	 return true;

      // Append a double new line if one does not exist
      unsigned int LineCount = 0;
      for (const char *E = d->End - 1; E - d->End < 6 && (*E == '\n' || *E == '\r'); E--)
	 if (*E == '\n')
	    ++LineCount;
      if (LineCount < 2)
      {
	 if (static_cast<unsigned long long>(d->End - d->Buffer) >= d->Size)
	    Resize(d->Size + 3);
	 for (; LineCount < 2; ++LineCount)
	    *d->End++ = '\n';
      }
   }
   return true;
}
									/*}}}*/
// TagFile::Jump - Jump to a pre-recorded location in the file		/*{{{*/
// ---------------------------------------------------------------------
/* This jumps to a pre-recorded file location and reads the record
   that is there */
bool pkgTagFile::Jump(pkgTagSection &Tag,unsigned long long Offset)
{
   if ((d->Flags & pkgTagFile::SUPPORT_COMMENTS) == 0 &&
   // We are within a buffer space of the next hit..
	 Offset >= d->iOffset && d->iOffset + (d->End - d->Start) > Offset)
   {
      unsigned long long Dist = Offset - d->iOffset;
      d->Start += Dist;
      d->iOffset += Dist;
      // if we have seen the end, don't ask for more
      if (d->Done == true)
	 return Tag.Scan(d->Start, d->End - d->Start);
      else
	 return Step(Tag);
   }

   // Reposition and reload..
   d->iOffset = Offset;
   d->Done = false;
   if (d->Fd->Seek(Offset) == false)
      return false;
   d->End = d->Start = d->Buffer;
   d->isCommentedLine = false;
   d->chunks.clear();

   if (Fill() == false)
      return false;

   if (Tag.Scan(d->Start, d->End - d->Start) == true)
      return true;
   
   // This appends a double new line (for the real eof handling)
   if (Fill() == false)
      return false;
   
   if (Tag.Scan(d->Start, d->End - d->Start, false) == false)
      return _error->Error(_("Unable to parse package file %s (%d)"),d->Fd->Name().c_str(), 2);
   
   return true;
}
									/*}}}*/
// pkgTagSection::pkgTagSection - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgTagSection::pkgTagSection()
   : Section(0), d(new pkgTagSectionPrivate()), Stop(0)
{
   memset(&AlphaIndexes, 0, sizeof(AlphaIndexes));
   memset(&BetaIndexes, 0, sizeof(BetaIndexes));
}
									/*}}}*/
// TagSection::Scan - Scan for the end of the header information	/*{{{*/
bool pkgTagSection::Scan(const char *Start,unsigned long MaxLength, bool const Restart)
{
   Section = Start;
   const char *End = Start + MaxLength;

   if (Restart == false && d->Tags.empty() == false)
   {
      Stop = Section + d->Tags.back().StartTag;
      if (End <= Stop)
	 return false;
      Stop = (const char *)memchr(Stop,'\n',End - Stop);
      if (Stop == NULL)
	 return false;
      ++Stop;
   }
   else
   {
      Stop = Section;
      if (d->Tags.empty() == false)
      {
	 memset(&AlphaIndexes, 0, sizeof(AlphaIndexes));
	 memset(&BetaIndexes, 0, sizeof(BetaIndexes));
	 d->Tags.clear();
      }
      d->Tags.reserve(0x100);
   }
   unsigned int TagCount = d->Tags.size();

   if (Stop == 0)
      return false;

   pkgTagSectionPrivate::TagData lastTagData(0);
   Key lastTagKey = Key::Unknown;
   unsigned int lastTagHash = 0;
   while (Stop < End)
   {
      TrimRecord(true,End);

      // this can happen when TrimRecord trims away the entire Record
      // (e.g. because it just contains comments)
      if(Stop == End)
         return true;

      // Start a new index and add it to the hash
      if (isspace_ascii(Stop[0]) == 0)
      {
	 // store the last found tag
	 if (lastTagData.StartValue != 0)
	 {
	    if (lastTagKey != Key::Unknown) {
	       AlphaIndexes[static_cast<size_t>(lastTagKey)] = TagCount;
	    } else {
	       if (BetaIndexes[lastTagHash] != 0)
		  lastTagData.NextInBucket = BetaIndexes[lastTagHash];
	       BetaIndexes[lastTagHash] = TagCount;
	    }
	    d->Tags.push_back(lastTagData);
	 }

	 ++TagCount;
	 lastTagData = pkgTagSectionPrivate::TagData(Stop - Section);
	 // find the colon separating tag and value
	 char const * Colon = (char const *) memchr(Stop, ':', End - Stop);
	 if (Colon == NULL)
	    return false;
	 // find the end of the tag (which might or might not be the colon)
	 char const * EndTag = Colon;
	 --EndTag;
	 for (; EndTag > Stop && isspace_ascii(*EndTag) != 0; --EndTag)
	    ;
	 ++EndTag;
	 lastTagData.EndTag = EndTag - Section;
	 lastTagKey = pkgTagHash(Stop, EndTag - Stop);
	 if (lastTagKey == Key::Unknown)
	    lastTagHash = BetaHash(Stop, EndTag - Stop);
	 // find the beginning of the value
	 Stop = Colon + 1;
	 for (; Stop < End && isspace_ascii(*Stop) != 0; ++Stop)
	    if (*Stop == '\n' && Stop[1] != ' ')
	       break;
	 if (Stop >= End)
	    return false;
	 lastTagData.StartValue = Stop - Section;
      }

      Stop = (const char *)memchr(Stop,'\n',End - Stop);

      if (Stop == 0)
	 return false;

      for (; Stop+1 < End && Stop[1] == '\r'; Stop++)
         /* nothing */
         ;

      // Double newline marks the end of the record
      if (Stop+1 < End && Stop[1] == '\n')
      {
	 if (lastTagData.StartValue != 0)
	 {
	    if (lastTagKey != Key::Unknown) {
	       AlphaIndexes[static_cast<size_t>(lastTagKey)] = TagCount;
	    } else {
	       if (BetaIndexes[lastTagHash] != 0)
		  lastTagData.NextInBucket = BetaIndexes[lastTagHash];
	       BetaIndexes[lastTagHash] = TagCount;
	    }
	    d->Tags.push_back(lastTagData);
	 }

	 pkgTagSectionPrivate::TagData const td(Stop - Section);
	 d->Tags.push_back(td);
	 TrimRecord(false,End);
	 return true;
      }
      
      Stop++;
   }

   return false;
}
									/*}}}*/
// TagSection::TrimRecord - Trim off any garbage before/after a record	/*{{{*/
// ---------------------------------------------------------------------
/* There should be exactly 2 newline at the end of the record, no more. */
void pkgTagSection::TrimRecord(bool BeforeRecord, const char*& End)
{
   if (BeforeRecord == true)
      return;
   for (; Stop < End && (Stop[0] == '\n' || Stop[0] == '\r'); Stop++);
}
									/*}}}*/
// TagSection::Trim - Trim off any trailing garbage			/*{{{*/
// ---------------------------------------------------------------------
/* There should be exactly 1 newline at the end of the buffer, no more. */
void pkgTagSection::Trim()
{
   for (; Stop > Section + 2 && (Stop[-2] == '\n' || Stop[-2] == '\r'); Stop--);
}
									/*}}}*/
// TagSection::Exists - return True if a tag exists			/*{{{*/
bool pkgTagSection::Exists(StringView Tag) const
{
   unsigned int tmp;
   return Find(Tag, tmp);
}
									/*}}}*/
// TagSection::Find - Locate a tag					/*{{{*/
// ---------------------------------------------------------------------
/* This searches the section for a tag that matches the given string. */
bool pkgTagSection::Find(Key key,unsigned int &Pos) const
{
   auto Bucket = AlphaIndexes[static_cast<size_t>(key)];
   Pos = Bucket - 1;
   return Bucket != 0;
}
bool pkgTagSection::Find(StringView TagView,unsigned int &Pos) const
{
   const char * const Tag = TagView.data();
   size_t const Length = TagView.length();
   auto key = pkgTagHash(Tag, Length);
   if (key != Key::Unknown)
      return Find(key, Pos);

   unsigned int Bucket = BetaIndexes[BetaHash(Tag, Length)];
   if (Bucket == 0)
      return false;

   for (; Bucket != 0; Bucket = d->Tags[Bucket - 1].NextInBucket)
   {
      if ((d->Tags[Bucket - 1].EndTag - d->Tags[Bucket - 1].StartTag) != Length)
	 continue;

      char const * const St = Section + d->Tags[Bucket - 1].StartTag;
      if (strncasecmp(Tag,St,Length) != 0)
	 continue;

      Pos = Bucket - 1;
      return true;
   }

   Pos = 0;
   return false;
}

bool pkgTagSection::FindInternal(unsigned int Pos, const char *&Start,
		         const char *&End) const
{
   if (unlikely(Pos + 1 >= d->Tags.size() || Pos >= d->Tags.size()))
      return _error->Error("Internal parsing error");

   Start = Section + d->Tags[Pos].StartValue;
   // Strip off the gunk from the end
   End = Section + d->Tags[Pos + 1].StartTag;
   if (unlikely(Start > End))
      return _error->Error("Internal parsing error");

   for (; isspace_ascii(End[-1]) != 0 && End > Start; --End);

   return true;
}
bool pkgTagSection::Find(StringView Tag,const char *&Start,
		         const char *&End) const
{
   unsigned int Pos;
   return Find(Tag, Pos) && FindInternal(Pos, Start, End);
}
bool pkgTagSection::Find(Key key,const char *&Start,
		         const char *&End) const
{
   unsigned int Pos;
   return Find(key, Pos) && FindInternal(Pos, Start, End);
}
									/*}}}*/
// TagSection::FindS - Find a string					/*{{{*/
StringView pkgTagSection::Find(StringView Tag) const
{
   const char *Start;
   const char *End;
   if (Find(Tag,Start,End) == false)
      return StringView();
   return StringView(Start, End - Start);
}
StringView pkgTagSection::Find(Key key) const
{
   const char *Start;
   const char *End;
   if (Find(key,Start,End) == false)
      return StringView();
   return StringView(Start, End - Start);
}
									/*}}}*/
// TagSection::FindRawS - Find a string					/*{{{*/
StringView pkgTagSection::FindRawInternal(unsigned int Pos) const
{
   if (unlikely(Pos + 1 >= d->Tags.size() || Pos >= d->Tags.size()))
      return _error->Error("Internal parsing error"), "";

   char const *Start = (char const *) memchr(Section + d->Tags[Pos].EndTag, ':', d->Tags[Pos].StartValue - d->Tags[Pos].EndTag);
   char const *End = Section + d->Tags[Pos + 1].StartTag;

   if (Start == nullptr)
      return "";

   ++Start;

   if (unlikely(Start > End))
      return "";

   for (; isspace_ascii(End[-1]) != 0 && End > Start; --End);

   return StringView(Start, End - Start);
}
StringView pkgTagSection::FindRaw(StringView Tag) const
{
   unsigned int Pos;
   return Find(Tag, Pos) ? FindRawInternal(Pos) : "";
}
StringView pkgTagSection::FindRaw(Key key) const
{
   unsigned int Pos;
   return Find(key, Pos) ? FindRawInternal(Pos) : "";
}
									/*}}}*/
// TagSection::FindI - Find an integer					/*{{{*/
// ---------------------------------------------------------------------
/* */
signed int pkgTagSection::FindIInternal(unsigned int Pos,signed long Default) const
{
   const char *Start;
   const char *Stop;
   if (FindInternal(Pos,Start,Stop) == false)
      return Default;

   // Copy it into a temp buffer so we can use strtol
   char S[300];
   if ((unsigned)(Stop - Start) >= sizeof(S))
      return Default;
   strncpy(S,Start,Stop-Start);
   S[Stop - Start] = 0;

   errno = 0;
   char *End;
   signed long Result = strtol(S,&End,10);
   if (errno == ERANGE ||
       Result < std::numeric_limits<int>::min() || Result > std::numeric_limits<int>::max()) {
      errno = ERANGE;
      _error->Error(_("Cannot convert %s to integer: out of range"), S);
   }
   if (S == End)
      return Default;
   return Result;
}
signed int pkgTagSection::FindI(Key key,signed long Default) const
{
   unsigned int Pos;

   return Find(key, Pos) ? FindIInternal(Pos) : Default;
}
signed int pkgTagSection::FindI(StringView Tag,signed long Default) const
{
   unsigned int Pos;

   return Find(Tag, Pos) ? FindIInternal(Pos, Default) : Default;
}
									/*}}}*/
// TagSection::FindULL - Find an unsigned long long integer		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long pkgTagSection::FindULLInternal(unsigned int Pos, unsigned long long const &Default) const
{
   const char *Start;
   const char *Stop;
   if (FindInternal(Pos,Start,Stop) == false)
      return Default;

   // Copy it into a temp buffer so we can use strtoull
   char S[100];
   if ((unsigned)(Stop - Start) >= sizeof(S))
      return Default;
   strncpy(S,Start,Stop-Start);
   S[Stop - Start] = 0;
   
   char *End;
   unsigned long long Result = strtoull(S,&End,10);
   if (S == End)
      return Default;
   return Result;
}
unsigned long long pkgTagSection::FindULL(Key key, unsigned long long const &Default) const
{
   unsigned int Pos;

   return Find(key, Pos) ? FindULLInternal(Pos, Default) : Default;
}
unsigned long long pkgTagSection::FindULL(StringView Tag, unsigned long long const &Default) const
{
   unsigned int Pos;

   return Find(Tag, Pos) ? FindULLInternal(Pos, Default) : Default;
}
									/*}}}*/
// TagSection::FindB - Find boolean value                		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgTagSection::FindBInternal(unsigned int Pos, bool Default) const
{
   const char *Start, *Stop;
   if (FindInternal(Pos, Start, Stop) == false)
      return Default;
   return StringToBool(string(Start, Stop));
}
bool pkgTagSection::FindB(Key key, bool Default) const
{
   unsigned int Pos;
   return Find(key, Pos) ? FindBInternal(Pos, Default): Default;
}
bool pkgTagSection::FindB(StringView Tag, bool Default) const
{
   unsigned int Pos;
   return Find(Tag, Pos) ? FindBInternal(Pos, Default) : Default;
}
									/*}}}*/
// TagSection::FindFlag - Locate a yes/no type flag			/*{{{*/
// ---------------------------------------------------------------------
/* The bits marked in Flag are masked on/off in Flags */
bool pkgTagSection::FindFlagInternal(unsigned int Pos, uint8_t &Flags,
			     uint8_t const Flag) const
{
   const char *Start;
   const char *Stop;
   if (FindInternal(Pos,Start,Stop) == false)
      return true;
   return FindFlag(Flags, Flag, Start, Stop);
}
bool pkgTagSection::FindFlag(Key key, uint8_t &Flags,
			     uint8_t const Flag) const
{
   unsigned int Pos;
   if (Find(key,Pos) == false)
      return true;
   return FindFlagInternal(Pos, Flags, Flag);
}
bool pkgTagSection::FindFlag(StringView Tag, uint8_t &Flags,
			     uint8_t const Flag) const
{
   unsigned int Pos;
   if (Find(Tag,Pos) == false)
      return true;
   return FindFlagInternal(Pos, Flags, Flag);
}
bool pkgTagSection::FindFlag(uint8_t &Flags, uint8_t const Flag,
					char const* const Start, char const* const Stop)
{
   switch (StringToBool(string(Start, Stop)))
   {
      case 0:
      Flags &= ~Flag;
      return true;

      case 1:
      Flags |= Flag;
      return true;

      default:
      _error->Warning("Unknown flag value: %s",string(Start,Stop).c_str());
      return true;
   }
   return true;
}
bool pkgTagSection::FindFlagInternal(unsigned int Pos,unsigned long &Flags,
			     unsigned long Flag) const
{
   const char *Start;
   const char *Stop;
   if (FindInternal(Pos,Start,Stop) == false)
      return true;
   return FindFlag(Flags, Flag, Start, Stop);
}
bool pkgTagSection::FindFlag(Key key,unsigned long &Flags,
			     unsigned long Flag) const
{
   unsigned int Pos;
   return Find(key, Pos) ? FindFlagInternal(Pos, Flags, Flag) : true;
}
bool pkgTagSection::FindFlag(StringView Tag,unsigned long &Flags,
			     unsigned long Flag) const
{
   unsigned int Pos;
   return Find(Tag, Pos) ? FindFlagInternal(Pos, Flags, Flag) : true;
}
bool pkgTagSection::FindFlag(unsigned long &Flags, unsigned long Flag,
					char const* Start, char const* Stop)
{
   switch (StringToBool(string(Start, Stop)))
   {
      case 0:
      Flags &= ~Flag;
      return true;

      case 1:
      Flags |= Flag;
      return true;

      default:
      _error->Warning("Unknown flag value: %s",string(Start,Stop).c_str());
      return true;
   }
   return true;
}
									/*}}}*/
void pkgTagSection::Get(const char *&Start,const char *&Stop,unsigned int I) const/*{{{*/
{
   if (unlikely(I + 1 >= d->Tags.size() || I >= d->Tags.size()))
      abort();
   Start = Section + d->Tags[I].StartTag;
   Stop = Section + d->Tags[I+1].StartTag;
}
									/*}}}*/
APT_PURE unsigned int pkgTagSection::Count() const {			/*{{{*/
   if (d->Tags.empty() == true)
      return 0;
   // the last element is just marking the end and isn't a real one
   return d->Tags.size() - 1;
}
									/*}}}*/
// TagSection::Write - Ordered (re)writing of fields			/*{{{*/
pkgTagSection::Tag pkgTagSection::Tag::Remove(std::string const &Name)
{
   return Tag(REMOVE, Name, "");
}
pkgTagSection::Tag pkgTagSection::Tag::Rename(std::string const &OldName, std::string const &NewName)
{
   return Tag(RENAME, OldName, NewName);
}
pkgTagSection::Tag pkgTagSection::Tag::Rewrite(std::string const &Name, std::string const &Data)
{
   if (Data.empty() == true)
      return Tag(REMOVE, Name, "");
   else
      return Tag(REWRITE, Name, Data);
}
static bool WriteTag(FileFd &File, std::string Tag, StringView Value)
{
   if (Value.empty() || isspace_ascii(Value[0]) != 0)
      Tag.append(":");
   else
      Tag.append(": ");
   Tag.append(Value.data(), Value.length());
   Tag.append("\n");
   return File.Write(Tag.c_str(), Tag.length());
}
static bool RewriteTags(FileFd &File, pkgTagSection const * const This, char const * const Tag,
      std::vector<pkgTagSection::Tag>::const_iterator &R,
      std::vector<pkgTagSection::Tag>::const_iterator const &REnd)
{
   size_t const TagLen = strlen(Tag);
   for (; R != REnd; ++R)
   {
      std::string data;
      if (R->Name.length() == TagLen && strncasecmp(R->Name.c_str(), Tag, R->Name.length()) == 0)
      {
	 if (R->Action != pkgTagSection::Tag::REWRITE)
	    break;
	 data = R->Data;
      }
      else if(R->Action == pkgTagSection::Tag::RENAME && R->Data.length() == TagLen &&
	    strncasecmp(R->Data.c_str(), Tag, R->Data.length()) == 0)
	 data = This->FindRaw(R->Name.c_str()).to_string();
      else
	 continue;

      return WriteTag(File, Tag, data);
   }
   return true;
}
bool pkgTagSection::Write(FileFd &File, char const * const * const Order, std::vector<Tag> const &Rewrite) const
{
   // first pass: Write everything we have an order for
   if (Order != NULL)
   {
      for (unsigned int I = 0; Order[I] != 0; ++I)
      {
	 std::vector<Tag>::const_iterator R = Rewrite.begin();
	 if (RewriteTags(File, this, Order[I], R, Rewrite.end()) == false)
	    return false;
	 if (R != Rewrite.end())
	    continue;

	 if (Exists(Order[I]) == false)
	    continue;

	 if (WriteTag(File, Order[I], FindRaw(Order[I])) == false)
	    return false;
      }
   }
   // second pass: See if we have tags which aren't ordered
   if (d->Tags.empty() == false)
   {
      for (std::vector<pkgTagSectionPrivate::TagData>::const_iterator T = d->Tags.begin(); T != d->Tags.end() - 1; ++T)
      {
	 char const * const fieldname = Section + T->StartTag;
	 size_t fieldnamelen = T->EndTag - T->StartTag;
	 if (Order != NULL)
	 {
	    unsigned int I = 0;
	    for (; Order[I] != 0; ++I)
	    {
	       if (fieldnamelen == strlen(Order[I]) && strncasecmp(fieldname, Order[I], fieldnamelen) == 0)
		  break;
	    }
	    if (Order[I] != 0)
	       continue;
	 }

	 std::string const name(fieldname, fieldnamelen);
	 std::vector<Tag>::const_iterator R = Rewrite.begin();
	 if (RewriteTags(File, this, name.c_str(), R, Rewrite.end()) == false)
	    return false;
	 if (R != Rewrite.end())
	    continue;

	 if (WriteTag(File, name, FindRaw(name)) == false)
	    return false;
      }
   }
   // last pass: see if there are any rewrites remaining we haven't done yet
   for (std::vector<Tag>::const_iterator R = Rewrite.begin(); R != Rewrite.end(); ++R)
   {
      if (R->Action == Tag::REMOVE)
	 continue;
      std::string const name = ((R->Action == Tag::RENAME) ? R->Data : R->Name);
      if (Exists(name.c_str()))
	 continue;
      if (Order != NULL)
      {
	 unsigned int I = 0;
	 for (; Order[I] != 0; ++I)
	 {
	    if (strncasecmp(name.c_str(), Order[I], name.length()) == 0 && name.length() == strlen(Order[I]))
	       break;
	 }
	 if (Order[I] != 0)
	    continue;
      }

      if (WriteTag(File, name, ((R->Action == Tag::RENAME) ? FindRaw(R->Name) : R->Data)) == false)
	 return false;
   }
   return true;
}
									/*}}}*/

#include "tagfile-order.c"

pkgTagSection::~pkgTagSection() { delete d; }
