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


#include <apt-pkg/debfile.h>

#include <db.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>

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
   std::string DBFile;

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
   inline bool Put(const void *In,unsigned long const &Length) 
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
   bool GetFileStat(bool const &doStat = false);
   bool GetCurStat();
   bool LoadControl();
   bool LoadContents(bool const &GenOnly);
   bool GetMD5(bool const &GenOnly);
   bool GetSHA1(bool const &GenOnly);
   bool GetSHA256(bool const &GenOnly);
   bool GetSHA512(bool const &GenOnly);
   
   // Stat info stored in the DB, Fixed types since it is written to disk.
   enum FlagList {FlControl = (1<<0),FlMD5=(1<<1),FlContents=(1<<2),
                  FlSize=(1<<3), FlSHA1=(1<<4), FlSHA256=(1<<5), 
                  FlSHA512=(1<<6)};

   struct StatStore
   {
      uint32_t Flags;
      uint32_t mtime;          
      uint64_t FileSize;
      uint8_t  MD5[16];
      uint8_t  SHA1[20];
      uint8_t  SHA256[32];
      uint8_t  SHA512[64];
   } CurStat;
   struct StatStore OldStat;
   
   // 'set' state
   std::string FileName;
   FileFd *Fd;
   debDebFile *DebFile;
   
   public:

   // Data collection helpers
   debDebFile::MemControlExtract Control;
   ContentsExtract Contents;
   std::string MD5Res;
   std::string SHA1Res;
   std::string SHA256Res;
   std::string SHA512Res;
   
   // Runtime statistics
   struct Stats
   {
      double Bytes;
      double MD5Bytes;
      double SHA1Bytes;
      double SHA256Bytes;
      double SHA512Bytes;
      unsigned long Packages;
      unsigned long Misses;  
      unsigned long long DeLinkBytes;
      
      inline void Add(const Stats &S) {
	 Bytes += S.Bytes; 
         MD5Bytes += S.MD5Bytes; 
         SHA1Bytes += S.SHA1Bytes; 
	 SHA256Bytes += S.SHA256Bytes;
	 SHA512Bytes += S.SHA512Bytes;
	 Packages += S.Packages;
         Misses += S.Misses; 
         DeLinkBytes += S.DeLinkBytes;
      };
      Stats() : Bytes(0), MD5Bytes(0), SHA1Bytes(0), SHA256Bytes(0),
		SHA512Bytes(0),Packages(0), Misses(0), DeLinkBytes(0) {};
   } Stats;
   
   bool ReadyDB(std::string const &DB);
   inline bool DBFailed() {return Dbp != 0 && DBLoaded == false;};
   inline bool Loaded() {return DBLoaded == true;};
   
   inline unsigned long long GetFileSize(void) {return CurStat.FileSize;}
   
   bool SetFile(std::string const &FileName,struct stat St,FileFd *Fd);
   bool GetFileInfo(std::string const &FileName, bool const &DoControl, bool const &DoContents, bool const &GenContentsOnly,
		    bool const &DoMD5, bool const &DoSHA1, bool const &DoSHA256, bool const &DoSHA512, bool const &checkMtime = false);
   bool Finish();   
   
   bool Clean();
   
   CacheDB(std::string const &DB) : Dbp(0), Fd(NULL), DebFile(0) {TmpKey[0]='\0'; ReadyDB(DB);};
   ~CacheDB() {ReadyDB(std::string()); delete DebFile;};
};
    
#endif
