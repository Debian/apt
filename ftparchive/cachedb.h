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

#include <apt-pkg/hashes.h>
#include <apt-pkg/debfile.h>

#include <db.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "contents.h"
#include "sources.h"

class FileFd;


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
   void _InitQuery(const char *Type)
   {
      memset(&Key,0,sizeof(Key));
      memset(&Data,0,sizeof(Data));
      Key.data = TmpKey;
      Key.size = snprintf(TmpKey,sizeof(TmpKey),"%s:%s",FileName.c_str(), Type);
   }
   
   void InitQueryStats() {
      _InitQuery("st");
   }
   void InitQuerySource() {
      _InitQuery("cs");
   }
   void InitQueryControl() {
      _InitQuery("cl");
   }
   void InitQueryContent() {
      _InitQuery("cn");
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
   void CloseFile();

   bool OpenDebFile();
   void CloseDebFile();

   // GetCurStat needs some compat code, see lp #1274466)
   bool GetCurStatCompatOldFormat();
   bool GetCurStatCompatNewFormat();
   bool GetCurStat();

   bool GetFileStat(bool const &doStat = false);
   bool LoadControl();
   bool LoadContents(bool const &GenOnly);
   bool LoadSource();
   bool GetHashes(bool const GenOnly, unsigned int const DoHashes);

   // Stat info stored in the DB, Fixed types since it is written to disk.
   enum FlagList {FlControl = (1<<0),FlMD5=(1<<1),FlContents=(1<<2),
                  FlSize=(1<<3), FlSHA1=(1<<4), FlSHA256=(1<<5),
                  FlSHA512=(1<<6), FlSource=(1<<7)
   };

   // the on-disk format changed (FileSize increased to 64bit) in 
   // commit 650faab0 which will lead to corruption with old caches
   struct StatStoreOldFormat
   {
      uint32_t Flags;
      uint32_t mtime;
      uint32_t FileSize;
      uint8_t  MD5[16];
      uint8_t  SHA1[20];
      uint8_t  SHA256[32];
   } CurStatOldFormat;

   // WARNING: this struct is read/written to the DB so do not change the
   //          layout of the fields (see lp #1274466), only append to it
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
   DscExtract Dsc;
   HashStringList HashesList;

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
   
   bool ReadyDB(std::string const &DB = "");
   inline bool DBFailed() {return Dbp != 0 && DBLoaded == false;};
   inline bool Loaded() {return DBLoaded == true;};
   
   inline unsigned long long GetFileSize(void) {return CurStat.FileSize;}
   
   bool SetFile(std::string const &FileName,struct stat St,FileFd *Fd);

   // terrible old overloaded interface
   bool GetFileInfo(std::string const &FileName,
	 bool const &DoControl,
	 bool const &DoContents,
	 bool const &GenContentsOnly,
	 bool const DoSource,
	 unsigned int const DoHashes,
	 bool const &checkMtime = false);

   bool Finish();   
   
   bool Clean();
   
   explicit CacheDB(std::string const &DB);
   ~CacheDB();
};
    
#endif
