// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   contents - Archive contents generator

   The GenContents class is a back end for an archive contents generator.
   It takes a list of per-deb file name and merges it into a memory
   database of all previous output. This database is stored as a set of
   pairs (path, package).

   This may be very inefficient since it does duplicate all path components,
   whereas most are shared. A previous implementation used a tree structure
   with a binary tree for entries in a directory, which was significantly
   more space-efficient but it did not do rebalancing and implementing custom
   self-balancing trees here seems a waste of effort.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debfile.h>
#include <apt-pkg/dirstream.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contents.h"

#include <apti18n.h>
									/*}}}*/

// GenContents::~GenContents - Free allocated memory			/*{{{*/
// ---------------------------------------------------------------------
/* Since all our allocations are static big-block allocations all that is 
   needed is to free all of them. */
GenContents::~GenContents()
{
   while (BlockList != 0)
   {
      BigBlock *Old = BlockList;
      BlockList = Old->Next;
      free(Old->Block);
      delete Old;
   }   
}
									/*}}}*/
// GenContents::Mystrdup - Custom strdup				/*{{{*/
// ---------------------------------------------------------------------
/* This strdup also uses a large block allocator to eliminate glibc
   overhead */
GenContents::StringInBlock GenContents::Mystrdup(const char *From)
{
   unsigned int Len = strlen(From) + 1;
   if (StrLeft <= Len)
   {
      StrLeft = 4 * 1024 * 1024;
      if (unlikely(StrLeft <= Len))
	 abort();
      StrPool = (char *)malloc(StrLeft);
      
      BigBlock *Block = new BigBlock;
      Block->Block = StrPool;
      Block->Next = BlockList;
      BlockList = Block;
   }
   
   memcpy(StrPool,From,Len);
   StrLeft -= Len;
   
   char *Res = StrPool;
   StrPool += Len;
   return StringInBlock{Res};
}
									/*}}}*/
// GenContents::Add - Add a path to the tree				/*{{{*/
// ---------------------------------------------------------------------
/* This takes a full pathname and adds it into the tree. We split the
   pathname into directory fragments adding each one as we go. Technically
   in output from tar this should result in hitting previous items. */
void GenContents::Add(const char *Dir, StringInBlock Package)
{
   // Do not add directories. We do not print out directories!
   if (APT::String::Endswith(Dir, "/"))
      return;
   // Drop leading slashes
   while (*Dir == '/')
      Dir++;

   // We used to add all parents directories here too, but we never printed
   // them, so just add the file directly.
   Entries.emplace(Mystrdup(Dir), Package);
}
									/*}}}*/
// GenContents::WriteSpace - Write a given number of white space chars	/*{{{*/
// ---------------------------------------------------------------------
/* We mod 8 it and write tabs where possible. */
void GenContents::WriteSpace(std::string &out, size_t Current, size_t Target)
{
   if (Target <= Current)
      Target = Current + 1;

   /* Now we write tabs so long as the next tab stop would not pass
      the target */
   for (; (Current/8 + 1)*8 < Target; Current = (Current/8 + 1)*8)
      out.append("\t");

   // Fill the last bit with spaces
   for (; Current < Target; Current++)
      out.append(" ");
}
									/*}}}*/
// GenContents::Print - Display the tree				/*{{{*/
// ---------------------------------------------------------------------
/* This is the final result function. It takes the tree and recursively
   calls itself and runs over each section of the tree printing out
   the pathname and the hit packages. We use Buf to build the pathname
   summed over all the directory parents of this node. */
void GenContents::Print(FileFd &Out)
{
   const char *last = nullptr;
   std::string line;
   for (auto &entry : Entries)
   {
      // Do not show the item if it is a directory
      if (not APT::String::Endswith(entry.first.c_str(), "/"))
      {
	 // We are still appending to the same file path
	 if (last != nullptr && strcmp(entry.first.c_str(), last) == 0)
	 {
	    line.append(",");
	    line.append(entry.second.c_str());
	    continue;
	 }
	 // New file. If we saw a file before, write out its line
	 if (last != nullptr)
	 {
	    line.append("\n", 1);
	    Out.Write(line.data(), line.length());
	 }

	 // Append the package name, tab(s), and first to the line
	 line.assign(entry.first.c_str());
	 WriteSpace(line, line.length(), 60);
	 line.append(entry.second.c_str());
	 last = entry.first.c_str();
      }
   }
   // Print the trailing line
   if (last != nullptr)
   {
      line.append("\n", 1);
      Out.Write(line.c_str(), line.length());
   }
}
									/*}}}*/
// ContentsExtract Constructor						/*{{{*/
ContentsExtract::ContentsExtract()
   : Data(0), MaxSize(0), CurSize(0)
{
}
									/*}}}*/
// ContentsExtract Destructor						/*{{{*/
ContentsExtract::~ContentsExtract()
{
   free(Data);
}
									/*}}}*/
// ContentsExtract::Read - Read the archive				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ContentsExtract::Read(debDebFile &Deb)
{
   Reset();
   return Deb.ExtractArchive(*this);
}
									/*}}}*/
// ContentsExtract::DoItem - Extract an item				/*{{{*/
// ---------------------------------------------------------------------
/* This just tacks the name onto the end of our memory buffer */
bool ContentsExtract::DoItem(Item &Itm, int &/*Fd*/)
{
   unsigned long Len = strlen(Itm.Name);
   
   // Strip leading ./'s
   if (Itm.Name[0] == '.' && Itm.Name[1] == '/')
   {
      // == './'
      if (Len == 2)
	 return true;
      
      Len -= 2;
      Itm.Name += 2;
   }

   // Allocate more storage for the string list
   if (CurSize + Len + 2 >= MaxSize || Data == 0)
   {
      if (MaxSize == 0)
	 MaxSize = 512*1024/2;
      char *NewData = (char *)realloc(Data,MaxSize*2);
      if (NewData == 0)
	 return _error->Error(_("realloc - Failed to allocate memory"));
      Data = NewData;
      MaxSize *= 2;
   }
   
   strcpy(Data+CurSize,Itm.Name);   
   CurSize += Len + 1;
   return true;
}
									/*}}}*/
// ContentsExtract::TakeContents - Load the contents data		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ContentsExtract::TakeContents(const void *NewData,unsigned long long Length)
{
   if (Length == 0)
   {
      CurSize = 0;
      return true;
   }

   // Allocate more storage for the string list
   if (Length + 2 >= MaxSize || Data == 0)
   {
      if (MaxSize == 0)
	 MaxSize = 512*1024/2;
      while (MaxSize*2 <= Length)
	 MaxSize *= 2;
      
      char *NewData = (char *)realloc(Data,MaxSize*2);
      if (NewData == 0)
	 return _error->Error(_("realloc - Failed to allocate memory"));
      Data = NewData;
      MaxSize *= 2;
   }
   memcpy(Data,NewData,Length);
   CurSize = Length;
   
   return Data[CurSize-1] == 0;
}
									/*}}}*/
// ContentsExtract::Add - Read the contents data into the sorter	/*{{{*/
// ---------------------------------------------------------------------
/* */
void ContentsExtract::Add(GenContents &Contents,std::string const &Package)
{
   const char *Start = Data;
   auto Pkg = Contents.Mystrdup(Package.c_str());
   for (const char *I = Data; I < Data + CurSize; I++)
   {
      if (*I == 0)
      {
	 Contents.Add(Start,Pkg);
	 Start = ++I;
      }      
   }   
}
									/*}}}*/
