// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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

#include <apt-pkg/hashes.h>
#include <apt-pkg/macros.h>

#include <cstdarg>
#include <ctime>

#include <string>
#include <unordered_map>
#include <vector>


class APT_PUBLIC pkgAcqMethod
{
   protected:

   struct FetchItem
   {
      FetchItem *Next;

      std::string Uri;
      std::string DestFile;
      int DestFileFd;
      time_t LastModified;
      bool IndexFile;
      bool FailIgnore;
      HashStringList ExpectedHashes;
      // a maximum size we will download, this can be the exact filesize
      // for when we know it or a arbitrary limit when we don't know the
      // filesize (like a InRelease file)
      unsigned long long MaximumSize;

      FetchItem();
      virtual ~FetchItem();
      std::string Proxy(); // For internal use only.
      void Proxy(std::string const &Proxy) APT_HIDDEN;

      private:
      struct Private;
      Private *const d;
   };

   struct FetchResult
   {
      HashStringList Hashes;
      std::vector<std::string> GPGVOutput;
      time_t LastModified;
      bool IMSHit;
      std::string Filename;
      unsigned long long Size;
      unsigned long long ResumePoint;
      
      void TakeHashes(class Hashes &Hash);
      FetchResult();
      virtual ~FetchResult();
      private:
      void * const d;
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
   virtual bool URIAcquire(std::string const &/*Message*/, FetchItem *Itm) { return Fetch(Itm); };

   // Outgoing messages
   void Fail(bool Transient = false);
   inline void Fail(const char *Why, bool Transient = false) {Fail(std::string(Why),Transient);};
   virtual void Fail(std::string Why, bool Transient = false);
   virtual void URIStart(FetchResult &Res);
   virtual void URIDone(FetchResult &Res,FetchResult *Alt = 0);
   void SendMessage(std::string const &header, std::unordered_map<std::string, std::string> &&fields);

   bool MediaFail(std::string Required,std::string Drive);
   virtual void Exit() {};

   APT_DEPRECATED_MSG("Use SendMessage instead") void PrintStatus(char const * const header, const char* Format, va_list &args) const;

   public:
   enum CnfFlags
   {
      SingleInstance = (1 << 0),
      Pipeline = (1 << 1),
      SendConfig = (1 << 2),
      LocalOnly = (1 << 3),
      NeedsCleanup = (1 << 4),
      Removable = (1 << 5),
      AuxRequests = (1 << 6),
      SendURIEncoded = (1 << 7),
   };

   void Log(const char *Format,...);
   void Status(const char *Format,...);
   
   void Redirect(const std::string &NewURI);
 
   int Run(bool Single = false);
   inline void SetFailReason(std::string Msg) {FailReason = Msg;};
   inline void SetIP(std::string aIP) {IP = aIP;};
   
   pkgAcqMethod(const char *Ver,unsigned long Flags = 0);
   virtual ~pkgAcqMethod();
   void DropPrivsOrDie();
   private:
   APT_HIDDEN void Dequeue();
};

/** @} */

#endif
