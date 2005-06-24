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

#ifdef __GNUG__
#pragma interface "cachedb.h"
#endif 

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
      Key.size = snprintf(TmpKey,sizeof(TmpKey),"%s:%s",Type,FileName.c_str());
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
   
   // Stat info stored in the DB, Fixed types since it is written to disk.
   enum FlagList {FlControl = (1<<0),FlMD5=(1<<1),FlContents=(1<<2)};
   struct StatStore
   {
      time_t   mtime;          
      uint32_t Flags;
   } CurStat;
   struct StatStore OldStat;
   
   // 'set' state
   string FileName;
   struct stat FileStat;
   FileFd *Fd;
   debDebFile *DebFile;
   
   public:

   // Data collection helpers
   debDebFile::MemControlExtract Control;
   ContentsExtract Contents;
   
   // Runtime statistics
   struct Stats
   {
      double Bytes;
      double MD5Bytes;
      unsigned long Packages;
      unsigned long Misses;  
      unsigned long DeLinkBytes;
      
      inline void Add(const Stats &S) {Bytes += S.Bytes; MD5Bytes += S.MD5Bytes;
	 Packages += S.Packages; Misses += S.Misses; DeLinkBytes += S.DeLinkBytes;};
      Stats() : Bytes(0), MD5Bytes(0), Packages(0), Misses(0), DeLinkBytes(0) {};
   } Stats;
   
   bool ReadyDB(string DB);
   inline bool DBFailed() {return Dbp != 0 && DBLoaded == false;};
   inline bool Loaded() {return DBLoaded == true;};
   
   bool SetFile(string FileName,struct stat St,FileFd *Fd);
   bool LoadControl();
   bool LoadContents(bool GenOnly);
   bool GetMD5(string &MD5Res,bool GenOnly);
   bool Finish();   
   
   bool Clean();
   
   CacheDB(string DB) : Dbp(0), DebFile(0) {ReadyDB(DB);};
   ~CacheDB() {ReadyDB(string()); delete DebFile;};
};
    
#endif
