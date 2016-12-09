// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP acquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTP_H
#define APT_HTTP_H

#include <apt-pkg/strutl.h>

#include <string>
#include <sys/time.h>
#include <iostream>

#include "basehttp.h"

using std::cout;
using std::endl;

class FileFd;
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
   // total amount of data that got written so far
   unsigned long long TotalWriten;

   // Read data in
   bool Read(int Fd);
   bool Read(std::string const &Data);

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

   void Reset();
   // Dump everything
   void Stats();

   CircleBuf(HttpMethod const * const Owner, unsigned long long Size);
   ~CircleBuf();
};

struct HttpServerState: public ServerState
{
   // This is the connection itself. Output is data FROM the server
   CircleBuf In;
   CircleBuf Out;
   int ServerFd;

   protected:
   virtual bool ReadHeaderLines(std::string &Data) APT_OVERRIDE;
   virtual bool LoadNextResponse(bool const ToFile, RequestState &Req) APT_OVERRIDE;
   virtual bool WriteResponse(std::string const &Data) APT_OVERRIDE;

   public:
   virtual void Reset() APT_OVERRIDE;

   virtual bool RunData(RequestState &Req) APT_OVERRIDE;
   virtual bool RunDataToDevNull(RequestState &Req) APT_OVERRIDE;

   virtual bool Open() APT_OVERRIDE;
   virtual bool IsOpen() APT_OVERRIDE;
   virtual bool Close() APT_OVERRIDE;
   virtual bool InitHashes(HashStringList const &ExpectedHashes) APT_OVERRIDE;
   virtual Hashes * GetHashes() APT_OVERRIDE;
   virtual bool Die(RequestState &Req) APT_OVERRIDE;
   virtual bool Flush(FileFd * const File) APT_OVERRIDE;
   virtual bool Go(bool ToFile, RequestState &Req) APT_OVERRIDE;

   HttpServerState(URI Srv, HttpMethod *Owner);
   virtual ~HttpServerState() {Close();};
};

class HttpMethod : public BaseHttpMethod
{
   public:
   virtual void SendReq(FetchItem *Itm) APT_OVERRIDE;

   virtual std::unique_ptr<ServerState> CreateServerState(URI const &uri) APT_OVERRIDE;
   virtual void RotateDNS() APT_OVERRIDE;
   virtual DealWithHeadersResult DealWithHeaders(FetchResult &Res, RequestState &Req) APT_OVERRIDE;

   protected:
   std::string AutoDetectProxyCmd;

   public:
   friend struct HttpServerState;

   explicit HttpMethod(std::string &&pProg);
};

#endif
