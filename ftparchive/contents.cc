// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: contents.cc,v 1.4 2003/02/10 07:34:41 doogie Exp $
/* ######################################################################
   
   contents - Archive contents generator
   
   The GenContents class is a back end for an archive contents generator. 
   It takes a list of per-deb file name and merges it into a memory 
   database of all previous output. This database is stored as a set
   of binary trees linked across directories to form a tree of all files+dirs
   given to it. The tree will also be sorted as it is built up thus 
   removing the massive sort time overhead.
   
   By breaking all the pathnames into components and storing them 
   separately a space savings is realized by not duplicating the string
   over and over again. Ultimately this saving is sacrificed to storage of
   the tree structure itself but the tree structure yields a speed gain
   in the sorting and processing. Ultimately it takes about 5 seconds to
   do 141000 nodes and about 5 meg of ram.

   The tree looks something like:
   
     usr/
      / \             / libslang
   bin/ lib/ --> libc6
        /   \         \ libfoo
   games/  sbin/
   
   The ---> is the DirDown link
   
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "contents.h"

#include <apti18n.h>
#include <apt-pkg/extracttar.h>
#include <apt-pkg/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>    
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
char *GenContents::Mystrdup(const char *From)
{
   unsigned int Len = strlen(From) + 1;
   if (StrLeft <= Len)
   {
      StrLeft = 4096*10;
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
   return Res;
}
									/*}}}*/
// GenContents::Node::operator new - Big block allocator		/*{{{*/
// ---------------------------------------------------------------------
/* This eliminates glibc's malloc overhead by allocating large blocks and
   having a continuous set of Nodes. This takes about 8 bytes off each nodes
   space needs. Freeing is not supported. */
void *GenContents::Node::operator new(size_t Amount,GenContents *Owner)
{
   if (Owner->NodeLeft == 0)
   {
      Owner->NodeLeft = 10000;
      Owner->NodePool = (Node *)malloc(Amount*Owner->NodeLeft);
      BigBlock *Block = new BigBlock;
      Block->Block = Owner->NodePool;
      Block->Next = Owner->BlockList;
      Owner->BlockList = Block;
   }
   
   Owner->NodeLeft--;
   return Owner->NodePool++;
}
									/*}}}*/
// GenContents::Grab - Grab a new node representing Name under Top	/*{{{*/
// ---------------------------------------------------------------------
/* This grabs a new node representing the pathname component Name under
   the node Top. The node is given the name Package. It is assumed that Name
   is inside of top. If a duplicate already entered name is found then 
   a note is made on the Dup list and the previous in-tree node is returned. */
GenContents::Node *GenContents::Grab(GenContents::Node *Top,const char *Name,
			const char *Package)
{
   /* We drop down to the next dir level each call. This simplifies
      the calling routine */
   if (Top->DirDown == 0)
   {
      Node *Item = new(this) Node;
      Item->Path = Mystrdup(Name);
      Item->Package = Package;
      Top->DirDown = Item;
      return Item;
   }
   Top = Top->DirDown;
   
   int Res;
   while (1)
   {
      Res = strcmp(Name,Top->Path);
      
      // Collision!
      if (Res == 0)
      {
	 // See if this the the same package (multi-version dup)
	 if (Top->Package == Package ||
	     strcasecmp(Top->Package,Package) == 0)
	    return Top;
	 
	 // Look for an already existing Dup
	 for (Node *I = Top->Dups; I != 0; I = I->Dups)
	    if (I->Package == Package || 
		strcasecmp(I->Package,Package) == 0)
	       return Top;

	 // Add the dup in
	 Node *Item = new(this) Node;
	 Item->Path = Top->Path;
	 Item->Package = Package;
	 Item->Dups = Top->Dups;
	 Top->Dups = Item;
	 return Top;
      }
      
      // Continue to traverse the tree
      if (Res < 0)
      {
	 if (Top->BTreeLeft == 0)
	    break;
	 Top = Top->BTreeLeft;
      }      
      else
      {
	 if (Top->BTreeRight == 0)
	    break;
	 Top = Top->BTreeRight;
      }      
   }

   // The item was not found in the tree
   Node *Item = new(this) Node;
   Item->Path = Mystrdup(Name);
   Item->Package = Package;
   
   // Link it into the tree
   if (Res < 0)
   {
      Item->BTreeLeft = Top->BTreeLeft;
      Top->BTreeLeft = Item;
   }
   else
   {
      Item->BTreeRight = Top->BTreeRight;
      Top->BTreeRight = Item;
   }
   
   return Item;
}
									/*}}}*/
// GenContents::Add - Add a path to the tree				/*{{{*/
// ---------------------------------------------------------------------
/* This takes a full pathname and adds it into the tree. We split the
   pathname into directory fragments adding each one as we go. Technically
   in output from tar this should result in hitting previous items. */
void GenContents::Add(const char *Dir,const char *Package)
{
   Node *Root = &this->Root;
   
   // Drop leading slashes
   while (*Dir == '/' && *Dir != 0)
      Dir++;
   
   // Run over the string and grab out each bit up to and including a /
   const char *Start = Dir;
   const char *I = Dir;
   while (*I != 0)
   {
      if (*I != '/' || I - Start <= 1)
      {
	 I++;
	 continue;
      }      
      I++;
      
      // Copy the path fragment over
      char Tmp[1024];
      strncpy(Tmp,Start,I - Start);
      Tmp[I - Start] = 0;
      
      // Grab a node for it
      Root = Grab(Root,Tmp,Package);
      
      Start = I;
   }
   
   // The final component if it does not have a trailing /
   if (I - Start >= 1)
      Root = Grab(Root,Start,Package);
}
									/*}}}*/
// GenContents::WriteSpace - Write a given number of white space chars	/*{{{*/
// ---------------------------------------------------------------------
/* We mod 8 it and write tabs where possible. */
void GenContents::WriteSpace(FILE *Out,unsigned int Current,unsigned int Target)
{
   if (Target <= Current)
      Target = Current + 1;
   
   /* Now we write tabs so long as the next tab stop would not pass
      the target */
   for (; (Current/8 + 1)*8 < Target; Current = (Current/8 + 1)*8)
      fputc('\t',Out);

   // Fill the last bit with spaces
   for (; Current < Target; Current++)
      fputc(' ',Out);
}
									/*}}}*/
// GenContents::Print - Display the tree				/*{{{*/
// ---------------------------------------------------------------------
/* This is the final result function. It takes the tree and recursively
   calls itself and runs over each section of the tree printing out
   the pathname and the hit packages. We use Buf to build the pathname
   summed over all the directory parents of this node. */
void GenContents::Print(FILE *Out)
{
   char Buffer[1024];
   Buffer[0] = 0;
   DoPrint(Out,&Root,Buffer);
}
void GenContents::DoPrint(FILE *Out,GenContents::Node *Top, char *Buf)
{
   if (Top == 0)
      return;

   // Go left
   DoPrint(Out,Top->BTreeLeft,Buf);

   // Print the current dir location and then descend to lower dirs
   char *OldEnd = Buf + strlen(Buf);
   if (Top->Path != 0)
   {
      strcat(Buf,Top->Path);
      
      // Do not show the item if it is a directory with dups
      if (Top->Path[strlen(Top->Path)-1] != '/' /*|| Top->Dups == 0*/)
      {
	 fputs(Buf,Out);
	 WriteSpace(Out,strlen(Buf),60);
	 for (Node *I = Top; I != 0; I = I->Dups)
	 {
	    if (I != Top)
	       fputc(',',Out);
	    fputs(I->Package,Out);
	 }
         fputc('\n',Out);
      }      
   }   
   
   // Go along the directory link
   DoPrint(Out,Top->DirDown,Buf);
   *OldEnd = 0;
   
   // Go right
   DoPrint(Out,Top->BTreeRight,Buf);  
}
									/*}}}*/

// ContentsExtract::Read - Read the archive				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ContentsExtract::Read(debDebFile &Deb)
{
   Reset();
   
   // Get the archive member and positition the file 
   const ARArchive::Member *Member = Deb.GotoMember("data.tar.gz");
   const char *Compressor = "gzip";
   if (Member == 0) {
      Member = Deb.GotoMember("data.tar.bz2");
      Compressor = "bzip2";
   }
   if (Member == 0) {
      Member = Deb.GotoMember("data.tar.lzma");
      Compressor = "lzma";
   }
   if (Member == 0) {
      _error->Error(_("Internal error, could not locate member %s"),
		    "data.tar.{gz,bz2,lzma}");
      return false;
   }
      
   // Extract it.
   ExtractTar Tar(Deb.GetFile(),Member->Size,Compressor);
   if (Tar.Go(*this) == false)
      return false;   
   return true;   
}
									/*}}}*/
// ContentsExtract::DoItem - Extract an item				/*{{{*/
// ---------------------------------------------------------------------
/* This just tacks the name onto the end of our memory buffer */
bool ContentsExtract::DoItem(Item &Itm,int &Fd)
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
bool ContentsExtract::TakeContents(const void *NewData,unsigned long Length)
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
void ContentsExtract::Add(GenContents &Contents,string Package)
{
   const char *Start = Data;
   char *Pkg = Contents.Mystrdup(Package.c_str());
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
