// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cachedb.h,v 1.4 2004/05/08 19:41:01 mdz Exp $
/* ######################################################################

   CacheDB
   
   Simple uniform interface to a cache database.
   
   ##################################################################### */
									/*}}}*/
#ifndef CACHEDB_H
#define CACHEDB_H



#include <db.h>
#include <string>
#include <apt-pkg/debfile.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
    
#include "contents.h"
    
class CacheDB
{
   protected:
      
   // Database state/access
   DBT Key;
   DBT Data;
   char TmpKey[600];
   DB *Dbp;
   bool DBLoaded;
   bool ReadOnly;
   string DBFile;

   // Generate a key for the DB of a given type
   inline void InitQuery(const char *Type)
   {
      memset(&Key,0,sizeof(Key));
      memset(&Data,0,sizeof(Data));
      Key.data = TmpKey;
      Key.size = snprintf(TmpKey,sizeof(TmpKey),"%s:%s",FileName.c_str(), Type);
   }
   
   inline bool Get() 
   {
      return Dbp->get(Dbp,0,&Key,&Data,0) == 0;
   };
   inline bool Put(const void *In,unsigned long Length) 
   {
      if (ReadOnly == true)
	 return true;
      Data.size = Length;
      Data.data = (void *)In;
      if (DBLoaded == true && (errno = Dbp->put(Dbp,0,&Key,&Data,0)) != 0)
      {
	 DBLoaded = false;
	 return false;
      }
      return true;
   }
   bool OpenFile();
   bool GetFileStat();
   bool GetCurStat();
   bool LoadControl();
   bool LoadContents(bool GenOnly);
   bool GetMD5(bool GenOnly);
   bool GetSHA1(bool GenOnly);
   bool GetSHA256(bool GenOnly);
   
   // Stat info stored in the DB, Fixed types since it is written to disk.
   enum FlagList {FlControl = (1<<0),FlMD5=(1<<1),FlContents=(1<<2),
   	FlSize=(1<<3), FlSHA1=(1<<4), FlSHA256=(1<<5)};
   struct StatStore
   {
      uint32_t Flags;
      uint32_t mtime;          
      uint32_t FileSize;
      uint8_t  MD5[16];
      uint8_t  SHA1[20];
      uint8_t  SHA256[32];
   } CurStat;
   struct StatStore OldStat;
   
   // 'set' state
   string FileName;
   FileFd *Fd;
   debDebFile *DebFile;
   
   public:

   // Data collection helpers
   debDebFile::MemControlExtract Control;
   ContentsExtract Contents;
   string MD5Res;
   string SHA1Res;
   string SHA256Res;
   
   // Runtime statistics
   struct Stats
   {
      double Bytes;
      double MD5Bytes;
      double SHA1Bytes;
      double SHA256Bytes;
      unsigned long Packages;
      unsigned long Misses;  
      unsigned long DeLinkBytes;
      
      inline void Add(const Stats &S) {
	 Bytes += S.Bytes; MD5Bytes += S.MD5Bytes; SHA1Bytes += S.SHA1Bytes; 
	 SHA256Bytes += S.SHA256Bytes;
	 Packages += S.Packages; Misses += S.Misses; DeLinkBytes += S.DeLinkBytes;};
      Stats() : Bytes(0), MD5Bytes(0), SHA1Bytes(0), SHA256Bytes(0), Packages(0), Misses(0), DeLinkBytes(0) {};
   } Stats;
   
   bool ReadyDB(string DB);
   inline bool DBFailed() {return Dbp != 0 && DBLoaded == false;};
   inline bool Loaded() {return DBLoaded == true;};
   
   inline off_t GetFileSize(void) {return CurStat.FileSize;}
   
   bool SetFile(string FileName,struct stat St,FileFd *Fd);
   bool GetFileInfo(string FileName, bool DoControl, bool DoContents,
		   bool GenContentsOnly, bool DoMD5, bool DoSHA1, bool DoSHA256);
   bool Finish();   
   
   bool Clean();
   
   CacheDB(string DB) : Dbp(0), Fd(NULL), DebFile(0) {ReadyDB(DB);};
   ~CacheDB() {ReadyDB(string()); delete DebFile;};
};
    
#endif
