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
#include <set>

#include "cachedb.h"
#include "multicompress.h"
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
   string Arch;
   const char *OriginalPath;
   bool ErrorPrinted;
   
   // Stuff for the delinker
   bool NoLinkAct;
   
   static FTWScanner *Owner;
   static int ScannerFTW(const char *File,const struct stat *sb,int Flag);
   static int ScannerFile(const char *File, bool const &ReadLink);

   bool Delink(string &FileName,const char *OriginalPath,
	       unsigned long &Bytes,off_t const &FileSize);

   inline void NewLine(unsigned const &Priority)
   {
      if (ErrorPrinted == false && Quiet <= Priority)
      {
	 c1out << endl;
	 ErrorPrinted = true;
      }	 
   }
   
   public:

   unsigned long DeLinkLimit;
   string InternalPrefix;

   virtual bool DoPackage(string FileName) = 0;
   bool RecursiveScan(string const &Dir);
   bool LoadFileList(string const &BaseDir,string const &File);
   void ClearPatterns() { Patterns.clear(); };
   void AddPattern(string const &Pattern) { Patterns.push_back(Pattern); };
   void AddPattern(char const *Pattern) { Patterns.push_back(Pattern); };
   void AddPatterns(std::vector<std::string> const &patterns) { Patterns.insert(Patterns.end(), patterns.begin(), patterns.end()); };
   bool SetExts(string const &Vals);
      
   FTWScanner(string const &Arch = string());
   virtual ~FTWScanner() {};
};

class TranslationWriter
{
   MultiCompress *Comp;
   FILE *Output;
   std::set<string> Included;
   unsigned short RefCounter;

   public:
   void IncreaseRefCounter() { ++RefCounter; };
   unsigned short DecreaseRefCounter() { return (RefCounter == 0) ? 0 : --RefCounter; };
   unsigned short GetRefCounter() const { return RefCounter; };
   bool DoPackage(string const &Pkg, string const &Desc, string const &MD5);

   TranslationWriter(string const &File, string const &TransCompress, mode_t const &Permissions);
   TranslationWriter() : Comp(NULL), Output(NULL), RefCounter(0) {};
   ~TranslationWriter();
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
   bool DoAlwaysStat;
   bool NoOverride;
   bool DoContents;
   bool LongDescription;

   // General options
   string PathPrefix;
   string DirStrip;
   FILE *Output;
   struct CacheDB::Stats &Stats;
   TranslationWriter *TransWriter;

   inline bool ReadOverride(string const &File) {return Over.ReadOverride(File);};
   inline bool ReadExtraOverride(string const &File) 
      {return Over.ReadExtraOverride(File);};
   virtual bool DoPackage(string FileName);

   PackagesWriter(string const &DB,string const &Overrides,string const &ExtOverrides=string(),
		  string const &Arch=string());
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
   bool ReadFromPkgs(string const &PkgFile,string const &PkgCompress);

   void Finish() {Gen.Print(Output);};
   inline bool ReadyDB(string const &DB) {return Db.ReadyDB(DB);};
   
   ContentsWriter(string const &DB, string const &Arch = string());
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

   SourcesWriter(string const &BOverrides,string const &SOverrides,
		 string const &ExtOverrides=string());
   virtual ~SourcesWriter() {free(Buffer);};
};

class ReleaseWriter : public FTWScanner
{
public:
   ReleaseWriter(string const &DB);
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
