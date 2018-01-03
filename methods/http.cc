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
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/proxy.h>
#include <apt-pkg/strutl.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

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
bool CircleBuf::Read(std::unique_ptr<MethodFd> const &Fd)
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
	 Res = Fd->Read(Buf + (InP % Size),
			BwReadMax > LeftRead() ? LeftRead() : BwReadMax);
      } else
	 Res = Fd->Read(Buf + (InP % Size), LeftRead());

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
bool CircleBuf::Write(std::unique_ptr<MethodFd> const &Fd)
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
      Res = Fd->Write(Buf + (OutP % Size), LeftWrite());

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
// CircleBuf::Write - Write from the buffer to a string			/*{{{*/
// ---------------------------------------------------------------------
/* This copies everything */
bool CircleBuf::Write(string &Data)
{
   Data = std::string((char *)Buf + (OutP % Size), LeftWrite());
   OutP += LeftWrite();
   return true;
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
CircleBuf::~CircleBuf()							/*{{{*/
{
   delete [] Buf;
   delete Hash;
}
									/*}}}*/

// UnwrapHTTPConnect - Does the HTTP CONNECT handshake			/*{{{*/
// ---------------------------------------------------------------------
/* Performs a TLS handshake on the socket */
struct HttpConnectFd : public MethodFd
{
   std::unique_ptr<MethodFd> UnderlyingFd;
   std::string Buffer;

   int Fd() APT_OVERRIDE { return UnderlyingFd->Fd(); }

   ssize_t Read(void *buf, size_t count) APT_OVERRIDE
   {
      if (!Buffer.empty())
      {
	 auto read = count < Buffer.size() ? count : Buffer.size();

	 memcpy(buf, Buffer.data(), read);
	 Buffer.erase(Buffer.begin(), Buffer.begin() + read);
	 return read;
      }

      return UnderlyingFd->Read(buf, count);
   }
   ssize_t Write(void *buf, size_t count) APT_OVERRIDE
   {
      return UnderlyingFd->Write(buf, count);
   }

   int Close() APT_OVERRIDE
   {
      return UnderlyingFd->Close();
   }

   bool HasPending() APT_OVERRIDE
   {
      return !Buffer.empty();
   }
};

static ResultState UnwrapHTTPConnect(std::string Host, int Port, URI Proxy, std::unique_ptr<MethodFd> &Fd,
				     unsigned long Timeout, aptAuthConfMethod *Owner)
{
   Owner->Status(_("Connecting to %s (%s)"), "HTTP proxy", URI::SiteOnly(Proxy).c_str());
   // The HTTP server expects a hostname with a trailing :port
   std::stringstream Req;
   std::string ProperHost;

   if (Host.find(':') != std::string::npos)
      ProperHost = '[' + Proxy.Host + ']';
   else
      ProperHost = Proxy.Host;

   // Build the connect
   Req << "CONNECT " << Host << ":" << std::to_string(Port) << " HTTP/1.1\r\n";
   if (Proxy.Port != 0)
      Req << "Host: " << ProperHost << ":" << std::to_string(Proxy.Port) << "\r\n";
   else
      Req << "Host: " << ProperHost << "\r\n";

   Owner->MaybeAddAuthTo(Proxy);
   if (Proxy.User.empty() == false || Proxy.Password.empty() == false)
      Req << "Proxy-Authorization: Basic "
	  << Base64Encode(Proxy.User + ":" + Proxy.Password) << "\r\n";

   Req << "User-Agent: " << Owner->ConfigFind("User-Agent", "Debian APT-HTTP/1.3 (" PACKAGE_VERSION ")") << "\r\n";

   Req << "\r\n";

   CircleBuf In(dynamic_cast<HttpMethod *>(Owner), 4096);
   CircleBuf Out(dynamic_cast<HttpMethod *>(Owner), 4096);
   std::string Headers;

   if (Owner->DebugEnabled() == true)
      cerr << Req.str() << endl;
   Out.Read(Req.str());

   // Writing from proxy
   while (Out.WriteSpace() > 0)
   {
      if (WaitFd(Fd->Fd(), true, Timeout) == false)
      {
	 _error->Errno("select", "Writing to proxy failed");
	 return ResultState::TRANSIENT_ERROR;
      }
      if (Out.Write(Fd) == false)
      {
	 _error->Errno("write", "Writing to proxy failed");
	 return ResultState::TRANSIENT_ERROR;
      }
   }

   while (In.ReadSpace() > 0)
   {
      if (WaitFd(Fd->Fd(), false, Timeout) == false)
      {
	 _error->Errno("select", "Reading from proxy failed");
	 return ResultState::TRANSIENT_ERROR;
      }
      if (In.Read(Fd) == false)
      {
	 _error->Errno("read", "Reading from proxy failed");
	 return ResultState::TRANSIENT_ERROR;
      }

      if (In.WriteTillEl(Headers))
	 break;
   }

   if (Owner->DebugEnabled() == true)
      cerr << Headers << endl;

   if (!(APT::String::Startswith(Headers, "HTTP/1.0 200") || APT::String::Startswith(Headers, "HTTP/1.1 200")))
   {
      _error->Error("Invalid response from proxy: %s", Headers.c_str());
      return ResultState::TRANSIENT_ERROR;
   }

   if (In.WriteSpace() > 0)
   {
      // Maybe there is actual data already read, if so we need to buffer it
      std::unique_ptr<HttpConnectFd> NewFd(new HttpConnectFd());
      In.Write(NewFd->Buffer);
      NewFd->UnderlyingFd = std::move(Fd);
      Fd = std::move(NewFd);
   }

   return ResultState::SUCCESSFUL;
}
									/*}}}*/

// HttpServerState::HttpServerState - Constructor			/*{{{*/
HttpServerState::HttpServerState(URI Srv,HttpMethod *Owner) : ServerState(Srv, Owner), In(Owner, 64*1024), Out(Owner, 4*1024)
{
   TimeOut = Owner->ConfigFindI("Timeout", TimeOut);
   ServerFd = MethodFd::FromFd(-1);
   Reset();
}
									/*}}}*/
// HttpServerState::Open - Open a connection to the server		/*{{{*/
// ---------------------------------------------------------------------
/* This opens a connection to the server. */
ResultState HttpServerState::Open()
{
   // Use the already open connection if possible.
   if (ServerFd->Fd() != -1)
      return ResultState::SUCCESSFUL;

   Close();
   In.Reset();
   Out.Reset();
   Persistent = true;

   bool tls = (ServerName.Access == "https" || APT::String::Endswith(ServerName.Access, "+https"));

   // Determine the proxy setting
   // Used to run AutoDetectProxy(ServerName) here, but we now send a Proxy
   // header in the URI Acquire request and set "Acquire::"+uri.Access+"::proxy::"+uri.Host
   // to it in BaseHttpMethod::Loop()
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
	      char *result = getenv("http_proxy");
	      Proxy = result ? result : "";
	      if (tls == true)
	      {
		 char *result = getenv("https_proxy");
		 if (result != nullptr)
		 {
		    Proxy = result;
		 }
	      }
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

   auto const DefaultService = tls ? "https" : "http";
   auto const DefaultPort = tls ? 443 : 80;
   if (Proxy.Access == "socks5h")
   {
      auto result = Connect(Proxy.Host, Proxy.Port, "socks", 1080, ServerFd, TimeOut, Owner);
      if (result != ResultState::SUCCESSFUL)
	 return result;

      result = UnwrapSocks(ServerName.Host, ServerName.Port == 0 ? DefaultPort : ServerName.Port,
			   Proxy, ServerFd, Owner->ConfigFindI("TimeOut", 120), Owner);
      if (result != ResultState::SUCCESSFUL)
	 return result;
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
      else if (Proxy.Access != "http" && Proxy.Access != "https")
      {
	 _error->Error("Unsupported proxy configured: %s", URI::SiteOnly(Proxy).c_str());
	 return ResultState::FATAL_ERROR;
      }
      else
      {
	 if (Proxy.Port != 0)
	    Port = Proxy.Port;
	 Host = Proxy.Host;

	 if (Proxy.Access == "https" && Port == 0)
	    Port = 443;
      }
      auto result = Connect(Host, Port, DefaultService, DefaultPort, ServerFd, TimeOut, Owner);
      if (result != ResultState::SUCCESSFUL)
	 return result;
      if (Host == Proxy.Host && Proxy.Access == "https")
      {
	 result = UnwrapTLS(Proxy.Host, ServerFd, TimeOut, Owner);
	 if (result != ResultState::SUCCESSFUL)
	    return result;
      }
      if (Host == Proxy.Host && tls)
      {
	 result = UnwrapHTTPConnect(ServerName.Host, ServerName.Port == 0 ? DefaultPort : ServerName.Port, Proxy, ServerFd, Owner->ConfigFindI("TimeOut", 120), Owner);
	 if (result != ResultState::SUCCESSFUL)
	    return result;
      }
   }

   if (tls)
      return UnwrapTLS(ServerName.Host, ServerFd, TimeOut, Owner);

   return ResultState::SUCCESSFUL;
}
									/*}}}*/
// HttpServerState::Close - Close a connection to the server		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool HttpServerState::Close()
{
   ServerFd->Close();
   return true;
}
									/*}}}*/
// HttpServerState::RunData - Transfer the data from the socket		/*{{{*/
ResultState HttpServerState::RunData(RequestState &Req)
{
   Req.State = RequestState::Data;
   
   // Chunked transfer encoding is fun..
   if (Req.Encoding == RequestState::Chunked)
   {
      while (1)
      {
	 // Grab the block size
	 ResultState Last = ResultState::SUCCESSFUL;
	 string Data;
	 In.Limit(-1);
	 do
	 {
	    if (In.WriteTillEl(Data,true) == true)
	       break;
	 } while ((Last = Go(false, Req)) == ResultState::SUCCESSFUL);

	 if (Last != ResultState::SUCCESSFUL)
	    return Last;

	 // See if we are done
	 unsigned long long Len = strtoull(Data.c_str(),0,16);
	 if (Len == 0)
	 {
	    In.Limit(-1);
	    
	    // We have to remove the entity trailer
	    Last = ResultState::SUCCESSFUL;
	    do
	    {
	       if (In.WriteTillEl(Data,true) == true && Data.length() <= 2)
		  break;
	    } while ((Last = Go(false, Req)) == ResultState::SUCCESSFUL);
	    return Last;
	 }

	 // Transfer the block
	 In.Limit(Len);
	 while (Go(true, Req) == ResultState::SUCCESSFUL)
	    if (In.IsLimit() == true)
	       break;
	 
	 // Error
	 if (In.IsLimit() == false)
	    return ResultState::TRANSIENT_ERROR;

	 // The server sends an extra new line before the next block specifier..
	 In.Limit(-1);
	 Last = ResultState::SUCCESSFUL;
	 do
	 {
	    if (In.WriteTillEl(Data,true) == true)
	       break;
	 } while ((Last = Go(false, Req)) == ResultState::SUCCESSFUL);
	 if (Last != ResultState::SUCCESSFUL)
	    return Last;
      }
   }
   else
   {
      /* Closes encoding is used when the server did not specify a size, the
         loss of the connection means we are done */
      if (Req.JunkSize != 0)
	 In.Limit(Req.JunkSize);
      else if (Req.DownloadSize != 0)
      {
	 if (Req.MaximumSize != 0 && Req.DownloadSize > Req.MaximumSize)
	 {
	    Owner->SetFailReason("MaximumSizeExceeded");
	    _error->Error(_("File has unexpected size (%llu != %llu). Mirror sync in progress?"),
			  Req.DownloadSize, Req.MaximumSize);
	    return ResultState::FATAL_ERROR;
	 }
	 In.Limit(Req.DownloadSize);
      }
      else if (Persistent == false)
	 In.Limit(-1);

      // Just transfer the whole block.
      while (true)
      {
	 if (In.IsLimit() == false)
	 {
	    auto const result = Go(true, Req);
	    if (result == ResultState::SUCCESSFUL)
	       continue;
	    return result;
	 }

	 In.Limit(-1);
	 return _error->PendingError() ? ResultState::FATAL_ERROR : ResultState::SUCCESSFUL;
      }
   }

   if (Flush(&Req.File) == false)
      return ResultState::TRANSIENT_ERROR;
   return ResultState::SUCCESSFUL;
}
									/*}}}*/
ResultState HttpServerState::RunDataToDevNull(RequestState &Req) /*{{{*/
{
   // no need to clean up if we discard the connection anyhow
   if (Persistent == false)
      return ResultState::SUCCESSFUL;
   Req.File.Open("/dev/null", FileFd::WriteOnly);
   return RunData(Req);
}
									/*}}}*/
bool HttpServerState::ReadHeaderLines(std::string &Data)		/*{{{*/
{
   return In.WriteTillEl(Data);
}
									/*}}}*/
ResultState HttpServerState::LoadNextResponse(bool const ToFile, RequestState &Req) /*{{{*/
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
   return (ServerFd->Fd() != -1);
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
   ServerFd->Close();
}
									/*}}}*/

APT_PURE Hashes * HttpServerState::GetHashes()				/*{{{*/
{
   return In.Hash;
}
									/*}}}*/
// HttpServerState::Die - The server has closed the connection.		/*{{{*/
ResultState HttpServerState::Die(RequestState &Req)
{
   unsigned int LErrno = errno;

   // Dump the buffer to the file
   if (Req.State == RequestState::Data)
   {
      if (Req.File.IsOpen() == false)
	 return ResultState::SUCCESSFUL;
      // on GNU/kFreeBSD, apt dies on /dev/null because non-blocking
      // can't be set
      if (Req.File.Name() != "/dev/null")
	 SetNonBlock(Req.File.Fd(),false);
      while (In.WriteSpace() == true)
      {
	 if (In.Write(MethodFd::FromFd(Req.File.Fd())) == false)
	 {
	    _error->Errno("write", _("Error writing to the file"));
	    return ResultState::TRANSIENT_ERROR;
	 }

	 // Done
	 if (In.IsLimit() == true)
	    return ResultState::SUCCESSFUL;
      }
   }

   // See if this is because the server finished the data stream
   if (In.IsLimit() == false && Req.State != RequestState::Header &&
       Persistent == true)
   {
      Close();
      if (LErrno == 0)
      {
	 _error->Error(_("Error reading from server. Remote end closed connection"));
	 return ResultState::TRANSIENT_ERROR;
      }
      errno = LErrno;
      _error->Errno("read", _("Error reading from server"));
      return ResultState::TRANSIENT_ERROR;
   }
   else
   {
      In.Limit(-1);

      // Nothing left in the buffer
      if (In.WriteSpace() == false)
	 return ResultState::TRANSIENT_ERROR;

      // We may have got multiple responses back in one packet..
      Close();
      return ResultState::SUCCESSFUL;
   }

   return ResultState::TRANSIENT_ERROR;
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
	 if (In.Write(MethodFd::FromFd(File->Fd())) == false)
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
ResultState HttpServerState::Go(bool ToFile, RequestState &Req)
{
   // Server has closed the connection
   if (ServerFd->Fd() == -1 && (In.WriteSpace() == false ||
				ToFile == false))
      return ResultState::TRANSIENT_ERROR;

   // Handle server IO
   if (ServerFd->HasPending() && In.ReadSpace() == true)
   {
      errno = 0;
      if (In.Read(ServerFd) == false)
	 return Die(Req);
   }

   fd_set rfds,wfds;
   FD_ZERO(&rfds);
   FD_ZERO(&wfds);
   
   /* Add the server. We only send more requests if the connection will 
      be persisting */
   if (Out.WriteSpace() == true && ServerFd->Fd() != -1 && Persistent == true)
      FD_SET(ServerFd->Fd(), &wfds);
   if (In.ReadSpace() == true && ServerFd->Fd() != -1)
      FD_SET(ServerFd->Fd(), &rfds);

   // Add the file
   auto FileFD = MethodFd::FromFd(-1);
   if (Req.File.IsOpen())
      FileFD = MethodFd::FromFd(Req.File.Fd());

   if (In.WriteSpace() == true && ToFile == true && FileFD->Fd() != -1)
      FD_SET(FileFD->Fd(), &wfds);

   // Add stdin
   if (Owner->ConfigFindB("DependOnSTDIN", true) == true)
      FD_SET(STDIN_FILENO,&rfds);
	  
   // Figure out the max fd
   int MaxFd = FileFD->Fd();
   if (MaxFd < ServerFd->Fd())
      MaxFd = ServerFd->Fd();

   // Select
   struct timeval tv;
   tv.tv_sec = TimeOut;
   tv.tv_usec = 0;
   int Res = 0;
   if ((Res = select(MaxFd+1,&rfds,&wfds,0,&tv)) < 0)
   {
      if (errno == EINTR)
	 return ResultState::SUCCESSFUL;
      _error->Errno("select", _("Select failed"));
      return ResultState::TRANSIENT_ERROR;
   }
   
   if (Res == 0)
   {
      _error->Error(_("Connection timed out"));
      return Die(Req);
   }
   
   // Handle server IO
   if (ServerFd->Fd() != -1 && FD_ISSET(ServerFd->Fd(), &rfds))
   {
      errno = 0;
      if (In.Read(ServerFd) == false)
	 return Die(Req);
   }

   if (ServerFd->Fd() != -1 && FD_ISSET(ServerFd->Fd(), &wfds))
   {
      errno = 0;
      if (Out.Write(ServerFd) == false)
	 return Die(Req);
   }

   // Send data to the file
   if (FileFD->Fd() != -1 && FD_ISSET(FileFD->Fd(), &wfds))
   {
      if (In.Write(FileFD) == false)
      {
	 _error->Errno("write", _("Error writing to output file"));
	 return ResultState::TRANSIENT_ERROR;
      }
   }

   if (Req.MaximumSize > 0 && Req.File.IsOpen() && Req.File.Failed() == false && Req.File.Tell() > Req.MaximumSize)
   {
      Owner->SetFailReason("MaximumSizeExceeded");
      _error->Error(_("File has unexpected size (%llu != %llu). Mirror sync in progress?"),
		    Req.File.Tell(), Req.MaximumSize);
      return ResultState::FATAL_ERROR;
   }

   // Handle commands from APT
   if (FD_ISSET(STDIN_FILENO,&rfds))
   {
      if (Owner->Run(true) != -1)
	 exit(100);
   }

   return ResultState::SUCCESSFUL;
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
   if ((Server->Proxy.Access != "http" && Server->Proxy.Access != "https") || APT::String::Endswith(Uri.Access, "https") || Server->Proxy.empty() == true || Server->Proxy.Host.empty())
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

   if ((Server->Proxy.Access == "http" || Server->Proxy.Access == "https") &&
       (Server->Proxy.User.empty() == false || Server->Proxy.Password.empty() == false))
      Req << "Proxy-Authorization: Basic "
	 << Base64Encode(Server->Proxy.User + ":" + Server->Proxy.Password) << "\r\n";

   MaybeAddAuthTo(Uri);
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
HttpMethod::HttpMethod(std::string &&pProg) : BaseHttpMethod(std::move(pProg), "1.2", Pipeline | SendConfig) /*{{{*/
{
   SeccompFlags = aptMethod::BASE | aptMethod::NETWORK;

   auto addName = std::inserter(methodNames, methodNames.begin());
   if (Binary != "http")
      addName = "http";
   auto const plus = Binary.find('+');
   if (plus != std::string::npos)
   {
      auto name2 = Binary.substr(plus + 1);
      if (std::find(methodNames.begin(), methodNames.end(), name2) == methodNames.end())
	 addName = std::move(name2);
      addName = Binary.substr(0, plus);
   }
}
									/*}}}*/

int main(int, const char *argv[])
{
   // ignore SIGPIPE, this can happen on write() if the socket
   // closes the connection (this is dealt with via ServerDie())
   signal(SIGPIPE, SIG_IGN);
   std::string Binary = flNotDir(argv[0]);
   if (Binary.find('+') == std::string::npos && Binary != "https" && Binary != "http")
      Binary.append("+http");
   return HttpMethod(std::move(Binary)).Loop();
}
