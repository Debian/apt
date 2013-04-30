// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP aquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTP_H
#define APT_HTTP_H

#include <apt-pkg/strutl.h>

#include <string>

using std::cout;
using std::endl;

class HttpMethod;
class Hashes;

class CircleBuf
{
   unsigned char *Buf;
   unsigned long long Size;
   unsigned long long InP;
   unsigned long long OutP;
   std::string OutQueue;
   unsigned long long StrPos;
   unsigned long long MaxGet;
   struct timeval Start;
   
   static unsigned long long BwReadLimit;
   static unsigned long long BwTickReadData;
   static struct timeval BwReadTick;
   static const unsigned int BW_HZ;

   unsigned long long LeftRead() const
   {
      unsigned long long Sz = Size - (InP - OutP);
      if (Sz > Size - (InP%Size))
	 Sz = Size - (InP%Size);
      return Sz;
   }
   unsigned long long LeftWrite() const
   {
      unsigned long long Sz = InP - OutP;
      if (InP > MaxGet)
	 Sz = MaxGet - OutP;
      if (Sz > Size - (OutP%Size))
	 Sz = Size - (OutP%Size);
      return Sz;
   }
   void FillOut();
   
   public:
   
   Hashes *Hash;
   
   // Read data in
   bool Read(int Fd);
   bool Read(std::string Data);
   
   // Write data out
   bool Write(int Fd);
   bool WriteTillEl(std::string &Data,bool Single = false);
   
   // Control the write limit
   void Limit(long long Max) {if (Max == -1) MaxGet = 0-1; else MaxGet = OutP + Max;}   
   bool IsLimit() const {return MaxGet == OutP;};
   void Print() const {cout << MaxGet << ',' << OutP << endl;};

   // Test for free space in the buffer
   bool ReadSpace() const {return Size - (InP - OutP) > 0;};
   bool WriteSpace() const {return InP - OutP > 0;};

   // Dump everything
   void Reset();
   void Stats();

   CircleBuf(unsigned long long Size);
   ~CircleBuf();
};

struct ServerState
{
   // This is the last parsed Header Line
   unsigned int Major;
   unsigned int Minor;
   unsigned int Result;
   char Code[360];
   
   // These are some statistics from the last parsed header lines
   unsigned long long Size;
   signed long long StartPos;
   time_t Date;
   bool HaveContent;
   enum {Chunked,Stream,Closes} Encoding;
   enum {Header, Data} State;
   bool Persistent;
   std::string Location;
   
   // This is a Persistent attribute of the server itself.
   bool Pipeline;
   
   HttpMethod *Owner;
   
   // This is the connection itself. Output is data FROM the server
   CircleBuf In;
   CircleBuf Out;
   int ServerFd;
   URI ServerName;
  
   bool HeaderLine(std::string Line);
   bool Comp(URI Other) const {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   void Reset() {Major = 0; Minor = 0; Result = 0; Code[0] = '\0'; Size = 0;
		 StartPos = 0; Encoding = Closes; time(&Date); HaveContent = false;
		 State = Header; Persistent = false; ServerFd = -1;
		 Pipeline = true;};

   /** \brief Result of the header acquire */
   enum RunHeadersResult {
      /** \brief Header ok */
      RUN_HEADERS_OK,
      /** \brief IO error while retrieving */
      RUN_HEADERS_IO_ERROR,
      /** \brief Parse error after retrieving */
      RUN_HEADERS_PARSE_ERROR,
   };
   /** \brief Get the headers before the data */
   RunHeadersResult RunHeaders();
   /** \brief Transfer the data from the socket */
   bool RunData();
   
   bool Open();
   bool Close();
   
   ServerState(URI Srv,HttpMethod *Owner);
   ~ServerState() {Close();};
};

class HttpMethod : public pkgAcqMethod
{
   void SendReq(FetchItem *Itm,CircleBuf &Out);
   bool Go(bool ToFile,ServerState *Srv);
   bool Flush(ServerState *Srv);
   bool ServerDie(ServerState *Srv);

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
   DealWithHeadersResult DealWithHeaders(FetchResult &Res,ServerState *Srv);

   /** \brief Try to AutoDetect the proxy */
   bool AutoDetectProxy();

   virtual bool Configuration(std::string Message);
   
   // In the event of a fatal signal this file will be closed and timestamped.
   static std::string FailFile;
   static int FailFd;
   static time_t FailTime;
   static void SigTerm(int);

   protected:
   virtual bool Fetch(FetchItem *);
   
   std::string NextURI;
   std::string AutoDetectProxyCmd;

   public:
   friend struct ServerState;

   FileFd *File;
   ServerState *Server;
   
   int Loop();
   
   HttpMethod() : pkgAcqMethod("1.2",Pipeline | SendConfig) 
   {
      File = 0;
      Server = 0;
   };
};

#endif
