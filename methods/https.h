// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP acquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTPS_H
#define APT_HTTPS_H

#include <apt-pkg/acquire-method.h>

#include <curl/curl.h>
#include <iostream>
#include <stddef.h>
#include <string>

#include "server.h"

using std::cout;
using std::endl;

class Hashes;
class HttpsMethod;
class FileFd;

class HttpsServerState : public ServerState
{
   protected:
   virtual bool ReadHeaderLines(std::string &/*Data*/) { return false; }
   virtual bool LoadNextResponse(bool const /*ToFile*/, FileFd * const /*File*/) { return false; }

   public:
   virtual bool WriteResponse(std::string const &/*Data*/) { return false; }

   /** \brief Transfer the data from the socket */
   virtual bool RunData(FileFd * const /*File*/) { return false; }

   virtual bool Open() { return false; }
   virtual bool IsOpen() { return false; }
   virtual bool Close() { return false; }
   virtual bool InitHashes(FileFd &/*File*/) { return false; }
   virtual Hashes * GetHashes() { return NULL; }
   virtual bool Die(FileFd &/*File*/) { return false; }
   virtual bool Flush(FileFd * const /*File*/) { return false; }
   virtual bool Go(bool /*ToFile*/, FileFd * const /*File*/) { return false; }

   HttpsServerState(URI Srv, HttpsMethod *Owner);
   virtual ~HttpsServerState() {Close();};
};

class HttpsMethod : public pkgAcqMethod
{
   // minimum speed in bytes/se that triggers download timeout handling
   static const int DL_MIN_SPEED = 10;

   virtual bool Fetch(FetchItem *);
   static size_t parse_header(void *buffer, size_t size, size_t nmemb, void *userp);
   static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
   static int progress_callback(void *clientp, double dltotal, double dlnow, 
				double ultotal, double ulnow);
   void SetupProxy();
   CURL *curl;
   FetchResult Res;
   HttpsServerState *Server;

   public:
   FileFd *File;
      
   HttpsMethod() : pkgAcqMethod("1.2",Pipeline | SendConfig), File(NULL)
   {
      File = 0;
      curl = curl_easy_init();
   };

   ~HttpsMethod()
   {
      curl_easy_cleanup(curl);
   };
};

#include <apt-pkg/strutl.h>
URI Proxy;

#endif
