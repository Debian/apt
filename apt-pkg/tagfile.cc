// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.cc,v 1.37.2.2 2003/12/31 16:02:30 mdz Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This uses a rotating buffer to load the package information into.
   The scanner runs over it and isolates and indexes a single section.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/tagfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <stdio.h>
#include <ctype.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

class pkgTagFilePrivate
{
public:
   pkgTagFilePrivate(FileFd *pFd, unsigned long long Size) : Fd(*pFd), Buffer(NULL),
							     Start(NULL), End(NULL),
							     Done(false), iOffset(0),
							     Size(Size)
   {
   }
   FileFd &Fd;
   char *Buffer;
   char *Start;
   char *End;
   bool Done;
   unsigned long long iOffset;
   unsigned long long Size;
};

// TagFile::pkgTagFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgTagFile::pkgTagFile(FileFd *pFd,unsigned long long Size)
{
   d = new pkgTagFilePrivate(pFd, Size);

   if (d->Fd.IsOpen() == false)
   {
      d->Start = d->End = d->Buffer = 0;
      d->Done = true;
      d->iOffset = 0;
      return;
   }
   
   d->Buffer = new char[Size];
   d->Start = d->End = d->Buffer;
   d->Done = false;
   d->iOffset = 0;
   Fill();
}
									/*}}}*/
// TagFile::~pkgTagFile - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgTagFile::~pkgTagFile()
{
   delete [] d->Buffer;
   delete d;
}
									/*}}}*/
// TagFile::Offset - Return the current offset in the buffer     	/*{{{*/
unsigned long pkgTagFile::Offset()
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
   char *tmp;
   unsigned long long EndSize = d->End - d->Start;

   // fail is the buffer grows too big
   if(d->Size > 1024*1024+1)
      return false;

   // get new buffer and use it
   tmp = new char[2*d->Size];
   memcpy(tmp, d->Buffer, d->Size);
   d->Size = d->Size*2;
   delete [] d->Buffer;
   d->Buffer = tmp;

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
   while (Tag.Scan(d->Start,d->End - d->Start) == false)
   {
      if (Fill() == false)
	 return false;
      
      if(Tag.Scan(d->Start,d->End - d->Start))
	 break;

      if (Resize() == false)
	 return _error->Error(_("Unable to parse package file %s (1)"),
                              d->Fd.Name().c_str());
   }
   d->Start += Tag.size();
   d->iOffset += Tag.size();

   Tag.Trim();
   return true;
}
									/*}}}*/
// TagFile::Fill - Top up the buffer					/*{{{*/
// ---------------------------------------------------------------------
/* This takes the bit at the end of the buffer and puts it at the start
   then fills the rest from the file */
bool pkgTagFile::Fill()
{
   unsigned long long EndSize = d->End - d->Start;
   unsigned long long Actual = 0;
   
   memmove(d->Buffer,d->Start,EndSize);
   d->Start = d->Buffer;
   d->End = d->Buffer + EndSize;
   
   if (d->Done == false)
   {
      // See if only a bit of the file is left
      if (d->Fd.Read(d->End, d->Size - (d->End - d->Buffer),&Actual) == false)
	 return false;
      if (Actual != d->Size - (d->End - d->Buffer))
	 d->Done = true;
      d->End += Actual;
   }
   
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
	    LineCount++;
      for (; LineCount < 2; LineCount++)
	 *d->End++ = '\n';
      
      return true;
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
   // We are within a buffer space of the next hit..
   if (Offset >= d->iOffset && d->iOffset + (d->End - d->Start) > Offset)
   {
      unsigned long long Dist = Offset - d->iOffset;
      d->Start += Dist;
      d->iOffset += Dist;
      return Step(Tag);
   }

   // Reposition and reload..
   d->iOffset = Offset;
   d->Done = false;
   if (d->Fd.Seek(Offset) == false)
      return false;
   d->End = d->Start = d->Buffer;
   
   if (Fill() == false)
      return false;

   if (Tag.Scan(d->Start, d->End - d->Start) == true)
      return true;
   
   // This appends a double new line (for the real eof handling)
   if (Fill() == false)
      return false;
   
   if (Tag.Scan(d->Start, d->End - d->Start) == false)
      return _error->Error(_("Unable to parse package file %s (2)"),d->Fd.Name().c_str());
   
   return true;
}
									/*}}}*/
// TagSection::Scan - Scan for the end of the header information	/*{{{*/
// ---------------------------------------------------------------------
/* This looks for the first double new line in the data stream.
   It also indexes the tags in the section. */
bool pkgTagSection::Scan(const char *Start,unsigned long MaxLength)
{
   const char *End = Start + MaxLength;
   Stop = Section = Start;
   memset(AlphaIndexes,0,sizeof(AlphaIndexes));

   if (Stop == 0)
      return false;

   TagCount = 0;
   while (TagCount+1 < sizeof(Indexes)/sizeof(Indexes[0]) && Stop < End)
   {
       TrimRecord(true,End);

      // Start a new index and add it to the hash
      if (isspace(Stop[0]) == 0)
      {
	 Indexes[TagCount++] = Stop - Section;
	 AlphaIndexes[AlphaHash(Stop,End)] = TagCount;
      }

      Stop = (const char *)memchr(Stop,'\n',End - Stop);
      
      if (Stop == 0)
	 return false;

      for (; Stop+1 < End && Stop[1] == '\r'; Stop++);

      // Double newline marks the end of the record
      if (Stop+1 < End && Stop[1] == '\n')
      {
	 Indexes[TagCount] = Stop - Section;
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
// TagSection::Exists - return True if a tag exists                	/*{{{*/
bool pkgTagSection::Exists(const char* const Tag)
{
   unsigned int tmp;
   return Find(Tag, tmp);
}
									/*}}}*/
// TagSection::Find - Locate a tag					/*{{{*/
// ---------------------------------------------------------------------
/* This searches the section for a tag that matches the given string. */
bool pkgTagSection::Find(const char *Tag,unsigned int &Pos) const
{
   unsigned int Length = strlen(Tag);
   unsigned int I = AlphaIndexes[AlphaHash(Tag)];
   if (I == 0)
      return false;
   I--;
   
   for (unsigned int Counter = 0; Counter != TagCount; Counter++, 
	I = (I+1)%TagCount)
   {
      const char *St;
      St = Section + Indexes[I];
      if (strncasecmp(Tag,St,Length) != 0)
	 continue;

      // Make sure the colon is in the right place
      const char *C = St + Length;
      for (; isspace(*C) != 0; C++);
      if (*C != ':')
	 continue;
      Pos = I;
      return true;
   }

   Pos = 0;
   return false;
}
									/*}}}*/
// TagSection::Find - Locate a tag					/*{{{*/
// ---------------------------------------------------------------------
/* This searches the section for a tag that matches the given string. */
bool pkgTagSection::Find(const char *Tag,const char *&Start,
		         const char *&End) const
{
   unsigned int Length = strlen(Tag);
   unsigned int I = AlphaIndexes[AlphaHash(Tag)];
   if (I == 0)
      return false;
   I--;
   
   for (unsigned int Counter = 0; Counter != TagCount; Counter++, 
	I = (I+1)%TagCount)
   {
      const char *St;
      St = Section + Indexes[I];
      if (strncasecmp(Tag,St,Length) != 0)
	 continue;
      
      // Make sure the colon is in the right place
      const char *C = St + Length;
      for (; isspace(*C) != 0; C++);
      if (*C != ':')
	 continue;

      // Strip off the gunk from the start end
      Start = C;
      End = Section + Indexes[I+1];
      if (Start >= End)
	 return _error->Error("Internal parsing error");
      
      for (; (isspace(*Start) != 0 || *Start == ':') && Start < End; Start++);
      for (; isspace(End[-1]) != 0 && End > Start; End--);
      
      return true;
   }
   
   Start = End = 0;
   return false;
}
									/*}}}*/
// TagSection::FindS - Find a string					/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgTagSection::FindS(const char *Tag) const
{
   const char *Start;
   const char *End;
   if (Find(Tag,Start,End) == false)
      return string();
   return string(Start,End);      
}
									/*}}}*/
// TagSection::FindI - Find an integer					/*{{{*/
// ---------------------------------------------------------------------
/* */
signed int pkgTagSection::FindI(const char *Tag,signed long Default) const
{
   const char *Start;
   const char *Stop;
   if (Find(Tag,Start,Stop) == false)
      return Default;

   // Copy it into a temp buffer so we can use strtol
   char S[300];
   if ((unsigned)(Stop - Start) >= sizeof(S))
      return Default;
   strncpy(S,Start,Stop-Start);
   S[Stop - Start] = 0;
   
   char *End;
   signed long Result = strtol(S,&End,10);
   if (S == End)
      return Default;
   return Result;
}
									/*}}}*/
// TagSection::FindULL - Find an unsigned long long integer		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long pkgTagSection::FindULL(const char *Tag, unsigned long long const &Default) const
{
   const char *Start;
   const char *Stop;
   if (Find(Tag,Start,Stop) == false)
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
									/*}}}*/
// TagSection::FindFlag - Locate a yes/no type flag			/*{{{*/
// ---------------------------------------------------------------------
/* The bits marked in Flag are masked on/off in Flags */
bool pkgTagSection::FindFlag(const char *Tag,unsigned long &Flags,
			     unsigned long Flag) const
{
   const char *Start;
   const char *Stop;
   if (Find(Tag,Start,Stop) == false)
      return true;
   return FindFlag(Flags, Flag, Start, Stop);
}
bool const pkgTagSection::FindFlag(unsigned long &Flags, unsigned long Flag,
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
// TFRewrite - Rewrite a control record					/*{{{*/
// ---------------------------------------------------------------------
/* This writes the control record to stdout rewriting it as necessary. The
   override map item specificies the rewriting rules to follow. This also
   takes the time to sort the feild list. */

/* The order of this list is taken from dpkg source lib/parse.c the fieldinfos
   array. */
static const char *iTFRewritePackageOrder[] = {
                          "Package",
                          "Essential",
                          "Status",
                          "Priority",
                          "Section",
                          "Installed-Size",
                          "Maintainer",
                          "Original-Maintainer",
                          "Architecture",
                          "Source",
                          "Version",
                           "Revision",         // Obsolete
                           "Config-Version",   // Obsolete
                          "Replaces",
                          "Provides",
                          "Depends",
                          "Pre-Depends",
                          "Recommends",
                          "Suggests",
                          "Conflicts",
                          "Breaks",
                          "Conffiles",
                          "Filename",
                          "Size",
                          "MD5Sum",
                          "SHA1",
                          "SHA256",
                          "SHA512",
                           "MSDOS-Filename",   // Obsolete
                          "Description",
                          0};
static const char *iTFRewriteSourceOrder[] = {"Package",
                                      "Source",
                                      "Binary",
                                      "Version",
                                      "Priority",
                                      "Section",
                                      "Maintainer",
				      "Original-Maintainer",
                                      "Build-Depends",
                                      "Build-Depends-Indep",
                                      "Build-Conflicts",
                                      "Build-Conflicts-Indep",
                                      "Architecture",
                                      "Standards-Version",
                                      "Format",
                                      "Directory",
                                      "Files",
                                      0};   

/* Two levels of initialization are used because gcc will set the symbol
   size of an array to the length of the array, causing dynamic relinking 
   errors. Doing this makes the symbol size constant */
const char **TFRewritePackageOrder = iTFRewritePackageOrder;
const char **TFRewriteSourceOrder = iTFRewriteSourceOrder;
   
bool TFRewrite(FILE *Output,pkgTagSection const &Tags,const char *Order[],
	       TFRewriteData *Rewrite)
{
   unsigned char Visited[256];   // Bit 1 is Order, Bit 2 is Rewrite
   for (unsigned I = 0; I != 256; I++)
      Visited[I] = 0;

   // Set new tag up as necessary.
   for (unsigned int J = 0; Rewrite != 0 && Rewrite[J].Tag != 0; J++)
   {
      if (Rewrite[J].NewTag == 0)
	 Rewrite[J].NewTag = Rewrite[J].Tag;
   }
   
   // Write all all of the tags, in order.
   for (unsigned int I = 0; Order[I] != 0; I++)
   {
      bool Rewritten = false;
      
      // See if this is a field that needs to be rewritten
      for (unsigned int J = 0; Rewrite != 0 && Rewrite[J].Tag != 0; J++)
      {
	 if (strcasecmp(Rewrite[J].Tag,Order[I]) == 0)
	 {
	    Visited[J] |= 2;
	    if (Rewrite[J].Rewrite != 0 && Rewrite[J].Rewrite[0] != 0)
	    {
	       if (isspace(Rewrite[J].Rewrite[0]))
		  fprintf(Output,"%s:%s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
	       else
		  fprintf(Output,"%s: %s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
	    }
	    
	    Rewritten = true;
	    break;
	 }
      }      
	    
      // See if it is in the fragment
      unsigned Pos;
      if (Tags.Find(Order[I],Pos) == false)
	 continue;
      Visited[Pos] |= 1;

      if (Rewritten == true)
	 continue;
      
      /* Write out this element, taking a moment to rewrite the tag
         in case of changes of case. */
      const char *Start;
      const char *Stop;
      Tags.Get(Start,Stop,Pos);
      
      if (fputs(Order[I],Output) < 0)
	 return _error->Errno("fputs","IO Error to output");
      Start += strlen(Order[I]);
      if (fwrite(Start,Stop - Start,1,Output) != 1)
	 return _error->Errno("fwrite","IO Error to output");
      if (Stop[-1] != '\n')
	 fprintf(Output,"\n");
   }   

   // Now write all the old tags that were missed.
   for (unsigned int I = 0; I != Tags.Count(); I++)
   {
      if ((Visited[I] & 1) == 1)
	 continue;

      const char *Start;
      const char *Stop;
      Tags.Get(Start,Stop,I);
      const char *End = Start;
      for (; End < Stop && *End != ':'; End++);

      // See if this is a field that needs to be rewritten
      bool Rewritten = false;
      for (unsigned int J = 0; Rewrite != 0 && Rewrite[J].Tag != 0; J++)
      {
	 if (stringcasecmp(Start,End,Rewrite[J].Tag) == 0)
	 {
	    Visited[J] |= 2;
	    if (Rewrite[J].Rewrite != 0 && Rewrite[J].Rewrite[0] != 0)
	    {
	       if (isspace(Rewrite[J].Rewrite[0]))
		  fprintf(Output,"%s:%s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
	       else
		  fprintf(Output,"%s: %s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
	    }
	    
	    Rewritten = true;
	    break;
	 }
      }      
      
      if (Rewritten == true)
	 continue;
      
      // Write out this element
      if (fwrite(Start,Stop - Start,1,Output) != 1)
	 return _error->Errno("fwrite","IO Error to output");
      if (Stop[-1] != '\n')
	 fprintf(Output,"\n");
   }
   
   // Now write all the rewrites that were missed
   for (unsigned int J = 0; Rewrite != 0 && Rewrite[J].Tag != 0; J++)
   {
      if ((Visited[J] & 2) == 2)
	 continue;
      
      if (Rewrite[J].Rewrite != 0 && Rewrite[J].Rewrite[0] != 0)
      {
	 if (isspace(Rewrite[J].Rewrite[0]))
	    fprintf(Output,"%s:%s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
	 else
	    fprintf(Output,"%s: %s\n",Rewrite[J].NewTag,Rewrite[J].Rewrite);
      }      
   }
      
   return true;
}
									/*}}}*/
