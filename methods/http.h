// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   HTTP Acquire Method - This is the HTTP acquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTP_H
#define APT_HTTP_H

#include <apt-pkg/strutl.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <sys/time.h>

#include "basehttp.h"
#include "connect.h"

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

   static unsigned long long BwReadLimit;
   static unsigned long long BwTickReadData;
   static std::chrono::steady_clock::duration BwReadTick;
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
   bool Read(std::unique_ptr<MethodFd> const &Fd);
   bool Read(std::string const &Data);

   // Write data out
   bool Write(std::unique_ptr<MethodFd> const &Fd);
   bool Write(std::string &Data);
   bool WriteTillEl(std::string &Data,bool Single = false);

   // Control the write limit
   void Limit(long long Max) {if (Max == -1) MaxGet = 0-1; else MaxGet = OutP + Max;}
   bool IsLimit() const {return MaxGet == OutP;};
   void Print() const {cout << MaxGet << ',' << OutP << endl;};

   // Test for free space in the buffer
   bool ReadSpace() const {return Size - (InP - OutP) > 0;};
   bool WriteSpace() const {return InP - OutP > 0;};

   void Reset();

   CircleBuf(HttpMethod const * const Owner, unsigned long long Size);
   ~CircleBuf();
};

struct HttpServerState final : public ServerState
{
   // This is the connection itself. Output is data FROM the server
   CircleBuf In;
   CircleBuf Out;
   std::unique_ptr<MethodFd> ServerFd;

   protected:
   bool ReadHeaderLines(std::string &Data) override;
   ResultState LoadNextResponse(bool ToFile, RequestState &Req) override;
   bool WriteResponse(std::string const &Data) override;

   public:
   void Reset() override;

   ResultState RunData(RequestState &Req) override;
   ResultState RunDataToDevNull(RequestState &Req) override;

   ResultState Open() override;
   bool IsOpen() override;
   bool Close() override;
   bool InitHashes(HashStringList const &ExpectedHashes) override;
   Hashes * GetHashes() override;
   ResultState Die(RequestState &Req) override;
   bool Flush(FileFd *File, bool MustComplete = true) override;
   ResultState Go(bool ToFile, RequestState &Req) override;

   HttpServerState(URI Srv, HttpMethod *Owner);
   ~HttpServerState() override {Close();};
};

class HttpMethod final : public BaseHttpMethod
{
   public:
   void SendReq(FetchItem *Itm) override;

   std::unique_ptr<ServerState> CreateServerState(URI const &uri) override;
   void RotateDNS() override;
   DealWithHeadersResult DealWithHeaders(FetchResult &Res, RequestState &Req) override;

   protected:
   std::string AutoDetectProxyCmd;

   public:
   friend struct HttpServerState;

   explicit HttpMethod(std::string &&pProg);
};

#endif
