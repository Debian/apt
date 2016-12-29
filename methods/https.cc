//-*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: http.cc,v 1.59 2004/05/08 19:42:35 mdz Exp $
/* ######################################################################

   HTTPS Acquire Method - This is the HTTPS acquire method for APT.
   
   It uses libcurl

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/netrc.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/proxy.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include <array>
#include <iostream>
#include <sstream>


#include "https.h"

#include <apti18n.h>
									/*}}}*/
using namespace std;

struct APT_HIDDEN CURLUserPointer {
   HttpsMethod * const https;
   HttpsMethod::FetchResult * const Res;
   HttpsMethod::FetchItem const * const Itm;
   RequestState * const Req;
   CURLUserPointer(HttpsMethod * const https, HttpsMethod::FetchResult * const Res,
	 HttpsMethod::FetchItem const * const Itm, RequestState * const Req) : https(https), Res(Res), Itm(Itm), Req(Req) {}
};

size_t
HttpsMethod::parse_header(void *buffer, size_t size, size_t nmemb, void *userp)
{
   size_t len = size * nmemb;
   CURLUserPointer *me = static_cast<CURLUserPointer *>(userp);
   std::string line((char*) buffer, len);
   for (--len; len > 0; --len)
      if (isspace_ascii(line[len]) == 0)
      {
	 ++len;
	 break;
      }
   line.erase(len);

   if (line.empty() == true)
   {
      if (me->Req->File.Open(me->Itm->DestFile, FileFd::WriteAny) == false)
	 return ERROR_NOT_FROM_SERVER;

      me->Req->JunkSize = 0;
      if (me->Req->Result != 416 && me->Req->StartPos != 0)
	 ;
      else if (me->Req->Result == 416)
      {
	 bool partialHit = false;
	 if (me->Itm->ExpectedHashes.usable() == true)
	 {
	    Hashes resultHashes(me->Itm->ExpectedHashes);
	    FileFd file(me->Itm->DestFile, FileFd::ReadOnly);
	    me->Req->TotalFileSize = file.FileSize();
	    me->Req->Date = file.ModificationTime();
	    resultHashes.AddFD(file);
	    HashStringList const hashList = resultHashes.GetHashStringList();
	    partialHit = (me->Itm->ExpectedHashes == hashList);
	 }
	 else if (me->Req->Result == 416 && me->Req->TotalFileSize == me->Req->File.FileSize())
	    partialHit = true;

	 if (partialHit == true)
	 {
	    me->Req->Result = 200;
	    me->Req->StartPos = me->Req->TotalFileSize;
	    // the actual size is not important for https as curl will deal with it
	    // by itself and e.g. doesn't bother us with transport-encoding…
	    me->Req->JunkSize = std::numeric_limits<unsigned long long>::max();
	 }
	 else
	    me->Req->StartPos = 0;
      }
      else
	 me->Req->StartPos = 0;

      me->Res->LastModified = me->Req->Date;
      me->Res->Size = me->Req->TotalFileSize;
      me->Res->ResumePoint = me->Req->StartPos;

      // we expect valid data, so tell our caller we get the file now
      if (me->Req->Result >= 200 && me->Req->Result < 300)
      {
	 if (me->Res->Size != 0 && me->Res->Size > me->Res->ResumePoint)
	    me->https->URIStart(*me->Res);
	 if (me->Req->AddPartialFileToHashes(me->Req->File) == false)
	    return 0;
      }
      else
	 me->Req->JunkSize = std::numeric_limits<decltype(me->Req->JunkSize)>::max();
   }
   else if (me->Req->HeaderLine(line) == false)
      return 0;

   return size*nmemb;
}

size_t 
HttpsMethod::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   CURLUserPointer *me = static_cast<CURLUserPointer *>(userp);
   size_t buffer_size = size * nmemb;
   // we don't need to count the junk here, just drop anything we get as
   // we don't always know how long it would be, e.g. in chunked encoding.
   if (me->Req->JunkSize != 0)
      return buffer_size;

   if(me->Req->File.Write(buffer, buffer_size) != true)
      return 0;

   if(me->https->Queue->MaximumSize > 0)
   {
      unsigned long long const TotalWritten = me->Req->File.Tell();
      if (TotalWritten > me->https->Queue->MaximumSize)
      {
	 me->https->SetFailReason("MaximumSizeExceeded");
	 _error->Error("Writing more data than expected (%llu > %llu)",
	       TotalWritten, me->https->Queue->MaximumSize);
	 return 0;
      }
   }

   if (me->https->Server->GetHashes()->Add((unsigned char const * const)buffer, buffer_size) == false)
      return 0;

   return buffer_size;
}

// HttpsServerState::HttpsServerState - Constructor			/*{{{*/
HttpsServerState::HttpsServerState(URI Srv,HttpsMethod * Owner) : ServerState(Srv, Owner), Hash(NULL)
{
   TimeOut = Owner->ConfigFindI("Timeout", TimeOut);
   Reset();
}
									/*}}}*/
bool HttpsServerState::InitHashes(HashStringList const &ExpectedHashes)	/*{{{*/
{
   delete Hash;
   Hash = new Hashes(ExpectedHashes);
   return true;
}
									/*}}}*/
APT_PURE Hashes * HttpsServerState::GetHashes()				/*{{{*/
{
   return Hash;
}
									/*}}}*/

bool HttpsMethod::SetupProxy()						/*{{{*/
{
   URI ServerName = Queue->Uri;

   // Determine the proxy setting
   AutoDetectProxy(ServerName);

   // Curl should never read proxy settings from the environment, as
   // we determine which proxy to use.  Do this for consistency among
   // methods and prevent an environment variable overriding a
   // no-proxy ("DIRECT") setting in apt.conf.
   curl_easy_setopt(curl, CURLOPT_PROXY, "");

   // Determine the proxy setting - try https first, fallback to http and use env at last
   string UseProxy = ConfigFind("Proxy::" + ServerName.Host, "");
   if (UseProxy.empty() == true)
      UseProxy = ConfigFind("Proxy", "");
   // User wants to use NO proxy, so nothing to setup
   if (UseProxy == "DIRECT")
      return true;

   // Parse no_proxy, a comma (,) separated list of domains we don't want to use
   // a proxy for so we stop right here if it is in the list
   if (getenv("no_proxy") != 0 && CheckDomainList(ServerName.Host,getenv("no_proxy")) == true)
      return true;

   if (UseProxy.empty() == true)
   {
      const char* result = nullptr;
      if (std::find(methodNames.begin(), methodNames.end(), "https") != methodNames.end())
	 result = getenv("https_proxy");
      // FIXME: Fall back to http_proxy is to remain compatible with
      // existing setups and behaviour of apt.conf.  This should be
      // deprecated in the future (including apt.conf).  Most other
      // programs do not fall back to http proxy settings and neither
      // should Apt.
      if (result == nullptr && std::find(methodNames.begin(), methodNames.end(), "http") != methodNames.end())
	 result = getenv("http_proxy");
      UseProxy = result == nullptr ? "" : result;
   }

   // Determine what host and port to use based on the proxy settings
   if (UseProxy.empty() == false)
   {
      Proxy = UseProxy;
      AddProxyAuth(Proxy, ServerName);

      if (Proxy.Access == "socks5h")
	 curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5_HOSTNAME);
      else if (Proxy.Access == "socks5")
	 curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
      else if (Proxy.Access == "socks4a")
	 curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
      else if (Proxy.Access == "socks")
	 curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
      else if (Proxy.Access == "http" || Proxy.Access == "https")
	 curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
      else
	 return false;

      if (Proxy.Port != 1)
	 curl_easy_setopt(curl, CURLOPT_PROXYPORT, Proxy.Port);
      curl_easy_setopt(curl, CURLOPT_PROXY, Proxy.Host.c_str());
      if (Proxy.User.empty() == false || Proxy.Password.empty() == false)
      {
         curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, Proxy.User.c_str());
         curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, Proxy.Password.c_str());
      }
   }
   return true;
}									/*}}}*/
// HttpsMethod::Fetch - Fetch an item					/*{{{*/
// ---------------------------------------------------------------------
/* This adds an item to the pipeline. We keep the pipeline at a fixed
   depth. */
bool HttpsMethod::Fetch(FetchItem *Itm)
{
   struct stat SBuf;
   struct curl_slist *headers=NULL;
   char curl_errorstr[CURL_ERROR_SIZE];
   URI Uri = Itm->Uri;
   setPostfixForMethodNames(Uri.Host.c_str());
   AllowRedirect = ConfigFindB("AllowRedirect", true);
   Debug = DebugEnabled();

   // TODO:
   //       - http::Pipeline-Depth
   //       - error checking/reporting
   //       - more debug options? (CURLOPT_DEBUGFUNCTION?)
   {
      auto const plus = Binary.find('+');
      if (plus != std::string::npos)
	 Uri.Access = Binary.substr(plus + 1);
   }

   curl_easy_reset(curl);
   if (SetupProxy() == false)
      return _error->Error("Unsupported proxy configured: %s", URI::SiteOnly(Proxy).c_str());

   maybe_add_auth (Uri, _config->FindFile("Dir::Etc::netrc"));
   if (Server == nullptr || Server->Comp(Itm->Uri) == false)
      Server = CreateServerState(Itm->Uri);

   // The "+" is encoded as a workaround for a amazon S3 bug
   // see LP bugs #1003633 and #1086997. (taken from http method)
   Uri.Path = QuoteString(Uri.Path, "+~ ");

   FetchResult Res;
   RequestState Req(this, Server.get());
   CURLUserPointer userp(this, &Res, Itm, &Req);
   // callbacks
   curl_easy_setopt(curl, CURLOPT_URL, static_cast<string>(Uri).c_str());
   curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, parse_header);
   curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &userp);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &userp);
   // options
   curl_easy_setopt(curl, CURLOPT_NOPROGRESS, true);
   curl_easy_setopt(curl, CURLOPT_FILETIME, true);
   curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0);

   if (std::find(methodNames.begin(), methodNames.end(), "https") != methodNames.end())
   {
      curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
      curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);

      // File containing the list of trusted CA.
      std::string const cainfo = ConfigFind("CaInfo", "");
      if(cainfo.empty() == false)
	 curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo.c_str());
      // Check server certificate against previous CA list ...
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ConfigFindB("Verify-Peer", true) ? 1 : 0);
      // ... and hostname against cert CN or subjectAltName
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ConfigFindB("Verify-Host", true) ? 2 : 0);
      // Also enforce issuer of server certificate using its cert
      std::string const issuercert = ConfigFind("IssuerCert", "");
      if(issuercert.empty() == false)
	 curl_easy_setopt(curl, CURLOPT_ISSUERCERT, issuercert.c_str());
      // For client authentication, certificate file ...
      std::string const pem = ConfigFind("SslCert", "");
      if(pem.empty() == false)
	 curl_easy_setopt(curl, CURLOPT_SSLCERT, pem.c_str());
      // ... and associated key.
      std::string const key = ConfigFind("SslKey", "");
      if(key.empty() == false)
	 curl_easy_setopt(curl, CURLOPT_SSLKEY, key.c_str());
      // Allow forcing SSL version to SSLv3 or TLSv1
      long final_version = CURL_SSLVERSION_DEFAULT;
      std::string const sslversion = ConfigFind("SslForceVersion", "");
      if(sslversion == "TLSv1")
	 final_version = CURL_SSLVERSION_TLSv1;
      else if(sslversion == "TLSv1.0")
	 final_version = CURL_SSLVERSION_TLSv1_0;
      else if(sslversion == "TLSv1.1")
	 final_version = CURL_SSLVERSION_TLSv1_1;
      else if(sslversion == "TLSv1.2")
	 final_version = CURL_SSLVERSION_TLSv1_2;
      else if(sslversion == "SSLv3")
	 final_version = CURL_SSLVERSION_SSLv3;
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, final_version);
      // CRL file
      std::string const crlfile = ConfigFind("CrlFile", "");
      if(crlfile.empty() == false)
	 curl_easy_setopt(curl, CURLOPT_CRLFILE, crlfile.c_str());
   }
   else
   {
      curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
      curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP);
   }
   // cache-control
   if(ConfigFindB("No-Cache", false) == false)
   {
      // cache enabled
      if (ConfigFindB("No-Store", false) == true)
	 headers = curl_slist_append(headers,"Cache-Control: no-store");
      std::string ss;
      strprintf(ss, "Cache-Control: max-age=%u", ConfigFindI("Max-Age", 0));
      headers = curl_slist_append(headers, ss.c_str());
   } else {
      // cache disabled by user
      headers = curl_slist_append(headers, "Cache-Control: no-cache");
      headers = curl_slist_append(headers, "Pragma: no-cache");
   }
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
   // speed limit
   int const dlLimit = ConfigFindI("Dl-Limit", 0) * 1024;
   if (dlLimit > 0)
      curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, dlLimit);

   // set header
   curl_easy_setopt(curl, CURLOPT_USERAGENT, ConfigFind("User-Agent", "Debian APT-CURL/1.0 (" PACKAGE_VERSION ")").c_str());

   // set timeout
   int const timeout = ConfigFindI("Timeout", 120);
   curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);
   //set really low lowspeed timeout (see #497983)
   curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, DL_MIN_SPEED);
   curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, timeout);

   if(_config->FindB("Acquire::ForceIPv4", false) == true)
      curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
   else if(_config->FindB("Acquire::ForceIPv6", false) == true)
      curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);

   // debug
   if (Debug == true)
      curl_easy_setopt(curl, CURLOPT_VERBOSE, true);

   // error handling
   curl_errorstr[0] = '\0';
   curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errorstr);

   // If we ask for uncompressed files servers might respond with content-
   // negotiation which lets us end up with compressed files we do not support,
   // see 657029, 657560 and co, so if we have no extension on the request
   // ask for text only. As a sidenote: If there is nothing to negotate servers
   // seem to be nice and ignore it.
   if (ConfigFindB("SendAccept", true))
   {
      size_t const filepos = Itm->Uri.find_last_of('/');
      string const file = Itm->Uri.substr(filepos + 1);
      if (flExtension(file) == file)
	 headers = curl_slist_append(headers, "Accept: text/*");
   }

   // if we have the file send an if-range query with a range header
   if (Server->RangesAllowed && stat(Itm->DestFile.c_str(),&SBuf) >= 0 && SBuf.st_size > 0)
   {
      std::string Buf;
      strprintf(Buf, "Range: bytes=%lli-", (long long) SBuf.st_size);
      headers = curl_slist_append(headers, Buf.c_str());
      strprintf(Buf, "If-Range: %s", TimeRFC1123(SBuf.st_mtime, false).c_str());
      headers = curl_slist_append(headers, Buf.c_str());
   }
   else if(Itm->LastModified > 0)
   {
      curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
      curl_easy_setopt(curl, CURLOPT_TIMEVALUE, Itm->LastModified);
   }

   if (Server->InitHashes(Itm->ExpectedHashes) == false)
      return false;

   // keep apt updated
   Res.Filename = Itm->DestFile;

   // get it!
   CURLcode success = curl_easy_perform(curl);

   // If the server returns 200 OK but the If-Modified-Since condition is not
   // met, CURLINFO_CONDITION_UNMET will be set to 1
   long curl_condition_unmet = 0;
   curl_easy_getinfo(curl, CURLINFO_CONDITION_UNMET, &curl_condition_unmet);
   if (curl_condition_unmet == 1)
      Req.Result = 304;

   Req.File.Close();
   curl_slist_free_all(headers);

   // cleanup
   if (success != CURLE_OK)
   {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
      switch (success)
      {
	 case CURLE_COULDNT_RESOLVE_PROXY:
	 case CURLE_COULDNT_RESOLVE_HOST:
	    SetFailReason("ResolveFailure");
	    break;
	 case CURLE_COULDNT_CONNECT:
	    SetFailReason("ConnectionRefused");
	    break;
	 case CURLE_OPERATION_TIMEDOUT:
	    SetFailReason("Timeout");
	    break;
      }
#pragma GCC diagnostic pop
      // only take curls technical errors if we haven't our own
      // (e.g. for the maximum size limit we have and curls can be confusing)
      if (_error->PendingError() == false)
	 _error->Error("%s", curl_errorstr);
      else
	 _error->Warning("curl: %s", curl_errorstr);
      return false;
   }

   switch (DealWithHeaders(Res, Req))
   {
      case BaseHttpMethod::IMS_HIT:
	 URIDone(Res);
	 break;

      case BaseHttpMethod::ERROR_WITH_CONTENT_PAGE:
	 // unlink, no need keep 401/404 page content in partial/
	 RemoveFile(Binary.c_str(), Req.File.Name());
      case BaseHttpMethod::ERROR_UNRECOVERABLE:
      case BaseHttpMethod::ERROR_NOT_FROM_SERVER:
	 return false;

      case BaseHttpMethod::TRY_AGAIN_OR_REDIRECT:
	 Redirect(NextURI);
	 break;

      case BaseHttpMethod::FILE_IS_OPEN:
	 struct stat resultStat;
	 if (unlikely(stat(Req.File.Name().c_str(), &resultStat) != 0))
	 {
	    _error->Errno("stat", "Unable to access file %s", Req.File.Name().c_str());
	    return false;
	 }
	 Res.Size = resultStat.st_size;

	 // Timestamp
	 curl_easy_getinfo(curl, CURLINFO_FILETIME, &Res.LastModified);
	 if (Res.LastModified != -1)
	 {
	    struct timeval times[2];
	    times[0].tv_sec = Res.LastModified;
	    times[1].tv_sec = Res.LastModified;
	    times[0].tv_usec = times[1].tv_usec = 0;
	    utimes(Req.File.Name().c_str(), times);
	 }
	 else
	    Res.LastModified = resultStat.st_mtime;

	 // take hashes
	 Res.TakeHashes(*(Server->GetHashes()));

	 // keep apt updated
	 URIDone(Res);
	 break;
   }
   return true;
}
									/*}}}*/
std::unique_ptr<ServerState> HttpsMethod::CreateServerState(URI const &uri)/*{{{*/
{
   return std::unique_ptr<ServerState>(new HttpsServerState(uri, this));
}
									/*}}}*/
HttpsMethod::HttpsMethod(std::string &&pProg) : BaseHttpMethod(std::move(pProg),"1.2",Pipeline | SendConfig)/*{{{*/
{
   auto addName = std::inserter(methodNames, methodNames.begin());
   addName = "http";
   auto const plus = Binary.find('+');
   if (plus != std::string::npos)
   {
      addName = Binary.substr(plus + 1);
      auto base = Binary.substr(0, plus);
      if (base != "https")
	 addName = base;
   }
   if (std::find(methodNames.begin(), methodNames.end(), "https") != methodNames.end())
      curl_global_init(CURL_GLOBAL_SSL);
   else
      curl_global_init(CURL_GLOBAL_NOTHING);
   curl = curl_easy_init();
}
									/*}}}*/
HttpsMethod::~HttpsMethod()						/*{{{*/
{
   curl_easy_cleanup(curl);
}
									/*}}}*/
int main(int, const char *argv[])					/*{{{*/
{
   std::string Binary = flNotDir(argv[0]);
   if (Binary.find('+') == std::string::npos && Binary != "https")
      Binary.append("+https");
   return HttpsMethod(std::move(Binary)).Run();
}
									/*}}}*/
