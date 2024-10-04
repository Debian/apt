// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   contents - Contents of archive things.
   
   ##################################################################### */
									/*}}}*/
#ifndef CONTENTS_H
#define CONTENTS_H

#include <apt-pkg/dirstream.h>
#include <apt-pkg/strutl.h>

#include <set>
#include <string>
#include <stddef.h>
#include <stdio.h>

class debDebFile;
class FileFd;

class GenContents
{
   /// \brief zero-terminated strings inside a BigBlock with minimal C++ accessor and comparison operator
   struct StringInBlock
   {
      const char *data;
      const char *c_str() const { return data; }
      bool operator<(const StringInBlock &other) const { return strcmp(data, other.data) < 0; }
   };
   struct BigBlock
   {
      void *Block;
      BigBlock *Next;
   };

   std::set<std::pair<StringInBlock, StringInBlock>> Entries;

   // Big block allocation pools
   BigBlock *BlockList;   
   char *StrPool;
   unsigned long StrLeft;

   void WriteSpace(std::string &out, size_t Current, size_t Target);

   public:
   StringInBlock Mystrdup(const char *From);
   void Add(const char *Dir, StringInBlock Package);
   void Print(FileFd &Out);

   GenContents() : BlockList(0), StrPool(0), StrLeft(0) {};
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
