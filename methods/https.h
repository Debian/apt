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
#include <memory>

#include "server.h"

using std::cout;
using std::endl;

class Hashes;
class HttpsMethod;
class FileFd;

class HttpsServerState : public ServerState
{
   Hashes * Hash;

   protected:
   virtual bool ReadHeaderLines(std::string &/*Data*/) APT_OVERRIDE { return false; }
   virtual bool LoadNextResponse(bool const /*ToFile*/, FileFd * const /*File*/) APT_OVERRIDE { return false; }

   public:
   virtual bool WriteResponse(std::string const &/*Data*/) APT_OVERRIDE { return false; }

   /** \brief Transfer the data from the socket */
   virtual bool RunData(FileFd * const /*File*/) APT_OVERRIDE { return false; }

   virtual bool Open() APT_OVERRIDE { return false; }
   virtual bool IsOpen() APT_OVERRIDE { return false; }
   virtual bool Close() APT_OVERRIDE { return false; }
   virtual bool InitHashes(HashStringList const &ExpectedHashes) APT_OVERRIDE;
   virtual Hashes * GetHashes() APT_OVERRIDE;
   virtual bool Die(FileFd &/*File*/) APT_OVERRIDE { return false; }
   virtual bool Flush(FileFd * const /*File*/) APT_OVERRIDE { return false; }
   virtual bool Go(bool /*ToFile*/, FileFd * const /*File*/) APT_OVERRIDE { return false; }

   HttpsServerState(URI Srv, HttpsMethod *Owner);
   virtual ~HttpsServerState() {Close();};
};

class HttpsMethod : public ServerMethod
{
   // minimum speed in bytes/se that triggers download timeout handling
   static const int DL_MIN_SPEED = 10;

   virtual bool Fetch(FetchItem *) APT_OVERRIDE;

   static size_t parse_header(void *buffer, size_t size, size_t nmemb, void *userp);
   static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
   static int progress_callback(void *clientp, double dltotal, double dlnow,
				 double ultotal, double ulnow);
   void SetupProxy();
   CURL *curl;
   std::unique_ptr<ServerState> Server;

   // Used by ServerMethods unused by https
   virtual void SendReq(FetchItem *) APT_OVERRIDE { exit(42); }
   virtual void RotateDNS() APT_OVERRIDE { exit(42); }

   public:
   FileFd *File;

   virtual bool Configuration(std::string Message) APT_OVERRIDE;
   virtual std::unique_ptr<ServerState> CreateServerState(URI const &uri) APT_OVERRIDE;
   using pkgAcqMethod::FetchResult;
   using pkgAcqMethod::FetchItem;

   HttpsMethod() : ServerMethod("1.2",Pipeline | SendConfig), File(NULL)
   {
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
