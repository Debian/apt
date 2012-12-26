// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.h,v 1.15.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Acquire Method - Method helper class + functions
   
   These functions are designed to be used within the method task to
   ease communication with APT.
   
   ##################################################################### */
									/*}}}*/

/** \addtogroup acquire
 *  @{
 *
 *  \file acquire-method.h
 */

#ifndef PKGLIB_ACQUIRE_METHOD_H
#define PKGLIB_ACQUIRE_METHOD_H

#include <stdarg.h>

#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#endif

class Hashes;
class pkgAcqMethod
{
   protected:

   struct FetchItem
   {
      FetchItem *Next;

      std::string Uri;
      std::string DestFile;
      time_t LastModified;
      bool IndexFile;
      bool FailIgnore;
   };
   
   struct FetchResult
   {
      std::string MD5Sum;
      std::string SHA1Sum;
      std::string SHA256Sum;
      std::string SHA512Sum;
      std::vector<std::string> GPGVOutput;
      time_t LastModified;
      bool IMSHit;
      std::string Filename;
      unsigned long long Size;
      unsigned long long ResumePoint;
      
      void TakeHashes(Hashes &Hash);
      FetchResult();
   };

   // State
   std::vector<std::string> Messages;
   FetchItem *Queue;
   FetchItem *QueueBack;
   std::string FailReason;
   std::string UsedMirror;
   std::string IP;
   
   // Handlers for messages
   virtual bool Configuration(std::string Message);
   virtual bool Fetch(FetchItem * /*Item*/) {return true;};
   
   // Outgoing messages
   void Fail(bool Transient = false);
   inline void Fail(const char *Why, bool Transient = false) {Fail(std::string(Why),Transient);};
   virtual void Fail(std::string Why, bool Transient = false);
   virtual void URIStart(FetchResult &Res);
   virtual void URIDone(FetchResult &Res,FetchResult *Alt = 0);

   bool MediaFail(std::string Required,std::string Drive);
   virtual void Exit() {};

   void PrintStatus(char const * const header, const char* Format, va_list &args) const;

   public:
   enum CnfFlags {SingleInstance = (1<<0),
                  Pipeline = (1<<1), SendConfig = (1<<2),
                  LocalOnly = (1<<3), NeedsCleanup = (1<<4), 
                  Removable = (1<<5)};

   void Log(const char *Format,...);
   void Status(const char *Format,...);
   
   void Redirect(const std::string &NewURI);
 
   int Run(bool Single = false);
   inline void SetFailReason(std::string Msg) {FailReason = Msg;};
   inline void SetIP(std::string aIP) {IP = aIP;};
   
   pkgAcqMethod(const char *Ver,unsigned long Flags = 0);
   virtual ~pkgAcqMethod() {};

   private:
   void Dequeue();
};

/** @} */

#endif
