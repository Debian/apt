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
#include <apt-pkg/acquire-method.h>

#include <time.h>
#include <iostream>
#include <string>
#include <memory>

using std::cout;
using std::endl;

class Hashes;
class ServerMethod;
class FileFd;

struct ServerState
{
   // This is the last parsed Header Line
   unsigned int Major;
   unsigned int Minor;
   unsigned int Result;
   char Code[360];

   // These are some statistics from the last parsed header lines

   // total size of the usable content (aka: the file)
   unsigned long long TotalFileSize;
   // size we actually download (can be smaller than Size if we have partial content)
   unsigned long long DownloadSize;
   // size of junk content (aka: server error pages)
   unsigned long long JunkSize;
   // The start of the data (for partial content)
   unsigned long long StartPos;

   time_t Date;
   bool HaveContent;
   enum {Chunked,Stream,Closes} Encoding;
   enum {Header, Data} State;
   bool Persistent;
   std::string Location;

   // This is a Persistent attribute of the server itself.
   bool Pipeline;
   URI ServerName;
   URI Proxy;
   unsigned long TimeOut;

   unsigned long long MaximumSize;

   protected:
   ServerMethod *Owner;

   virtual bool ReadHeaderLines(std::string &Data) = 0;
   virtual bool LoadNextResponse(bool const ToFile, FileFd * const File) = 0;

   public:
   bool HeaderLine(std::string Line);

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
   RunHeadersResult RunHeaders(FileFd * const File, const std::string &Uri);
   bool AddPartialFileToHashes(FileFd &File);

   bool Comp(URI Other) const {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   virtual void Reset() {Major = 0; Minor = 0; Result = 0; Code[0] = '\0'; TotalFileSize = 0; JunkSize = 0;
		 StartPos = 0; Encoding = Closes; time(&Date); HaveContent = false;
		 State = Header; Persistent = false; Pipeline = true; MaximumSize = 0;};
   virtual bool WriteResponse(std::string const &Data) = 0;

   /** \brief Transfer the data from the socket */
   virtual bool RunData(FileFd * const File) = 0;

   virtual bool Open() = 0;
   virtual bool IsOpen() = 0;
   virtual bool Close() = 0;
   virtual bool InitHashes(HashStringList const &ExpectedHashes) = 0;
   virtual Hashes * GetHashes() = 0;
   virtual bool Die(FileFd &File) = 0;
   virtual bool Flush(FileFd * const File) = 0;
   virtual bool Go(bool ToFile, FileFd * const File) = 0;

   ServerState(URI Srv, ServerMethod *Owner);
   virtual ~ServerState() {};
};

class ServerMethod : public pkgAcqMethod
{
   protected:
   virtual bool Fetch(FetchItem *) APT_OVERRIDE;

   std::unique_ptr<ServerState> Server;
   std::string NextURI;
   FileFd *File;

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
   DealWithHeadersResult DealWithHeaders(FetchResult &Res);

   // In the event of a fatal signal this file will be closed and timestamped.
   static std::string FailFile;
   static int FailFd;
   static time_t FailTime;
   static APT_NORETURN void SigTerm(int);

   virtual bool Configuration(std::string Message) APT_OVERRIDE;
   virtual bool Flush() { return Server->Flush(File); };

   int Loop();

   virtual void SendReq(FetchItem *Itm) = 0;
   virtual std::unique_ptr<ServerState> CreateServerState(URI const &uri) = 0;
   virtual void RotateDNS() = 0;

   ServerMethod(const char *Ver,unsigned long Flags = 0);
   virtual ~ServerMethod() {};
};

#endif
