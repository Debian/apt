// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.cc,v 1.1 1998/07/02 02:58:13 jgg Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information
   
   This uses a rotating 64K buffer to load the package information into.
   The scanner runs over it and isolates and indexes a single section.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <pkglib/tagfile.h>
#include <pkglib/error.h>

#include <string>
#include <stdio.h>
									/*}}}*/

// TagFile::pkgTagFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgTagFile::pkgTagFile(File &Fd) : Fd(Fd)
{
   Buffer = new char[64*1024];
   Start = End = Buffer + 64*1024;
   Left = Fd.Size();
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
   Start += Tag.Length();
   return true;
}
									/*}}}*/
// TagFile::Fill - Top up the buffer					/*{{{*/
// ---------------------------------------------------------------------
/* This takes the bit at the end of the buffer and puts it at the start
   then fills the rest from the file */
bool pkgTagFile::Fill()
{
   unsigned long Size = End - Start;
   
   if (Left == 0)
   {
      if (Size <= 1)
	 return false;
      return true;
   }
   
   memmove(Buffer,Start,Size);
   Start = Buffer;
   
   // See if only a bit of the file is left or if 
   if (Left < End - Buffer - Size)
   {
      if (Fd.Read(Buffer + Size,Left) == false)
	 return false;
      End = Buffer + Size + Left;
      Left = 0;
   }
   else
   {
      if (Fd.Read(Buffer + Size, End - Buffer - Size) == false)
	 return false;
      Left -= End - Buffer - Size;
   }   
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
      if (Stop[0] == '\n')
      {
	 // Extra one at the end to simplify find
	 Indexes[TagCount] = Stop - Section;
	 for (; Stop[0] == '\n' && Stop < End; Stop++);
	 return true;
	 break;
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

#include <pkglib/pkgcachegen.h>

int main(int argc,char *argv[])
{
   {
      File F(argv[1],File::ReadOnly);
      pkgTagFile Test(F);
      File CacheF("./cache",File::WriteEmpty);
      DynamicMMap Map(CacheF,MMap::Public);
      pkgCacheGenerator Gen(Map);
      Gen.SelectFile("tet");
   }

#if 0 
   pkgTagSection I;
   while (Test.Step(I) == true)
   {
      const char *Start;
      const char *End;
      if (I.Find("Package",Start,End) == false)
      {
	 cout << "Failed" << endl;
	 continue;
      }
      
      cout << "Package: " << string(Start,End - Start) << endl;
      
/*      for (const char *I = Start; I < End; I++)
      {
	 const char *Begin = I;
	 bool Number = true;
	 while (isspace(*I) == 0 && ispunct(*I) == 0 && I < End)
	 {
	    if (isalpha(*I) != 0)
	       Number = false;
	    I++;
	 }
	 if (Number == false)
	    cout << string(Begin,I-Begin) << endl;	 
	 while ((isspace(*I) != 0 || ispunct(*I) != 0) && I < End)
	    I++;
	 I--;
      }      */
   }
#endif   
   _error->DumpErrors();
}
