// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.cc,v 1.14 1998/11/13 04:23:36 jgg Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This uses a rotating buffer to load the package information into.
   The scanner runs over it and isolates and indexes a single section.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/tagfile.h"
#endif

#include <apt-pkg/tagfile.h>
#include <apt-pkg/error.h>

#include <string>
#include <stdio.h>
									/*}}}*/

// TagFile::pkgTagFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgTagFile::pkgTagFile(FileFd &Fd,unsigned long Size) : Fd(Fd), Size(Size)
{
   Buffer = new char[Size];
   Start = End = Buffer;
   Left = Fd.Size();
   iOffset = 0;
   Fill();
}
									/*}}}*/
// TagFile::Step - Advance to the next section				/*{{{*/
// ---------------------------------------------------------------------
/* If the Section Scanner fails we refill the buffer and try again. */
bool pkgTagFile::Step(pkgTagSection &Tag)
{
   if (Tag.Scan(Start,End - Start) == false)
   {
      if (Fill() == false)
	 return false;
      
      if (Tag.Scan(Start,End - Start) == false)
	 return _error->Error("Unable to parse package file");
   }   
   Start += Tag.size();
   iOffset += Tag.size();
   
   return true;
}
									/*}}}*/
// TagFile::Fill - Top up the buffer					/*{{{*/
// ---------------------------------------------------------------------
/* This takes the bit at the end of the buffer and puts it at the start
   then fills the rest from the file */
bool pkgTagFile::Fill()
{
   unsigned long EndSize = End - Start;
   
   if (Left == 0)
   {
      if (EndSize <= 1)
	 return false;
      return true;
   }
   
   memmove(Buffer,Start,EndSize);
   Start = Buffer;
   End = Buffer + EndSize;
   
   // See if only a bit of the file is left
   if (Left < Size - (End - Buffer))
   {
      if (Fd.Read(End,Left) == false)
	 return false;
      
      End += Left;
      Left = 0;
   }
   else
   {
      if (Fd.Read(End,Size - (End - Buffer)) == false)
	 return false;
      
      Left -= Size - (End - Buffer);
      End = Buffer + Size;
   }   
   return true;
}
									/*}}}*/
// TagFile::Jump - Jump to a pre-recorded location in the file		/*{{{*/
// ---------------------------------------------------------------------
/* This jumps to a pre-recorded file location and reads the record
   that is there */
bool pkgTagFile::Jump(pkgTagSection &Tag,unsigned long Offset)
{
   iOffset = Offset;
   Left = Fd.Size() - Offset;
   if (Fd.Seek(Offset) == false)
      return false;
   End = Start = Buffer;
   
   if (Fill() == false)
      return false;
   
   if (Tag.Scan(Start,End - Start) == false)
      return _error->Error("Unable to parse package file");
   return true;
}
									/*}}}*/
// TagSection::Scan - Scan for the end of the header information	/*{{{*/
// ---------------------------------------------------------------------
/* This looks for the first double new line in the data stream. It also
   indexes the tags in the section. */
bool pkgTagSection::Scan(const char *Start,unsigned long MaxLength)
{
   const char *End = Start + MaxLength;
   Stop = Section = Start;
   
   TagCount = 0;
   Indexes[TagCount++] = Stop - Section;
   Stop++;
   for (; Stop < End; Stop++)
   {
      if (Stop[-1] != '\n')
	 continue;

      // Skip line feeds
      for (; Stop[0] == '\r' && Stop < End; Stop++);
      
      if (Stop[0] == '\n')
      {
	 // Extra one at the end to simplify find
	 Indexes[TagCount] = Stop - Section;
	 for (; (Stop[0] == '\n' || Stop[0] == '\r') && Stop < End; Stop++);
	 return true;
      }
      
      if (isspace(Stop[0]) == 0)
	 Indexes[TagCount++] = Stop - Section;
      
      // Just in case.
      if (TagCount > sizeof(Indexes)/sizeof(Indexes[0]))
	 TagCount = sizeof(Indexes)/sizeof(Indexes[0]);
   }   
   return false;
}
									/*}}}*/
// TagSection::Find - Locate a tag					/*{{{*/
// ---------------------------------------------------------------------
/* This searches the section for a tag that matches the given string. */
bool pkgTagSection::Find(const char *Tag,const char *&Start,
		         const char *&End)
{
   unsigned int Length = strlen(Tag);
   for (unsigned int I = 0; I != TagCount; I++)
   {
      if (strncasecmp(Tag,Section + Indexes[I],Length) != 0)
	 continue;

      // Make sure the colon is in the right place
      const char *C = Section + Length + Indexes[I];
      for (; isspace(*C) != 0; C++);
      if (*C != ':')
	 continue;

      // Strip off the gunk from the start end
      Start = C;
      End = Section + Indexes[I+1];
      for (; (isspace(*Start) != 0 || *Start == ':') && Start < End; Start++);
      for (; isspace(End[-1]) != 0 && End > Start; End--);
      
      return true;
   }
   Start = End = 0;
   return false;
}
									/*}}}*/
