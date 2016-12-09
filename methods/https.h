// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP acquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTPS_H
#define APT_HTTPS_H

#include <curl/curl.h>
#include <iostream>
#include <stddef.h>
#include <string>
#include <memory>

#include "basehttp.h"

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
   virtual bool LoadNextResponse(bool const /*ToFile*/, RequestState &/*Req*/) APT_OVERRIDE { return false; }

   public:
   virtual bool WriteResponse(std::string const &/*Data*/) APT_OVERRIDE { return false; }

   /** \brief Transfer the data from the socket */
   virtual bool RunData(RequestState &) APT_OVERRIDE { return false; }
   virtual bool RunDataToDevNull(RequestState &) APT_OVERRIDE { return false; }

   virtual bool Open() APT_OVERRIDE { return false; }
   virtual bool IsOpen() APT_OVERRIDE { return false; }
   virtual bool Close() APT_OVERRIDE { return false; }
   virtual bool InitHashes(HashStringList const &ExpectedHashes) APT_OVERRIDE;
   virtual Hashes * GetHashes() APT_OVERRIDE;
   virtual bool Die(RequestState &/*Req*/) APT_OVERRIDE { return false; }
   virtual bool Flush(FileFd * const /*File*/) APT_OVERRIDE { return false; }
   virtual bool Go(bool /*ToFile*/, RequestState &/*Req*/) APT_OVERRIDE { return false; }

   HttpsServerState(URI Srv, HttpsMethod *Owner);
   virtual ~HttpsServerState() {Close();};
};

class HttpsMethod : public BaseHttpMethod
{
   // minimum speed in bytes/se that triggers download timeout handling
   static const int DL_MIN_SPEED = 10;

   virtual bool Fetch(FetchItem *) APT_OVERRIDE;

   static size_t parse_header(void *buffer, size_t size, size_t nmemb, void *userp);
   static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
   static int progress_callback(void *clientp, double dltotal, double dlnow,
				 double ultotal, double ulnow);
   bool SetupProxy();
   CURL *curl;

   // Used by BaseHttpMethods unused by https
   virtual void SendReq(FetchItem *) APT_OVERRIDE { exit(42); }
   virtual void RotateDNS() APT_OVERRIDE { exit(42); }

   public:

   virtual std::unique_ptr<ServerState> CreateServerState(URI const &uri) APT_OVERRIDE;
   using pkgAcqMethod::FetchResult;
   using pkgAcqMethod::FetchItem;

   explicit HttpsMethod(std::string &&pProg);
   virtual ~HttpsMethod();
};

#include <apt-pkg/strutl.h>
URI Proxy;

#endif
