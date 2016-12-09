// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Classes dealing with the abstraction of talking to a end via a text
   protocol like HTTP (which is used by the http and https methods)

   ##################################################################### */
									/*}}}*/

#ifndef APT_SERVER_H
#define APT_SERVER_H

#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include "aptmethod.h"

#include <time.h>
#include <iostream>
#include <string>
#include <memory>

using std::cout;
using std::endl;

class Hashes;
class BaseHttpMethod;
struct ServerState;

struct RequestState
{
   unsigned int Major = 0;
   unsigned int Minor = 0;
   unsigned int Result = 0;
   char Code[360];

   // total size of the usable content (aka: the file)
   unsigned long long TotalFileSize = 0;
   // size we actually download (can be smaller than Size if we have partial content)
   unsigned long long DownloadSize = 0;
   // size of junk content (aka: server error pages)
   unsigned long long JunkSize = 0;
   // The start of the data (for partial content)
   unsigned long long StartPos = 0;

   unsigned long long MaximumSize = 0;

   time_t Date;
   bool HaveContent = false;
   enum {Chunked,Stream,Closes} Encoding = Closes;
   enum {Header, Data} State = Header;
   std::string Location;

   FileFd File;

   BaseHttpMethod * const Owner;
   ServerState * const Server;

   bool HeaderLine(std::string const &Line);
   bool AddPartialFileToHashes(FileFd &File);

   RequestState(BaseHttpMethod * const Owner, ServerState * const Server) :
      Owner(Owner), Server(Server) { time(&Date); }
};

struct ServerState
{
   bool Persistent;
   bool PipelineAllowed;
   bool RangesAllowed;

   bool Pipeline;
   URI ServerName;
   URI Proxy;
   unsigned long TimeOut;

   protected:
   BaseHttpMethod *Owner;

   virtual bool ReadHeaderLines(std::string &Data) = 0;
   virtual bool LoadNextResponse(bool const ToFile, RequestState &Req) = 0;

   public:

   /** \brief Result of the header acquire */
   enum RunHeadersResult {
      /** \brief Header ok */
      RUN_HEADERS_OK,
      /** \brief IO error while retrieving */
      RUN_HEADERS_IO_ERROR,
      /** \brief Parse error after retrieving */
      RUN_HEADERS_PARSE_ERROR
   };
   /** \brief Get the headers before the data */
   RunHeadersResult RunHeaders(RequestState &Req, const std::string &Uri);

   bool Comp(URI Other) const {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   virtual void Reset();
   virtual bool WriteResponse(std::string const &Data) = 0;

   /** \brief Transfer the data from the socket */
   virtual bool RunData(RequestState &Req) = 0;
   virtual bool RunDataToDevNull(RequestState &Req) = 0;

   virtual bool Open() = 0;
   virtual bool IsOpen() = 0;
   virtual bool Close() = 0;
   virtual bool InitHashes(HashStringList const &ExpectedHashes) = 0;
   virtual bool Die(RequestState &Req) = 0;
   virtual bool Flush(FileFd * const File) = 0;
   virtual bool Go(bool ToFile, RequestState &Req) = 0;
   virtual Hashes * GetHashes() = 0;

   ServerState(URI Srv, BaseHttpMethod *Owner);
   virtual ~ServerState() {};
};

class BaseHttpMethod : public aptMethod
{
   protected:
   virtual bool Fetch(FetchItem *) APT_OVERRIDE;

   std::unique_ptr<ServerState> Server;
   std::string NextURI;

   unsigned long PipelineDepth;
   bool AllowRedirect;

   // Find the biggest item in the fetch queue for the checking of the maximum
   // size
   unsigned long long FindMaximumObjectSizeInQueue() const APT_PURE;

   public:
   bool Debug;

   /** \brief Result of the header parsing */
   enum DealWithHeadersResult {
      /** \brief The file is open and ready */
      FILE_IS_OPEN,
      /** \brief We got a IMS hit, the file has not changed */
      IMS_HIT,
      /** \brief The server reported a unrecoverable error */
      ERROR_UNRECOVERABLE,
      /** \brief The server reported a error with a error content page */
      ERROR_WITH_CONTENT_PAGE,
      /** \brief An error on the client side */
      ERROR_NOT_FROM_SERVER,
      /** \brief A redirect or retry request */
      TRY_AGAIN_OR_REDIRECT
   };
   /** \brief Handle the retrieved header data */
   virtual DealWithHeadersResult DealWithHeaders(FetchResult &Res, RequestState &Req);

   // In the event of a fatal signal this file will be closed and timestamped.
   static std::string FailFile;
   static int FailFd;
   static time_t FailTime;
   static APT_NORETURN void SigTerm(int);

   int Loop();

   virtual void SendReq(FetchItem *Itm) = 0;
   virtual std::unique_ptr<ServerState> CreateServerState(URI const &uri) = 0;
   virtual void RotateDNS() = 0;
   virtual bool Configuration(std::string Message) APT_OVERRIDE;

   bool AddProxyAuth(URI &Proxy, URI const &Server) const;

   BaseHttpMethod(std::string &&Binary, char const * const Ver,unsigned long const Flags);
   virtual ~BaseHttpMethod() {};
};

#endif
