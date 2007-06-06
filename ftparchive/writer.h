// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: writer.h,v 1.4.2.2 2003/12/26 22:55:43 mdz Exp $
/* ######################################################################

   Writer 
   
   The file writer classes. These write various types of output, sources,
   packages and contents.
   
   ##################################################################### */
									/*}}}*/
#ifndef WRITER_H
#define WRITER_H


#include <string>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>

#include "cachedb.h"
#include "override.h"
#include "apt-ftparchive.h"

using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::map;
    
class FTWScanner
{
   protected:
   vector<string> Patterns;
   const char *OriginalPath;
   char *RealPath;
   bool ErrorPrinted;
   
   // Stuff for the delinker
   bool NoLinkAct;
   
   static FTWScanner *Owner;
   static int ScannerFTW(const char *File,const struct stat *sb,int Flag);
   static int ScannerFile(const char *File, bool ReadLink);

   bool Delink(string &FileName,const char *OriginalPath,
	       unsigned long &Bytes,off_t FileSize);

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
   void ClearPatterns() { Patterns.clear(); };
   void AddPattern(string Pattern) { Patterns.push_back(Pattern); };
   bool SetExts(string Vals);
      
   FTWScanner();
   virtual ~FTWScanner() {delete [] RealPath;};
};

class PackagesWriter : public FTWScanner
{
   Override Over;
   CacheDB Db;
      
   public:

   // Some flags
   bool DoMD5;
   bool DoSHA1;
   bool DoSHA256;
   bool NoOverride;
   bool DoContents;

   // General options
   string PathPrefix;
   string DirStrip;
   FILE *Output;
   struct CacheDB::Stats &Stats;
   string Arch;

   inline bool ReadOverride(string File) {return Over.ReadOverride(File);};
   inline bool ReadExtraOverride(string File) 
      {return Over.ReadExtraOverride(File);};
   virtual bool DoPackage(string FileName);

   PackagesWriter(string DB,string Overrides,string ExtOverrides=string(),
		  string Arch=string());
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

   virtual bool DoPackage(string FileName);

   SourcesWriter(string BOverrides,string SOverrides,
		 string ExtOverrides=string());
   virtual ~SourcesWriter() {free(Buffer);};
};

class ReleaseWriter : public FTWScanner
{
public:
   ReleaseWriter(string DB);
   virtual bool DoPackage(string FileName);
   void Finish();

   FILE *Output;
   // General options
   string PathPrefix;
   string DirStrip;

protected:
   struct CheckSum
   {
      string MD5;
      string SHA1;
      string SHA256;
      // Limited by FileFd::Size()
      unsigned long size;
      ~CheckSum() {};
   };
   map<string,struct CheckSum> CheckSums;
};

#endif
