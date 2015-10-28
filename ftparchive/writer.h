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

#include <apt-pkg/hashes.h>

#include <string>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stdlib.h>
#include <sys/types.h>

#include "contents.h"
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
   string Arch;
   bool IncludeArchAll;
   const char *OriginalPath;
   bool ErrorPrinted;

   // Stuff for the delinker
   bool NoLinkAct;

   static FTWScanner *Owner;
   static int ScannerFTW(const char *File,const struct stat *sb,int Flag);
   static int ScannerFile(const char *File, bool const &ReadLink);

   bool Delink(string &FileName,const char *OriginalPath,
	       unsigned long long &Bytes,unsigned long long const &FileSize);

   inline void NewLine(unsigned const &Priority)
   {
      if (ErrorPrinted == false && Quiet <= Priority)
      {
	 c1out << endl;
	 ErrorPrinted = true;
      }
   }

   public:
   FileFd *Output;
   bool OwnsOutput;
   unsigned int DoHashes;

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

   FTWScanner(FileFd * const Output, string const &Arch = string(), bool const IncludeArchAll = true);
   virtual ~FTWScanner();
};

class MultiCompress;

class TranslationWriter
{
   MultiCompress *Comp;
   std::set<string> Included;
   FileFd *Output;

   public:
   bool DoPackage(string const &Pkg, string const &Desc, string const &MD5);

   TranslationWriter(string const &File, string const &TransCompress, mode_t const &Permissions);
   ~TranslationWriter();
};

class PackagesWriter : public FTWScanner
{
   Override Over;
   CacheDB Db;

   public:

   // Some flags
   bool DoAlwaysStat;
   bool NoOverride;
   bool DoContents;
   bool LongDescription;

   // General options
   string PathPrefix;
   string DirStrip;
   struct CacheDB::Stats &Stats;
   TranslationWriter * const TransWriter;

   inline bool ReadOverride(string const &File) {return Over.ReadOverride(File);};
   inline bool ReadExtraOverride(string const &File) 
      {return Over.ReadExtraOverride(File);};
   virtual bool DoPackage(string FileName) APT_OVERRIDE;

   PackagesWriter(FileFd * const Output, TranslationWriter * const TransWriter, string const &DB,
                  string const &Overrides,
                  string const &ExtOverrides = "",
		  string const &Arch = "",
		  bool const IncludeArchAll = true);
   virtual ~PackagesWriter();
};

class ContentsWriter : public FTWScanner
{
   CacheDB Db;

   GenContents Gen;

   public:

   // General options
   struct CacheDB::Stats &Stats;
   string Prefix;

   bool DoPackage(string FileName,string Package);
   virtual bool DoPackage(string FileName) APT_OVERRIDE 
             {return DoPackage(FileName,string());};
   bool ReadFromPkgs(string const &PkgFile,string const &PkgCompress);

   void Finish() {Gen.Print(*Output);};
   inline bool ReadyDB(string const &DB) {return Db.ReadyDB(DB);};

   ContentsWriter(FileFd * const Output, string const &DB, string const &Arch = string(),
	 bool const IncludeArchAll = true);
   virtual ~ContentsWriter() {};
};

class SourcesWriter : public FTWScanner
{
   CacheDB Db;
   Override BOver;
   Override SOver;
   char *Buffer;
   unsigned long long BufSize;

   public:

   bool NoOverride;
   bool DoAlwaysStat;

   // General options
   string PathPrefix;
   string DirStrip;
   struct CacheDB::Stats &Stats;

   virtual bool DoPackage(string FileName) APT_OVERRIDE;

   SourcesWriter(FileFd * const Output, string const &DB,string const &BOverrides,string const &SOverrides,
		 string const &ExtOverrides=string());
   virtual ~SourcesWriter() {free(Buffer);};
};

class ReleaseWriter : public FTWScanner
{
public:
   ReleaseWriter(FileFd * const Output, string const &DB);
   virtual bool DoPackage(string FileName) APT_OVERRIDE;
   void Finish();

   // General options
   string PathPrefix;
   string DirStrip;

   struct CheckSum
   {
      HashStringList Hashes;
      // Limited by FileFd::Size()
      unsigned long long size;
      ~CheckSum() {};
   };
protected:
   map<string,struct CheckSum> CheckSums;
};

#endif
