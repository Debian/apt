// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.h,v 1.1 1998/10/30 07:53:35 jgg Exp $
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
   
   string CurrentURI;
   string DestFile;
   time_t LastModified;

   vector<string> Messages;
   
   struct FetchResult
   {
      string MD5Sum;
      time_t LastModified;
      bool IMSHit;
      string Filename;
      unsigned long Size;
      FetchResult();
   };
   
   // Handlers for messages
   virtual bool Configuration(string Message);
   virtual bool Fetch(string Message,URI Get) {return true;};
   
   // Outgoing messages
   void Fail();
   void Fail(string Why);
//   void Log(const char *Format,...);
   void URIStart(FetchResult &Res,unsigned long Resume = 0);
   void URIDone(FetchResult &Res,FetchResult *Alt = 0);
		 
   public:
   
   enum CnfFlags {SingleInstance = (1<<0), PreScan = (1<<1), 
                  Pipeline = (1<<2), SendConfig = (1<<3)};

   int Run();
   
   pkgAcqMethod(const char *Ver,unsigned long Flags = 0);
   virtual ~pkgAcqMethod() {};
};

#endif
