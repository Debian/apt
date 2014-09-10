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
#include <apt-pkg/acquire-method.h>

#include <string>
#include <sys/time.h>
#include <iostream>

#include "server.h"

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

struct HttpServerState: public ServerState
{
   // This is the connection itself. Output is data FROM the server
   CircleBuf In;
   CircleBuf Out;
   int ServerFd;

   protected:
   virtual bool ReadHeaderLines(std::string &Data);
   virtual bool LoadNextResponse(bool const ToFile, FileFd * const File);
   virtual bool WriteResponse(std::string const &Data);

   public:
   virtual void Reset() { ServerState::Reset(); ServerFd = -1; };

   virtual bool RunData(FileFd * const File);

   virtual bool Open();
   virtual bool IsOpen();
   virtual bool Close();
   virtual bool InitHashes(FileFd &File);
   virtual Hashes * GetHashes();
   virtual bool Die(FileFd &File);
   virtual bool Flush(FileFd * const File);
   virtual bool Go(bool ToFile, FileFd * const File);

   HttpServerState(URI Srv, HttpMethod *Owner);
   virtual ~HttpServerState() {Close();};
};

class HttpMethod : public ServerMethod
{
   public:
   virtual void SendReq(FetchItem *Itm);

   virtual bool Configuration(std::string Message);

   virtual ServerState * CreateServerState(URI uri);
   virtual void RotateDNS();

   protected:
   std::string AutoDetectProxyCmd;

   public:
   friend struct HttpServerState;

   HttpMethod() : ServerMethod("1.2",Pipeline | SendConfig)
   {
      File = 0;
      Server = 0;
   };
};

#endif
