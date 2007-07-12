// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP aquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_HTTP_H
#define APT_HTTP_H

#define MAXLEN 360

#include <iostream>

using std::cout;
using std::endl;

class HttpMethod;

class CircleBuf
{
   unsigned char *Buf;
   unsigned long Size;
   unsigned long InP;
   unsigned long OutP;
   string OutQueue;
   unsigned long StrPos;
   unsigned long MaxGet;
   struct timeval Start;
   
   static unsigned long BwReadLimit;
   static unsigned long BwTickReadData;
   static struct timeval BwReadTick;
   static const unsigned int BW_HZ;

   unsigned long LeftRead()
   {
      unsigned long Sz = Size - (InP - OutP);
      if (Sz > Size - (InP%Size))
	 Sz = Size - (InP%Size);
      return Sz;
   }
   unsigned long LeftWrite()
   {
      unsigned long Sz = InP - OutP;
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
   bool Read(string Data);
   
   // Write data out
   bool Write(int Fd);
   bool WriteTillEl(string &Data,bool Single = false);
   
   // Control the write limit
   void Limit(long Max) {if (Max == -1) MaxGet = 0-1; else MaxGet = OutP + Max;}   
   bool IsLimit() {return MaxGet == OutP;};
   void Print() {cout << MaxGet << ',' << OutP << endl;};

   // Test for free space in the buffer
   bool ReadSpace() {return Size - (InP - OutP) > 0;};
   bool WriteSpace() {return InP - OutP > 0;};

   // Dump everything
   void Reset();
   void Stats();

   CircleBuf(unsigned long Size);
   ~CircleBuf() {delete [] Buf; delete Hash;};
};

struct ServerState
{
   // This is the last parsed Header Line
   unsigned int Major;
   unsigned int Minor;
   unsigned int Result;
   char Code[MAXLEN];
   
   // These are some statistics from the last parsed header lines
   unsigned long Size;
   signed long StartPos;
   time_t Date;
   bool HaveContent;
   enum {Chunked,Stream,Closes} Encoding;
   enum {Header, Data} State;
   bool Persistent;
   
   // This is a Persistent attribute of the server itself.
   bool Pipeline;
   
   HttpMethod *Owner;
   
   // This is the connection itself. Output is data FROM the server
   CircleBuf In;
   CircleBuf Out;
   int ServerFd;
   URI ServerName;
  
   bool HeaderLine(string Line);
   bool Comp(URI Other) {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   void Reset() {Major = 0; Minor = 0; Result = 0; Size = 0; StartPos = 0;
                 Encoding = Closes; time(&Date); ServerFd = -1; 
                 Pipeline = true;};
   int RunHeaders();
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
   int DealWithHeaders(FetchResult &Res,ServerState *Srv);

   virtual bool Fetch(FetchItem *);
   virtual bool Configuration(string Message);
   
   // In the event of a fatal signal this file will be closed and timestamped.
   static string FailFile;
   static int FailFd;
   static time_t FailTime;
   static void SigTerm(int);
   
   public:
   friend class ServerState;

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
