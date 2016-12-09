// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: http.cc,v 1.59 2004/05/08 19:42:35 mdz Exp $
/* ######################################################################

   HTTP Acquire Method - This is the HTTP acquire method for APT.
   
   It uses HTTP/1.1 and many of the fancy options there-in, such as
   pipelining, range, if-range and so on. 

   It is based on a doubly buffered select loop. A groupe of requests are 
   fed into a single output buffer that is constantly fed out the 
   socket. This provides ideal pipelining as in many cases all of the
   requests will fit into a single packet. The input socket is buffered 
   the same way and fed into the fd for the file (may be a pipe in future).
   
   This double buffering provides fairly substantial transfer rates,
   compared to wget the http method is about 4% faster. Most importantly,
   when HTTP is compared with FTP as a protocol the speed difference is
   huge. In tests over the internet from two sites to llug (via ATM) this
   program got 230k/s sustained http transfer rates. FTP on the other 
   hand topped out at 170k/s. That combined with the time to setup the
   FTP connection makes HTTP a vastly superior protocol.
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/netrc.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/proxy.h>

#include <stddef.h>
#include <stdlib.h>
#include <sys/select.h>
#include <cstring>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>

#include "config.h"
#include "connect.h"
#include "http.h"

#include <apti18n.h>
									/*}}}*/
using namespace std;

unsigned long long CircleBuf::BwReadLimit=0;
unsigned long long CircleBuf::BwTickReadData=0;
struct timeval CircleBuf::BwReadTick={0,0};
const unsigned int CircleBuf::BW_HZ=10;

// CircleBuf::CircleBuf - Circular input buffer				/*{{{*/
// ---------------------------------------------------------------------
/* */
CircleBuf::CircleBuf(HttpMethod const * const Owner, unsigned long long Size)
   : Size(Size), Hash(NULL), TotalWriten(0)
{
   Buf = new unsigned char[Size];
   Reset();

   CircleBuf::BwReadLimit = Owner->ConfigFindI("Dl-Limit", 0) * 1024;
}
									/*}}}*/
// CircleBuf::Reset - Reset to the default state			/*{{{*/
// ---------------------------------------------------------------------
/* */
void CircleBuf::Reset()
{
   InP = 0;
   OutP = 0;
   StrPos = 0;
   TotalWriten = 0;
   MaxGet = (unsigned long long)-1;
   OutQueue = string();
   if (Hash != NULL)
   {
      delete Hash;
      Hash = NULL;
   }
}
									/*}}}*/
// CircleBuf::Read - Read from a FD into the circular buffer		/*{{{*/
// ---------------------------------------------------------------------
/* This fills up the buffer with as much data as is in the FD, assuming it
   is non-blocking.. */
bool CircleBuf::Read(int Fd)
{
   while (1)
   {
      // Woops, buffer is full
      if (InP - OutP == Size)
	 return true;

      // what's left to read in this tick
      unsigned long long const BwReadMax = CircleBuf::BwReadLimit/BW_HZ;

      if(CircleBuf::BwReadLimit) {
	 struct timeval now;
	 gettimeofday(&now,0);

	 unsigned long long d = (now.tv_sec-CircleBuf::BwReadTick.tv_sec)*1000000 +
	    now.tv_usec-CircleBuf::BwReadTick.tv_usec;
	 if(d > 1000000/BW_HZ) {
	    CircleBuf::BwReadTick = now;
	    CircleBuf::BwTickReadData = 0;
	 } 
	 
	 if(CircleBuf::BwTickReadData >= BwReadMax) {
	    usleep(1000000/BW_HZ);
	    return true;
	 }
      }

      // Write the buffer segment
      ssize_t Res;
      if(CircleBuf::BwReadLimit) {
	 Res = read(Fd,Buf + (InP%Size), 
		    BwReadMax > LeftRead() ? LeftRead() : BwReadMax);
      } else
	 Res = read(Fd,Buf + (InP%Size),LeftRead());
      
      if(Res > 0 && BwReadLimit > 0) 
	 CircleBuf::BwTickReadData += Res;
    
      if (Res == 0)
	 return false;
      if (Res < 0)
      {
	 if (errno == EAGAIN)
	    return true;
	 return false;
      }

      if (InP == 0)
	 gettimeofday(&Start,0);
      InP += Res;
   }
}
									/*}}}*/
// CircleBuf::Read - Put the string into the buffer			/*{{{*/
// ---------------------------------------------------------------------
/* This will hold the string in and fill the buffer with it as it empties */
bool CircleBuf::Read(string const &Data)
{
   OutQueue.append(Data);
   FillOut();
   return true;
}
									/*}}}*/
// CircleBuf::FillOut - Fill the buffer from the output queue		/*{{{*/
// ---------------------------------------------------------------------
/* */
void CircleBuf::FillOut()
{
   if (OutQueue.empty() == true)
      return;
   while (1)
   {
      // Woops, buffer is full
      if (InP - OutP == Size)
	 return;
      
      // Write the buffer segment
      unsigned long long Sz = LeftRead();
      if (OutQueue.length() - StrPos < Sz)
	 Sz = OutQueue.length() - StrPos;
      memcpy(Buf + (InP%Size),OutQueue.c_str() + StrPos,Sz);
      
      // Advance
      StrPos += Sz;
      InP += Sz;
      if (OutQueue.length() == StrPos)
      {
	 StrPos = 0;
	 OutQueue = "";
	 return;
      }
   }
}
									/*}}}*/
// CircleBuf::Write - Write from the buffer into a FD			/*{{{*/
// ---------------------------------------------------------------------
/* This empties the buffer into the FD. */
bool CircleBuf::Write(int Fd)
{
   while (1)
   {
      FillOut();
      
      // Woops, buffer is empty
      if (OutP == InP)
	 return true;
      
      if (OutP == MaxGet)
	 return true;
      
      // Write the buffer segment
      ssize_t Res;
      Res = write(Fd,Buf + (OutP%Size),LeftWrite());

      if (Res == 0)
	 return false;
      if (Res < 0)
      {
	 if (errno == EAGAIN)
	    return true;
	 
	 return false;
      }

      TotalWriten += Res;
      
      if (Hash != NULL)
	 Hash->Add(Buf + (OutP%Size),Res);
      
      OutP += Res;
   }
}
									/*}}}*/
// CircleBuf::WriteTillEl - Write from the buffer to a string		/*{{{*/
// ---------------------------------------------------------------------
/* This copies till the first empty line */
bool CircleBuf::WriteTillEl(string &Data,bool Single)
{
   // We cheat and assume it is unneeded to have more than one buffer load
   for (unsigned long long I = OutP; I < InP; I++)
   {      
      if (Buf[I%Size] != '\n')
	 continue;
      ++I;
      
      if (Single == false)
      {
         if (I < InP  && Buf[I%Size] == '\r')
            ++I;
         if (I >= InP || Buf[I%Size] != '\n')
            continue;
         ++I;
      }
      
      Data = "";
      while (OutP < I)
      {
	 unsigned long long Sz = LeftWrite();
	 if (Sz == 0)
	    return false;
	 if (I - OutP < Sz)
	    Sz = I - OutP;
	 Data += string((char *)(Buf + (OutP%Size)),Sz);
	 OutP += Sz;
      }
      return true;
   }      
   return false;
}
									/*}}}*/
// CircleBuf::Stats - Print out stats information			/*{{{*/
// ---------------------------------------------------------------------
/* */
void CircleBuf::Stats()
{
   if (InP == 0)
      return;
   
   struct timeval Stop;
   gettimeofday(&Stop,0);
/*   float Diff = Stop.tv_sec - Start.tv_sec + 
             (float)(Stop.tv_usec - Start.tv_usec)/1000000;
   clog << "Got " << InP << " in " << Diff << " at " << InP/Diff << endl;*/
}
									/*}}}*/
CircleBuf::~CircleBuf()
{
   delete [] Buf;
   delete Hash;
}

// HttpServerState::HttpServerState - Constructor			/*{{{*/
HttpServerState::HttpServerState(URI Srv,HttpMethod *Owner) : ServerState(Srv, Owner), In(Owner, 64*1024), Out(Owner, 4*1024)
{
   TimeOut = Owner->ConfigFindI("Timeout", TimeOut);
   Reset();
}
									/*}}}*/
// HttpServerState::Open - Open a connection to the server		/*{{{*/
// ---------------------------------------------------------------------
/* This opens a connection to the server. */
static bool TalkToSocksProxy(int const ServerFd, std::string const &Proxy,
      char const * const type, bool const ReadWrite, uint8_t * const ToFrom,
      unsigned int const Size, unsigned int const Timeout)
{
   if (WaitFd(ServerFd, ReadWrite, Timeout) == false)
      return _error->Error("Waiting for the SOCKS proxy %s to %s timed out", URI::SiteOnly(Proxy).c_str(), type);
   if (ReadWrite == false)
   {
      if (FileFd::Read(ServerFd, ToFrom, Size) == false)
	 return _error->Error("Reading the %s from SOCKS proxy %s failed", type, URI::SiteOnly(Proxy).c_str());
   }
   else
   {
      if (FileFd::Write(ServerFd, ToFrom, Size) == false)
	 return _error->Error("Writing the %s to SOCKS proxy %s failed", type, URI::SiteOnly(Proxy).c_str());
   }
   return true;
}
bool HttpServerState::Open()
{
   // Use the already open connection if possible.
   if (ServerFd != -1)
      return true;
   
   Close();
   In.Reset();
   Out.Reset();
   Persistent = true;
   
   // Determine the proxy setting
   AutoDetectProxy(ServerName);
   string SpecificProxy = Owner->ConfigFind("Proxy::" + ServerName.Host, "");
   if (!SpecificProxy.empty())
   {
	   if (SpecificProxy == "DIRECT")
		   Proxy = "";
	   else
		   Proxy = SpecificProxy;
   }
   else
   {
	   string DefProxy = Owner->ConfigFind("Proxy", "");
	   if (!DefProxy.empty())
	   {
		   Proxy = DefProxy;
	   }
	   else
	   {
		   char* result = getenv("http_proxy");
		   Proxy = result ? result : "";
	   }
   }
   
   // Parse no_proxy, a , separated list of domains
   if (getenv("no_proxy") != 0)
   {
      if (CheckDomainList(ServerName.Host,getenv("no_proxy")) == true)
	 Proxy = "";
   }

   if (Proxy.empty() == false)
      Owner->AddProxyAuth(Proxy, ServerName);

   if (Proxy.Access == "socks5h")
   {
      if (Connect(Proxy.Host, Proxy.Port, "socks", 1080, ServerFd, TimeOut, Owner) == false)
	 return false;

      /* We implement a very basic SOCKS5 client here complying mostly to RFC1928 expect
       * for not offering GSSAPI auth which is a must (we only do no or user/pass auth).
       * We also expect the SOCKS5 server to do hostname lookup (aka socks5h) */
      std::string const ProxyInfo = URI::SiteOnly(Proxy);
      Owner->Status(_("Connecting to %s (%s)"),"SOCKS5h proxy",ProxyInfo.c_str());
      auto const Timeout = Owner->ConfigFindI("TimeOut", 120);
      #define APT_WriteOrFail(TYPE, DATA, LENGTH) if (TalkToSocksProxy(ServerFd, ProxyInfo, TYPE, true, DATA, LENGTH, Timeout) == false) return false
      #define APT_ReadOrFail(TYPE, DATA, LENGTH) if (TalkToSocksProxy(ServerFd, ProxyInfo, TYPE, false, DATA, LENGTH, Timeout) == false) return false
      if (ServerName.Host.length() > 255)
	 return _error->Error("Can't use SOCKS5h as hostname %s is too long!", ServerName.Host.c_str());
      if (Proxy.User.length() > 255 || Proxy.Password.length() > 255)
	 return _error->Error("Can't use user&pass auth as they are too long (%lu and %lu) for the SOCKS5!", Proxy.User.length(), Proxy.Password.length());
      if (Proxy.User.empty())
      {
	 uint8_t greeting[] = { 0x05, 0x01, 0x00 };
	 APT_WriteOrFail("greet-1", greeting, sizeof(greeting));
      }
      else
      {
	 uint8_t greeting[] = { 0x05, 0x02, 0x00, 0x02 };
	 APT_WriteOrFail("greet-2", greeting, sizeof(greeting));
      }
      uint8_t greeting[2];
      APT_ReadOrFail("greet back", greeting, sizeof(greeting));
      if (greeting[0] != 0x05)
	 return _error->Error("SOCKS proxy %s greets back with wrong version: %d", ProxyInfo.c_str(), greeting[0]);
      if (greeting[1] == 0x00)
	 ; // no auth has no method-dependent sub-negotiations
      else if (greeting[1] == 0x02)
      {
	 if (Proxy.User.empty())
	    return _error->Error("SOCKS proxy %s negotiated user&pass auth, but we had not offered it!", ProxyInfo.c_str());
	 // user&pass auth sub-negotiations are defined by RFC1929
	 std::vector<uint8_t> auth = {{ 0x01, static_cast<uint8_t>(Proxy.User.length()) }};
	 std::copy(Proxy.User.begin(), Proxy.User.end(), std::back_inserter(auth));
	 auth.push_back(static_cast<uint8_t>(Proxy.Password.length()));
	 std::copy(Proxy.Password.begin(), Proxy.Password.end(), std::back_inserter(auth));
	 APT_WriteOrFail("user&pass auth", auth.data(), auth.size());
	 uint8_t authstatus[2];
	 APT_ReadOrFail("auth report", authstatus, sizeof(authstatus));
	 if (authstatus[0] != 0x01)
	    return _error->Error("SOCKS proxy %s auth status response with wrong version: %d", ProxyInfo.c_str(), authstatus[0]);
	 if (authstatus[1] != 0x00)
	    return _error->Error("SOCKS proxy %s reported authorization failure: username or password incorrect? (%d)", ProxyInfo.c_str(), authstatus[1]);
      }
      else
	 return _error->Error("SOCKS proxy %s greets back having not found a common authorization method: %d", ProxyInfo.c_str(), greeting[1]);
      union { uint16_t * i; uint8_t * b; } portu;
      uint16_t port = htons(static_cast<uint16_t>(ServerName.Port == 0 ? 80 : ServerName.Port));
      portu.i = &port;
      std::vector<uint8_t> request = {{ 0x05, 0x01, 0x00, 0x03, static_cast<uint8_t>(ServerName.Host.length()) }};
      std::copy(ServerName.Host.begin(), ServerName.Host.end(), std::back_inserter(request));
      request.push_back(portu.b[0]);
      request.push_back(portu.b[1]);
      APT_WriteOrFail("request", request.data(), request.size());
      uint8_t response[4];
      APT_ReadOrFail("first part of response", response, sizeof(response));
      if (response[0] != 0x05)
	 return _error->Error("SOCKS proxy %s response with wrong version: %d", ProxyInfo.c_str(), response[0]);
      if (response[2] != 0x00)
	 return _error->Error("SOCKS proxy %s has unexpected non-zero reserved field value: %d", ProxyInfo.c_str(), response[2]);
      std::string bindaddr;
      if (response[3] == 0x01) // IPv4 address
      {
	 uint8_t ip4port[6];
	 APT_ReadOrFail("IPv4+Port of response", ip4port, sizeof(ip4port));
	 portu.b[0] = ip4port[4];
	 portu.b[1] = ip4port[5];
	 port = ntohs(*portu.i);
	 strprintf(bindaddr, "%d.%d.%d.%d:%d", ip4port[0], ip4port[1], ip4port[2], ip4port[3], port);
      }
      else if (response[3] == 0x03) // hostname
      {
	 uint8_t namelength;
	 APT_ReadOrFail("hostname length of response", &namelength, 1);
	 uint8_t hostname[namelength + 2];
	 APT_ReadOrFail("hostname of response", hostname, sizeof(hostname));
	 portu.b[0] = hostname[namelength];
	 portu.b[1] = hostname[namelength + 1];
	 port = ntohs(*portu.i);
	 hostname[namelength] = '\0';
	 strprintf(bindaddr, "%s:%d", hostname, port);
      }
      else if (response[3] == 0x04) // IPv6 address
      {
	 uint8_t ip6port[18];
	 APT_ReadOrFail("IPv6+port of response", ip6port, sizeof(ip6port));
	 portu.b[0] = ip6port[16];
	 portu.b[1] = ip6port[17];
	 port = ntohs(*portu.i);
	 strprintf(bindaddr, "[%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X]:%d",
	       ip6port[0], ip6port[1], ip6port[2], ip6port[3], ip6port[4], ip6port[5], ip6port[6], ip6port[7],
	       ip6port[8], ip6port[9], ip6port[10], ip6port[11], ip6port[12], ip6port[13], ip6port[14], ip6port[15],
	       port);
      }
      else
	 return _error->Error("SOCKS proxy %s destination address is of unknown type: %d",
	       ProxyInfo.c_str(), response[3]);
      if (response[1] != 0x00)
      {
	 char const * errstr = nullptr;
	 auto errcode = response[1];
	 // Tor error reporting can be a bit arcane, lets try to detect & fix it up
	 if (bindaddr == "0.0.0.0:0")
	 {
	    auto const lastdot = ServerName.Host.rfind('.');
	    if (lastdot == std::string::npos || ServerName.Host.substr(lastdot) != ".onion")
	       ;
	    else if (errcode == 0x01)
	    {
	       auto const prevdot = ServerName.Host.rfind('.', lastdot - 1);
	       if (lastdot == 16 && prevdot == std::string::npos)
		  ; // valid .onion address
	       else if (prevdot != std::string::npos && (lastdot - prevdot) == 17)
		  ; // valid .onion address with subdomain(s)
	       else
	       {
		  errstr = "Invalid hostname: onion service name must be 16 characters long";
		  Owner->SetFailReason("SOCKS");
	       }
	    }
	    // in all likelihood the service is either down or the address has
	    // a typo and so "Host unreachable" is the better understood error
	    // compared to the technically correct "TLL expired".
	    else if (errcode == 0x06)
	       errcode = 0x04;
	 }
	 if (errstr == nullptr)
	 {
	    switch (errcode)
	    {
	       case 0x01: errstr = "general SOCKS server failure"; Owner->SetFailReason("SOCKS"); break;
	       case 0x02: errstr = "connection not allowed by ruleset"; Owner->SetFailReason("SOCKS"); break;
	       case 0x03: errstr = "Network unreachable"; Owner->SetFailReason("ConnectionTimedOut"); break;
	       case 0x04: errstr = "Host unreachable"; Owner->SetFailReason("ConnectionTimedOut"); break;
	       case 0x05: errstr = "Connection refused"; Owner->SetFailReason("ConnectionRefused"); break;
	       case 0x06: errstr = "TTL expired"; Owner->SetFailReason("Timeout"); break;
	       case 0x07: errstr = "Command not supported"; Owner->SetFailReason("SOCKS"); break;
	       case 0x08: errstr = "Address type not supported"; Owner->SetFailReason("SOCKS"); break;
	       default: errstr = "Unknown error"; Owner->SetFailReason("SOCKS"); break;
	    }
	 }
	 return _error->Error("SOCKS proxy %s could not connect to %s (%s) due to: %s (%d)",
	       ProxyInfo.c_str(), ServerName.Host.c_str(), bindaddr.c_str(), errstr, response[1]);
      }
      else if (Owner->DebugEnabled())
	 ioprintf(std::clog, "http: SOCKS proxy %s connection established to %s (%s)\n",
	       ProxyInfo.c_str(), ServerName.Host.c_str(), bindaddr.c_str());

      if (WaitFd(ServerFd, true, Timeout) == false)
	 return _error->Error("SOCKS proxy %s reported connection to %s (%s), but timed out",
	       ProxyInfo.c_str(), ServerName.Host.c_str(), bindaddr.c_str());
      #undef APT_ReadOrFail
      #undef APT_WriteOrFail
   }
   else
   {
      // Determine what host and port to use based on the proxy settings
      int Port = 0;
      string Host;
      if (Proxy.empty() == true || Proxy.Host.empty() == true)
      {
	 if (ServerName.Port != 0)
	    Port = ServerName.Port;
	 Host = ServerName.Host;
      }
      else if (Proxy.Access != "http")
	 return _error->Error("Unsupported proxy configured: %s", URI::SiteOnly(Proxy).c_str());
      else
      {
	 if (Proxy.Port != 0)
	    Port = Proxy.Port;
	 Host = Proxy.Host;
      }
      return Connect(Host,Port,"http",80,ServerFd,TimeOut,Owner);
   }
   return true;
}
									/*}}}*/
// HttpServerState::Close - Close a connection to the server		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool HttpServerState::Close()
{
   close(ServerFd);
   ServerFd = -1;
   return true;
}
									/*}}}*/
// HttpServerState::RunData - Transfer the data from the socket		/*{{{*/
bool HttpServerState::RunData(RequestState &Req)
{
   Req.State = RequestState::Data;
   
   // Chunked transfer encoding is fun..
   if (Req.Encoding == RequestState::Chunked)
   {
      while (1)
      {
	 // Grab the block size
	 bool Last = true;
	 string Data;
	 In.Limit(-1);
	 do
	 {
	    if (In.WriteTillEl(Data,true) == true)
	       break;
	 }
	 while ((Last = Go(false, Req)) == true);

	 if (Last == false)
	    return false;
	 	 
	 // See if we are done
	 unsigned long long Len = strtoull(Data.c_str(),0,16);
	 if (Len == 0)
	 {
	    In.Limit(-1);
	    
	    // We have to remove the entity trailer
	    Last = true;
	    do
	    {
	       if (In.WriteTillEl(Data,true) == true && Data.length() <= 2)
		  break;
	    }
	    while ((Last = Go(false, Req)) == true);
	    if (Last == false)
	       return false;
	    return !_error->PendingError();
	 }
	 
	 // Transfer the block
	 In.Limit(Len);
	 while (Go(true, Req) == true)
	    if (In.IsLimit() == true)
	       break;
	 
	 // Error
	 if (In.IsLimit() == false)
	    return false;
	 
	 // The server sends an extra new line before the next block specifier..
	 In.Limit(-1);
	 Last = true;
	 do
	 {
	    if (In.WriteTillEl(Data,true) == true)
	       break;
	 }
	 while ((Last = Go(false, Req)) == true);
	 if (Last == false)
	    return false;
      }
   }
   else
   {
      /* Closes encoding is used when the server did not specify a size, the
         loss of the connection means we are done */
      if (Req.JunkSize != 0)
	 In.Limit(Req.JunkSize);
      else if (Req.DownloadSize != 0)
	 In.Limit(Req.DownloadSize);
      else if (Persistent == false)
	 In.Limit(-1);
      
      // Just transfer the whole block.
      do
      {
	 if (In.IsLimit() == false)
	    continue;
	 
	 In.Limit(-1);
	 return !_error->PendingError();
      }
      while (Go(true, Req) == true);
   }

   return Flush(&Req.File) && !_error->PendingError();
}
									/*}}}*/
bool HttpServerState::RunDataToDevNull(RequestState &Req)		/*{{{*/
{
   // no need to clean up if we discard the connection anyhow
   if (Persistent == false)
      return true;
   Req.File.Open("/dev/null", FileFd::WriteOnly);
   return RunData(Req);
}
									/*}}}*/
bool HttpServerState::ReadHeaderLines(std::string &Data)		/*{{{*/
{
   return In.WriteTillEl(Data);
}
									/*}}}*/
bool HttpServerState::LoadNextResponse(bool const ToFile, RequestState &Req)/*{{{*/
{
   return Go(ToFile, Req);
}
									/*}}}*/
bool HttpServerState::WriteResponse(const std::string &Data)		/*{{{*/
{
   return Out.Read(Data);
}
									/*}}}*/
APT_PURE bool HttpServerState::IsOpen()					/*{{{*/
{
   return (ServerFd != -1);
}
									/*}}}*/
bool HttpServerState::InitHashes(HashStringList const &ExpectedHashes)	/*{{{*/
{
   delete In.Hash;
   In.Hash = new Hashes(ExpectedHashes);
   return true;
}
									/*}}}*/
void HttpServerState::Reset()						/*{{{*/
{
   ServerState::Reset();
   ServerFd = -1;
}
									/*}}}*/

APT_PURE Hashes * HttpServerState::GetHashes()				/*{{{*/
{
   return In.Hash;
}
									/*}}}*/
// HttpServerState::Die - The server has closed the connection.		/*{{{*/
bool HttpServerState::Die(RequestState &Req)
{
   unsigned int LErrno = errno;

   // Dump the buffer to the file
   if (Req.State == RequestState::Data)
   {
      if (Req.File.IsOpen() == false)
	 return true;
      // on GNU/kFreeBSD, apt dies on /dev/null because non-blocking
      // can't be set
      if (Req.File.Name() != "/dev/null")
	 SetNonBlock(Req.File.Fd(),false);
      while (In.WriteSpace() == true)
      {
	 if (In.Write(Req.File.Fd()) == false)
	    return _error->Errno("write",_("Error writing to the file"));

	 // Done
	 if (In.IsLimit() == true)
	    return true;
      }
   }

   // See if this is because the server finished the data stream
   if (In.IsLimit() == false && Req.State != RequestState::Header &&
       Persistent == true)
   {
      Close();
      if (LErrno == 0)
	 return _error->Error(_("Error reading from server. Remote end closed connection"));
      errno = LErrno;
      return _error->Errno("read",_("Error reading from server"));
   }
   else
   {
      In.Limit(-1);

      // Nothing left in the buffer
      if (In.WriteSpace() == false)
	 return false;

      // We may have got multiple responses back in one packet..
      Close();
      return true;
   }

   return false;
}
									/*}}}*/
// HttpServerState::Flush - Dump the buffer into the file		/*{{{*/
// ---------------------------------------------------------------------
/* This takes the current input buffer from the Server FD and writes it
   into the file */
bool HttpServerState::Flush(FileFd * const File)
{
   if (File != nullptr)
   {
      // on GNU/kFreeBSD, apt dies on /dev/null because non-blocking
      // can't be set
      if (File->Name() != "/dev/null")
	 SetNonBlock(File->Fd(),false);
      if (In.WriteSpace() == false)
	 return true;
      
      while (In.WriteSpace() == true)
      {
	 if (In.Write(File->Fd()) == false)
	    return _error->Errno("write",_("Error writing to file"));
	 if (In.IsLimit() == true)
	    return true;
      }

      if (In.IsLimit() == true || Persistent == false)
	 return true;
   }
   return false;
}
									/*}}}*/
// HttpServerState::Go - Run a single loop				/*{{{*/
// ---------------------------------------------------------------------
/* This runs the select loop over the server FDs, Output file FDs and
   stdin. */
bool HttpServerState::Go(bool ToFile, RequestState &Req)
{
   // Server has closed the connection
   if (ServerFd == -1 && (In.WriteSpace() == false || 
			       ToFile == false))
      return false;
   
   fd_set rfds,wfds;
   FD_ZERO(&rfds);
   FD_ZERO(&wfds);
   
   /* Add the server. We only send more requests if the connection will 
      be persisting */
   if (Out.WriteSpace() == true && ServerFd != -1 
       && Persistent == true)
      FD_SET(ServerFd,&wfds);
   if (In.ReadSpace() == true && ServerFd != -1)
      FD_SET(ServerFd,&rfds);
   
   // Add the file
   int FileFD = -1;
   if (Req.File.IsOpen())
      FileFD = Req.File.Fd();
   
   if (In.WriteSpace() == true && ToFile == true && FileFD != -1)
      FD_SET(FileFD,&wfds);

   // Add stdin
   if (Owner->ConfigFindB("DependOnSTDIN", true) == true)
      FD_SET(STDIN_FILENO,&rfds);
	  
   // Figure out the max fd
   int MaxFd = FileFD;
   if (MaxFd < ServerFd)
      MaxFd = ServerFd;

   // Select
   struct timeval tv;
   tv.tv_sec = TimeOut;
   tv.tv_usec = 0;
   int Res = 0;
   if ((Res = select(MaxFd+1,&rfds,&wfds,0,&tv)) < 0)
   {
      if (errno == EINTR)
	 return true;
      return _error->Errno("select",_("Select failed"));
   }
   
   if (Res == 0)
   {
      _error->Error(_("Connection timed out"));
      return Die(Req);
   }
   
   // Handle server IO
   if (ServerFd != -1 && FD_ISSET(ServerFd,&rfds))
   {
      errno = 0;
      if (In.Read(ServerFd) == false)
	 return Die(Req);
   }
	 
   if (ServerFd != -1 && FD_ISSET(ServerFd,&wfds))
   {
      errno = 0;
      if (Out.Write(ServerFd) == false)
	 return Die(Req);
   }

   // Send data to the file
   if (FileFD != -1 && FD_ISSET(FileFD,&wfds))
   {
      if (In.Write(FileFD) == false)
	 return _error->Errno("write",_("Error writing to output file"));
   }

   if (Req.MaximumSize > 0 && Req.File.IsOpen() && Req.File.Failed() == false && Req.File.Tell() > Req.MaximumSize)
   {
      Owner->SetFailReason("MaximumSizeExceeded");
      return _error->Error("Writing more data than expected (%llu > %llu)",
                           Req.File.Tell(), Req.MaximumSize);
   }

   // Handle commands from APT
   if (FD_ISSET(STDIN_FILENO,&rfds))
   {
      if (Owner->Run(true) != -1)
	 exit(100);
   }   
       
   return true;
}
									/*}}}*/

// HttpMethod::SendReq - Send the HTTP request				/*{{{*/
// ---------------------------------------------------------------------
/* This places the http request in the outbound buffer */
void HttpMethod::SendReq(FetchItem *Itm)
{
   URI Uri = Itm->Uri;
   {
      auto const plus = Binary.find('+');
      if (plus != std::string::npos)
	 Uri.Access = Binary.substr(plus + 1);
   }

   // The HTTP server expects a hostname with a trailing :port
   std::stringstream Req;
   string ProperHost;

   if (Uri.Host.find(':') != string::npos)
      ProperHost = '[' + Uri.Host + ']';
   else
      ProperHost = Uri.Host;

   /* RFC 2616 §5.1.2 requires absolute URIs for requests to proxies,
      but while its a must for all servers to accept absolute URIs,
      it is assumed clients will sent an absolute path for non-proxies */
   std::string requesturi;
   if (Server->Proxy.Access != "http" || Server->Proxy.empty() == true || Server->Proxy.Host.empty())
      requesturi = Uri.Path;
   else
      requesturi = Uri;

   // The "+" is encoded as a workaround for a amazon S3 bug
   // see LP bugs #1003633 and #1086997.
   requesturi = QuoteString(requesturi, "+~ ");

   /* Build the request. No keep-alive is included as it is the default
      in 1.1, can cause problems with proxies, and we are an HTTP/1.1
      client anyway.
      C.f. https://tools.ietf.org/wg/httpbis/trac/ticket/158 */
   Req << "GET " << requesturi << " HTTP/1.1\r\n";
   if (Uri.Port != 0)
      Req << "Host: " << ProperHost << ":" << std::to_string(Uri.Port) << "\r\n";
   else
      Req << "Host: " << ProperHost << "\r\n";

   // generate a cache control header (if needed)
   if (ConfigFindB("No-Cache",false) == true)
      Req << "Cache-Control: no-cache\r\n"
	 << "Pragma: no-cache\r\n";
   else if (Itm->IndexFile == true)
      Req << "Cache-Control: max-age=" << std::to_string(ConfigFindI("Max-Age", 0)) << "\r\n";
   else if (ConfigFindB("No-Store", false) == true)
      Req << "Cache-Control: no-store\r\n";

   // If we ask for uncompressed files servers might respond with content-
   // negotiation which lets us end up with compressed files we do not support,
   // see 657029, 657560 and co, so if we have no extension on the request
   // ask for text only. As a sidenote: If there is nothing to negotate servers
   // seem to be nice and ignore it.
   if (ConfigFindB("SendAccept", true) == true)
   {
      size_t const filepos = Itm->Uri.find_last_of('/');
      string const file = Itm->Uri.substr(filepos + 1);
      if (flExtension(file) == file)
	 Req << "Accept: text/*\r\n";
   }

   // Check for a partial file and send if-queries accordingly
   struct stat SBuf;
   if (Server->RangesAllowed && stat(Itm->DestFile.c_str(),&SBuf) >= 0 && SBuf.st_size > 0)
      Req << "Range: bytes=" << std::to_string(SBuf.st_size) << "-\r\n"
	 << "If-Range: " << TimeRFC1123(SBuf.st_mtime, false) << "\r\n";
   else if (Itm->LastModified != 0)
      Req << "If-Modified-Since: " << TimeRFC1123(Itm->LastModified, false).c_str() << "\r\n";

   if (Server->Proxy.Access == "http" &&
	 (Server->Proxy.User.empty() == false || Server->Proxy.Password.empty() == false))
      Req << "Proxy-Authorization: Basic "
	 << Base64Encode(Server->Proxy.User + ":" + Server->Proxy.Password) << "\r\n";

   maybe_add_auth (Uri, _config->FindFile("Dir::Etc::netrc"));
   if (Uri.User.empty() == false || Uri.Password.empty() == false)
      Req << "Authorization: Basic "
	 << Base64Encode(Uri.User + ":" + Uri.Password) << "\r\n";

   Req << "User-Agent: " << ConfigFind("User-Agent",
		"Debian APT-HTTP/1.3 (" PACKAGE_VERSION ")") << "\r\n";

   Req << "\r\n";

   if (Debug == true)
      cerr << Req.str() << endl;

   Server->WriteResponse(Req.str());
}
									/*}}}*/
std::unique_ptr<ServerState> HttpMethod::CreateServerState(URI const &uri)/*{{{*/
{
   return std::unique_ptr<ServerState>(new HttpServerState(uri, this));
}
									/*}}}*/
void HttpMethod::RotateDNS()						/*{{{*/
{
   ::RotateDNS();
}
									/*}}}*/
BaseHttpMethod::DealWithHeadersResult HttpMethod::DealWithHeaders(FetchResult &Res, RequestState &Req)/*{{{*/
{
   auto ret = BaseHttpMethod::DealWithHeaders(Res, Req);
   if (ret != BaseHttpMethod::FILE_IS_OPEN)
      return ret;
   if (Req.File.Open(Queue->DestFile, FileFd::WriteAny) == false)
      return ERROR_NOT_FROM_SERVER;

   FailFile = Queue->DestFile;
   FailFile.c_str();   // Make sure we don't do a malloc in the signal handler
   FailFd = Req.File.Fd();
   FailTime = Req.Date;

   if (Server->InitHashes(Queue->ExpectedHashes) == false || Req.AddPartialFileToHashes(Req.File) == false)
   {
      _error->Errno("read",_("Problem hashing file"));
      return ERROR_NOT_FROM_SERVER;
   }
   if (Req.StartPos > 0)
      Res.ResumePoint = Req.StartPos;

   SetNonBlock(Req.File.Fd(),true);
   return FILE_IS_OPEN;
}
									/*}}}*/
HttpMethod::HttpMethod(std::string &&pProg) : BaseHttpMethod(pProg.c_str(), "1.2", Pipeline | SendConfig)/*{{{*/
{
   auto addName = std::inserter(methodNames, methodNames.begin());
   if (Binary != "http")
      addName = "http";
   auto const plus = Binary.find('+');
   if (plus != std::string::npos)
      addName = Binary.substr(0, plus);
}
									/*}}}*/
