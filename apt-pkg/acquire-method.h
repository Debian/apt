// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.h,v 1.2 1998/11/01 05:27:32 jgg Exp $
/* ######################################################################

   Acquire Method - Method helper class + functions
   
   These functions are designed to be used within the method task to
   ease communication with APT.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_METHOD_H
#define PKGLIB_ACQUIRE_METHOD_H

#include <apt-pkg/configuration.h>
#include <strutl.h>

#ifdef __GNUG__
#pragma interface "apt-pkg/acquire-method.h"
#endif 

class pkgAcqMethod
{
   protected:

   struct FetchItem
   {
      FetchItem *Next;

      string Uri;
      string DestFile;
      time_t LastModified;
   };
   
   
   struct FetchResult
   {
      string MD5Sum;
      time_t LastModified;
      bool IMSHit;
      string Filename;
      unsigned long Size;
      unsigned long ResumePoint;      
      FetchResult();
   };

   // State
   vector<string> Messages;
   FetchItem *Queue;
      
   // Handlers for messages
   virtual bool Configuration(string Message);
   virtual bool Fetch(FetchItem *Item) {return true;};
   
   // Outgoing messages
   void Fail();
   void Fail(string Why);
   void URIStart(FetchResult &Res);
   void URIDone(FetchResult &Res,FetchResult *Alt = 0);
		 
   public:
   
   enum CnfFlags {SingleInstance = (1<<0), PreScan = (1<<1), 
                  Pipeline = (1<<2), SendConfig = (1<<3)};

   void Log(const char *Format,...);
   void Status(const char *Format,...);
   
   int Run(bool Single = false);
   
   pkgAcqMethod(const char *Ver,unsigned long Flags = 0);
   virtual ~pkgAcqMethod() {};
};

#endif
