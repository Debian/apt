// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: writer.h,v 1.3 2001/05/29 04:08:09 jgg Exp $
/* ######################################################################

   Writer 
   
   The file writer classes. These write various types of output, sources,
   packages and contents.
   
   ##################################################################### */
									/*}}}*/
#ifndef WRITER_H
#define WRITER_H

#ifdef __GNUG__
#pragma interface "writer.h"
#endif 

#include <string>
#include <stdio.h>
#include <iostream>

#include "cachedb.h"
#include "override.h"
#include "apt-ftparchive.h"

using std::string;
using std::cout;
using std::endl;
    
class FTWScanner
{
   protected:

   char *TmpExt;
   const char *Ext[10];
   const char *OriginalPath;
   char *RealPath;
   bool ErrorPrinted;
   
   // Stuff for the delinker
   bool NoLinkAct;
   
   static FTWScanner *Owner;
   static int Scanner(const char *File,const struct stat *sb,int Flag);

   bool Delink(string &FileName,const char *OriginalPath,
	       unsigned long &Bytes,struct stat &St);

   inline void NewLine(unsigned Priority)
   {
      if (ErrorPrinted == false && Quiet <= Priority)
      {
	 cout << endl;
	 ErrorPrinted = true;
      }	 
   }
   
   public:

   unsigned long DeLinkLimit;
   string InternalPrefix;

   virtual bool DoPackage(string FileName) = 0;
   bool RecursiveScan(string Dir);
   bool LoadFileList(string BaseDir,string File);
   bool SetExts(string Vals);
      
   FTWScanner();
   virtual ~FTWScanner() {delete [] RealPath; delete [] TmpExt;};
};

class PackagesWriter : public FTWScanner
{
   Override Over;
   CacheDB Db;
      
   public:

   // Some flags
   bool DoMD5;
   bool NoOverride;
   bool DoContents;

   // General options
   string PathPrefix;
   string DirStrip;
   FILE *Output;
   struct CacheDB::Stats &Stats;
   
   inline bool ReadOverride(string File) {return Over.ReadOverride(File);};
   virtual bool DoPackage(string FileName);

   PackagesWriter(string DB,string Overrides);
   virtual ~PackagesWriter() {};
};

class ContentsWriter : public FTWScanner
{
   CacheDB Db;
   
   GenContents Gen;
   
   public:

   // General options
   FILE *Output;
   struct CacheDB::Stats &Stats;
   string Prefix;
   
   bool DoPackage(string FileName,string Package);
   virtual bool DoPackage(string FileName) 
             {return DoPackage(FileName,string());};
   bool ReadFromPkgs(string PkgFile,string PkgCompress);

   void Finish() {Gen.Print(Output);};
   inline bool ReadyDB(string DB) {return Db.ReadyDB(DB);};
   
   ContentsWriter(string DB);
   virtual ~ContentsWriter() {};
};

class SourcesWriter : public FTWScanner
{
   Override BOver;
   Override SOver;
   char *Buffer;
   unsigned long BufSize;
   
   public:

   bool NoOverride;
   
   // General options
   string PathPrefix;
   string DirStrip;
   FILE *Output;
   struct CacheDB::Stats Stats;

/*   inline bool ReadBinOverride(string File) {return BOver.ReadOverride(File);};
   bool ReadSrcOverride(string File); // {return BOver.ReadOverride(File);};*/
   virtual bool DoPackage(string FileName);

   SourcesWriter(string BOverrides,string SOverrides);
   virtual ~SourcesWriter() {free(Buffer);};
};


#endif
