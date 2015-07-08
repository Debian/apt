// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: contents.h,v 1.2 2001/02/20 07:03:18 jgg Exp $
/* ######################################################################
   
   contents - Contents of archive things.
   
   ##################################################################### */
									/*}}}*/
#ifndef CONTENTS_H
#define CONTENTS_H

#include <apt-pkg/dirstream.h>

#include <stddef.h>
#include <stdio.h>
#include <string>

class debDebFile;
class FileFd;

class GenContents
{
   struct Node
   {
      // Binary Tree links
      Node *BTreeLeft;
      Node *BTreeRight;
      Node *DirDown;
      Node *Dups;
      const char *Path;
      const char *Package;
      
      void *operator new(size_t Amount,GenContents *Owner);
      void operator delete(void *) {};
      
      Node() : BTreeLeft(0), BTreeRight(0), DirDown(0), Dups(0), 
               Path(0), Package(0) {};
   };
   friend struct Node;
   
   struct BigBlock
   {
      void *Block;
      BigBlock *Next;
   };
   
   Node Root;
   
   // Big block allocation pools
   BigBlock *BlockList;   
   char *StrPool;
   unsigned long StrLeft;
   Node *NodePool;
   unsigned long NodeLeft;
   
   Node *Grab(Node *Top,const char *Name,const char *Package);
   void WriteSpace(std::string &out, size_t Current, size_t Target);
   void DoPrint(FileFd &Out,Node *Top, char *Buf);
   
   public:
   
   char *Mystrdup(const char *From);
   void Add(const char *Dir,const char *Package);   
   void Print(FileFd &Out);

   GenContents() : BlockList(0), StrPool(0), StrLeft(0), 
                   NodePool(0), NodeLeft(0) {};
   ~GenContents();
};

class ContentsExtract : public pkgDirStream      
{
   public:

   // The Data Block
   char *Data;
   unsigned long long MaxSize;
   unsigned long long CurSize;
   void AddData(const char *Text);
   
   bool Read(debDebFile &Deb);
   
   virtual bool DoItem(Item &Itm,int &Fd) APT_OVERRIDE;      
   void Reset() {CurSize = 0;};
   bool TakeContents(const void *Data,unsigned long long Length);
   void Add(GenContents &Contents,std::string const &Package);
   
   ContentsExtract();
   virtual ~ContentsExtract();
};

#endif
