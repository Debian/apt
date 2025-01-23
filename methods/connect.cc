// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+ and curl
// Description								/*{{{*/
/* ######################################################################

   Connect - Replacement connect call

   This was originally authored by Jason Gunthorpe <jgg@debian.org>
   and is placed in the Public Domain, do with it what you will. It
   is now GPL-2.0+. See COPYING for details.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/srvrec.h>
#include <apt-pkg/strutl.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>

// Internet stuff
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "aptmethod.h"
#include "connect.h"
#include "rfc2553emu.h"
#include <apti18n.h>
									/*}}}*/

static std::string LastHost;
static std::string LastService;
static struct addrinfo *LastHostAddr = 0;
static struct addrinfo *LastUsed = 0;

static std::vector<SrvRec> SrvRecords;

// Set of IP/hostnames that we timed out before or couldn't resolve
static std::set<std::string> bad_addr;

// RotateDNS - Select a new server from a DNS rotation			/*{{{*/
// ---------------------------------------------------------------------
/* This is called during certain errors in order to recover by selecting a 
   new server */
void RotateDNS()
{
   if (LastUsed != 0 && LastUsed->ai_next != 0)
      LastUsed = LastUsed->ai_next;
   else
      LastUsed = LastHostAddr;
}
									/*}}}*/
static bool ConnectionAllowed(char const * const Service, std::string const &Host)/*{{{*/
{
   if (unlikely(Host.empty())) // the only legal empty host (RFC2782 '.' target) is detected by caller
      return false;
   if (APT::String::Endswith(Host, ".onion") && _config->FindB("Acquire::BlockDotOnion", true))
   {
      // TRANSLATOR: %s is e.g. Tor's ".onion" which would likely fail or leak info (RFC7686)
      _error->Error(_("Direct connection to %s domains is blocked by default."), ".onion");
      if (strcmp(Service, "http") == 0)
	_error->Error(_("If you meant to use Tor remember to use %s instead of %s."), "tor+http", "http");
      return false;
   }
   return true;
}
									/*}}}*/

// File Descriptor based Fd /*{{{*/
struct FdFd final : public MethodFd
{
   int fd = -1;
   int Fd() override { return fd; }
   ssize_t Read(void *buf, size_t count) override { return ::read(fd, buf, count); }
   ssize_t Write(void *buf, size_t count) override { return ::write(fd, buf, count); }
   int Close() override
   {
      int result = 0;
      if (fd != -1)
	 result = ::close(fd);
      fd = -1;
      return result;
   }
};

bool MethodFd::HasPending()
{
   return false;
}
std::unique_ptr<MethodFd> MethodFd::FromFd(int iFd)
{
   FdFd *fd = new FdFd();
   fd->fd = iFd;
   return std::unique_ptr<MethodFd>(fd);
}
									/*}}}*/
// DoConnect - Attempt a connect operation				/*{{{*/
// ---------------------------------------------------------------------
/* This helper function attempts a connection to a single address. */
struct Connection
{
   struct addrinfo *Addr;
   std::string Host;
   aptMethod *Owner;
   std::unique_ptr<FdFd> Fd;
   char Name[NI_MAXHOST];
   char Service[NI_MAXSERV];

   Connection(struct addrinfo *Addr, std::string const &Host, aptMethod *Owner) : Addr(Addr), Host(Host), Owner(Owner), Fd(new FdFd()), Name{0}, Service{0}
   {
   }

   // Allow moving values, but not connections.
   Connection(Connection &&Conn) = default;
   Connection(const Connection &Conn) = delete;
   Connection &operator=(const Connection &) = delete;
   Connection &operator=(Connection &&Conn) = default;

   ~Connection()
   {
      if (Fd != nullptr)
      {
	 Fd->Close();
      }
   }

   std::unique_ptr<MethodFd> Take()
   {
      /* Store the IP we are using.. If something goes
      wrong this will get tacked onto the end of the error message */
      std::stringstream ss;
      ioprintf(ss, _("[IP: %s %s]"), Name, Service);
      Owner->SetIP(ss.str());
      Owner->Status(_("Connected to %s (%s)"), Host.c_str(), Name);
      _error->Discard();
      Owner->SetFailReason("");
      LastUsed = Addr;
      return std::move(Fd);
   }

   ResultState DoConnect();

   ResultState CheckError();
};

ResultState Connection::DoConnect()
{
   getnameinfo(Addr->ai_addr,Addr->ai_addrlen,
	       Name,sizeof(Name),Service,sizeof(Service),
	       NI_NUMERICHOST|NI_NUMERICSERV);
   Owner->Status(_("Connecting to %s (%s)"),Host.c_str(),Name);

   // if that addr did timeout before, we do not try it again
   if(bad_addr.find(std::string(Name)) != bad_addr.end())
      return ResultState::TRANSIENT_ERROR;
      
   // Get a socket
   if ((static_cast<FdFd *>(Fd.get())->fd = socket(Addr->ai_family, Addr->ai_socktype,
						   Addr->ai_protocol)) < 0)
   {
      _error->Errno("socket", _("Could not create a socket for %s (f=%u t=%u p=%u)"),
		    Name, Addr->ai_family, Addr->ai_socktype, Addr->ai_protocol);
      return ResultState::FATAL_ERROR;
   }

   SetNonBlock(Fd->Fd(), true);
   if (connect(Fd->Fd(), Addr->ai_addr, Addr->ai_addrlen) < 0 &&
       errno != EINPROGRESS)
   {
      _error->Errno("connect", _("Cannot initiate the connection "
				 "to %s:%s (%s)."),
		    Host.c_str(), Service, Name);
      return ResultState::TRANSIENT_ERROR;
   }

   return ResultState::SUCCESSFUL;
}

ResultState Connection::CheckError()
{
   // Check the socket for an error condition
   unsigned int Err;
   unsigned int Len = sizeof(Err);
   if (getsockopt(Fd->Fd(), SOL_SOCKET, SO_ERROR, &Err, &Len) != 0)
   {
      _error->Errno("getsockopt", _("Failed"));
      return ResultState::FATAL_ERROR;
   }

   if (Err != 0)
   {
      errno = Err;
      if(errno == ECONNREFUSED)
         Owner->SetFailReason("ConnectionRefused");
      else if (errno == ETIMEDOUT)
	 Owner->SetFailReason("ConnectionTimedOut");
      bad_addr.insert(bad_addr.begin(), std::string(Name));
      _error->Errno("connect", _("Could not connect to %s:%s (%s)."), Host.c_str(),
		    Service, Name);
      return ResultState::TRANSIENT_ERROR;
   }

   Owner->SetFailReason("");

   return ResultState::SUCCESSFUL;
}
									/*}}}*/
// Order the given host names returned by getaddrinfo()			/*{{{*/
static std::vector<struct addrinfo *> OrderAddresses(struct addrinfo *CurHost)
{
   std::vector<struct addrinfo *> preferredAddrs;
   std::vector<struct addrinfo *> otherAddrs;
   std::vector<struct addrinfo *> allAddrs;

   // Partition addresses into preferred and other address families
   while (CurHost != 0)
   {
      if (preferredAddrs.empty() || CurHost->ai_family == preferredAddrs[0]->ai_family)
	 preferredAddrs.push_back(CurHost);
      else
	 otherAddrs.push_back(CurHost);

      // Ignore UNIX domain sockets
      do
      {
	 CurHost = CurHost->ai_next;
      } while (CurHost != 0 && CurHost->ai_family == AF_UNIX);

      /* If we reached the end of the search list then wrap around to the
	 start */
      if (CurHost == 0 && LastUsed != 0)
	 CurHost = LastHostAddr;

      // Reached the end of the search cycle
      if (CurHost == LastUsed)
	 break;
   }

   // Build a new address vector alternating between preferred and other
   for (auto prefIter = preferredAddrs.cbegin(), otherIter = otherAddrs.cbegin();
	prefIter != preferredAddrs.end() || otherIter != otherAddrs.end();)
   {
      if (prefIter != preferredAddrs.end())
	 allAddrs.push_back(*prefIter++);
      if (otherIter != otherAddrs.end())
	 allAddrs.push_back(*otherIter++);
   }

   return allAddrs;
}
									/*}}}*/
// Check for errors and report them					/*{{{*/
static ResultState WaitAndCheckErrors(std::list<Connection> &Conns, std::unique_ptr<MethodFd> &Fd, long TimeoutMsec, bool ReportTimeout)
{
   // The last error detected
   ResultState Result = ResultState::TRANSIENT_ERROR;

   struct timeval tv = {
      // Split our millisecond timeout into seconds and microseconds
      .tv_sec = TimeoutMsec / 1000,
      .tv_usec = (TimeoutMsec % 1000) * 1000,
   };

   // We will return once we have no more connections, a time out, or
   // a success.
   while (!Conns.empty())
   {
      fd_set Set;
      int nfds = -1;

      FD_ZERO(&Set);

      for (auto &Conn : Conns)
      {
	 int fd = Conn.Fd->Fd();
	 FD_SET(fd, &Set);
	 nfds = std::max(nfds, fd);
      }

      {
	 int Res;
	 do
	 {
	    Res = select(nfds + 1, 0, &Set, 0, (TimeoutMsec != 0 ? &tv : 0));
	 } while (Res < 0 && errno == EINTR);

	 if (Res == 0)
	 {
	    if (ReportTimeout)
	    {
	       for (auto &Conn : Conns)
	       {
		  Conn.Owner->SetFailReason("Timeout");
		  bad_addr.insert(bad_addr.begin(), Conn.Name);
		  _error->Error(_("Could not connect to %s:%s (%s), "
				  "connection timed out"),
				Conn.Host.c_str(), Conn.Service, Conn.Name);
	       }
	    }
	    return ResultState::TRANSIENT_ERROR;
	 }
      }

      // iterate over connections, remove failed ones, and return if
      // there was a successful one.
      for (auto ConnI = Conns.begin(); ConnI != Conns.end();)
      {
	 if (!FD_ISSET(ConnI->Fd->Fd(), &Set))
	 {
	    ConnI++;
	    continue;
	 }

	 Result = ConnI->CheckError();
	 if (Result == ResultState::SUCCESSFUL)
	 {
	    Fd = ConnI->Take();
	    return Result;
	 }

	 // Connection failed. Erase it and continue to next position
	 ConnI = Conns.erase(ConnI);
      }
   }

   return Result;
}
									/*}}}*/
// Connect to a given Hostname						/*{{{*/
static ResultState ConnectToHostname(std::string const &Host, int const Port,
				     const char *const Service, int DefPort, std::unique_ptr<MethodFd> &Fd,
				     unsigned long const TimeOut, aptMethod *const Owner)
{
   if (ConnectionAllowed(Service, Host) == false)
      return ResultState::FATAL_ERROR;

   // Used by getaddrinfo(); prefer port if given, else fallback to service
   std::string ServiceNameOrPort = Port != 0 ? std::to_string(Port) : Service;
   
   /* We used a cached address record.. Yes this is against the spec but
      the way we have setup our rotating dns suggests that this is more
      sensible */
   if (LastHost != Host || LastService != ServiceNameOrPort)
   {
      Owner->Status(_("Connecting to %s"),Host.c_str());

      // Free the old address structure
      if (LastHostAddr != 0)
      {
	 freeaddrinfo(LastHostAddr);
	 LastHostAddr = 0;
	 LastUsed = 0;
      }
      
      // We only understand SOCK_STREAM sockets.
      struct addrinfo Hints;
      memset(&Hints,0,sizeof(Hints));
      Hints.ai_socktype = SOCK_STREAM;
      Hints.ai_flags = 0;
#ifdef AI_IDN
      if (_config->FindB("Acquire::Connect::IDN", true) == true)
	 Hints.ai_flags |= AI_IDN;
#endif
      // see getaddrinfo(3): only return address if system has such a address configured
      // useful if system is ipv4 only, to not get ipv6, but that fails if the system has
      // no address configured: e.g. offline and trying to connect to localhost.
      if (_config->FindB("Acquire::Connect::AddrConfig", true) == true)
	 Hints.ai_flags |= AI_ADDRCONFIG;
      Hints.ai_protocol = 0;
      
      if(_config->FindB("Acquire::ForceIPv4", false) == true)
         Hints.ai_family = AF_INET;
      else if(_config->FindB("Acquire::ForceIPv6", false) == true)
         Hints.ai_family = AF_INET6;
      else
         Hints.ai_family = AF_UNSPEC;

      // if we couldn't resolve the host before, we don't try now
      if (bad_addr.find(Host) != bad_addr.end())
      {
	 _error->Error(_("Could not resolve '%s'"), Host.c_str());
	 return ResultState::TRANSIENT_ERROR;
      }

      // Resolve both the host and service simultaneously
      while (1)
      {
	 int Res;
	 if ((Res = getaddrinfo(Host.c_str(), ServiceNameOrPort.c_str(), &Hints, &LastHostAddr)) != 0 ||
	     LastHostAddr == 0)
	 {
	    if (Res == EAI_NONAME || Res == EAI_SERVICE)
	    {
	       if (DefPort != 0)
	       {
		  ServiceNameOrPort = std::to_string(DefPort);
		  DefPort = 0;
		  continue;
	       }
	       bad_addr.insert(bad_addr.begin(), Host);
	       Owner->SetFailReason("ResolveFailure");
	       _error->Error(_("Could not resolve '%s'"), Host.c_str());
	       return ResultState::TRANSIENT_ERROR;
	    }
	    
	    if (Res == EAI_AGAIN)
	    {
	       Owner->SetFailReason("TmpResolveFailure");
	       _error->Error(_("Temporary failure resolving '%s'"),
			     Host.c_str());
	       return ResultState::TRANSIENT_ERROR;
	    }
	    if (Res == EAI_SYSTEM)
	       _error->Errno("getaddrinfo", _("System error resolving '%s:%s'"),
			     Host.c_str(), ServiceNameOrPort.c_str());
	    else
	       _error->Error(_("Something wicked happened resolving '%s:%s' (%i - %s)"),
			     Host.c_str(), ServiceNameOrPort.c_str(), Res, gai_strerror(Res));
	    return ResultState::TRANSIENT_ERROR;
	 }
	 break;
      }
      
      LastHost = Host;
      LastService = ServiceNameOrPort;
   }

   // When we have an IP rotation stay with the last IP.
   auto Addresses = OrderAddresses(LastUsed != nullptr ? LastUsed : LastHostAddr);
   std::list<Connection> Conns;
   ResultState Result = ResultState::SUCCESSFUL;

   for (auto Addr : Addresses)
   {
      Connection Conn(Addr, Host, Owner);
      if (Conn.DoConnect() != ResultState::SUCCESSFUL)
	 continue;

      Conns.push_back(std::move(Conn));

      Result = WaitAndCheckErrors(Conns, Fd, Owner->ConfigFindI("ConnectionAttemptDelayMsec", 250), false);

      if (Result == ResultState::SUCCESSFUL)
	 return ResultState::SUCCESSFUL;
   }

   if (!Conns.empty())
      return WaitAndCheckErrors(Conns, Fd, TimeOut * 1000, true);
   if (Result != ResultState::SUCCESSFUL)
      return Result;
   if (_error->PendingError() == true)
      return ResultState::FATAL_ERROR;
   _error->Error(_("Unable to connect to %s:%s:"), Host.c_str(), ServiceNameOrPort.c_str());
   return ResultState::TRANSIENT_ERROR;
}
									/*}}}*/
// Connect - Connect to a server					/*{{{*/
// ---------------------------------------------------------------------
/* Performs a connection to the server (including SRV record lookup) */
ResultState Connect(std::string Host, int Port, const char *Service,
		    int DefPort, std::unique_ptr<MethodFd> &Fd,
		    unsigned long TimeOut, aptMethod *Owner)
{
   if (_error->PendingError() == true)
      return ResultState::FATAL_ERROR;

   if (ConnectionAllowed(Service, Host) == false)
      return ResultState::FATAL_ERROR;

   // Used by getaddrinfo(); prefer port if given, else fallback to service
   std::string ServiceNameOrPort = Port != 0 ? std::to_string(Port) : Service;

   if(LastHost != Host || LastService != ServiceNameOrPort)
   {
      SrvRecords.clear();
      if (_config->FindB("Acquire::EnableSrvRecords", true) == true)
      {
         GetSrvRecords(Host, DefPort, SrvRecords);
	 // RFC2782 defines that a lonely '.' target is an abort reason
	 if (SrvRecords.size() == 1 && SrvRecords[0].target.empty())
	 {
	    _error->Error("SRV records for %s indicate that "
			  "%s service is not available at this domain",
			  Host.c_str(), Service);
	    return ResultState::FATAL_ERROR;
	 }
      }
   }

   size_t stackSize = 0;
   // try to connect in the priority order of the srv records
   std::string initialHost{std::move(Host)};
   auto const initialPort = Port;
   while(SrvRecords.empty() == false)
   {
      _error->PushToStack();
      ++stackSize;
      // PopFromSrvRecs will also remove the server
      auto Srv = PopFromSrvRecs(SrvRecords);
      Host = Srv.target;
      Port = Srv.port;
      auto const ret = ConnectToHostname(Host, Port, Service, DefPort, Fd, TimeOut, Owner);
      if (ret == ResultState::SUCCESSFUL)
      {
	 while(stackSize--)
	    _error->RevertToStack();
	 return ret;
      }
   }
   Host = std::move(initialHost);
   Port = initialPort;

   // we have no (good) SrvRecords for this host, connect right away
   _error->PushToStack();
   ++stackSize;
   auto const ret = ConnectToHostname(Host, Port, Service, DefPort, Fd,
	 TimeOut, Owner);
   while(stackSize--)
      if (ret == ResultState::SUCCESSFUL)
	 _error->RevertToStack();
      else
	 _error->MergeWithStack();
   return ret;
}
									/*}}}*/
// UnwrapSocks - Handle SOCKS setup					/*{{{*/
// ---------------------------------------------------------------------
/* This does socks magic */
static bool TalkToSocksProxy(int const ServerFd, std::string const &Proxy,
			     char const *const type, bool const ReadWrite, uint8_t *const ToFrom,
			     unsigned int const Size, unsigned int const Timeout)
{
   if (WaitFd(ServerFd, ReadWrite, Timeout) == false)
   {
      if (ReadWrite)
	 return _error->Error("Timed out while waiting to write '%s' to proxy %s", type, URI::SiteOnly(Proxy).c_str());
      else
	 return _error->Error("Timed out while waiting to read '%s' from proxy %s", type, URI::SiteOnly(Proxy).c_str());
   }
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

ResultState UnwrapSocks(std::string Host, int Port, URI Proxy, std::unique_ptr<MethodFd> &Fd,
			unsigned long Timeout, aptMethod *Owner)
{
   /* We implement a very basic SOCKS5 client here complying mostly to RFC1928 expect
    * for not offering GSSAPI auth which is a must (we only do no or user/pass auth).
    * We also expect the SOCKS5 server to do hostname lookup (aka socks5h) */
   std::string const ProxyInfo = URI::SiteOnly(Proxy);
   Owner->Status(_("Connecting to %s (%s)"), "SOCKS5h proxy", ProxyInfo.c_str());
#define APT_WriteOrFail(TYPE, DATA, LENGTH)                                               \
   if (TalkToSocksProxy(Fd->Fd(), ProxyInfo, TYPE, true, DATA, LENGTH, Timeout) == false) \
   return ResultState::TRANSIENT_ERROR
#define APT_ReadOrFail(TYPE, DATA, LENGTH)                                                 \
   if (TalkToSocksProxy(Fd->Fd(), ProxyInfo, TYPE, false, DATA, LENGTH, Timeout) == false) \
   return ResultState::TRANSIENT_ERROR
   if (Host.length() > 255)
   {
      _error->Error("Can't use SOCKS5h as hostname %s is too long!", Host.c_str());
      return ResultState::FATAL_ERROR;
   }
   if (Proxy.User.length() > 255 || Proxy.Password.length() > 255)
   {
      _error->Error("Can't use user&pass auth as they are too long (%lu and %lu) for the SOCKS5!", Proxy.User.length(), Proxy.Password.length());
      return ResultState::FATAL_ERROR;
   }
   if (Proxy.User.empty())
   {
      uint8_t greeting[] = {0x05, 0x01, 0x00};
      APT_WriteOrFail("greet-1", greeting, sizeof(greeting));
   }
   else
   {
      uint8_t greeting[] = {0x05, 0x02, 0x00, 0x02};
      APT_WriteOrFail("greet-2", greeting, sizeof(greeting));
   }
   uint8_t greeting[2];
   APT_ReadOrFail("greet back", greeting, sizeof(greeting));
   if (greeting[0] != 0x05)
   {
      _error->Error("SOCKS proxy %s greets back with wrong version: %d", ProxyInfo.c_str(), greeting[0]);
      return ResultState::FATAL_ERROR;
   }
   if (greeting[1] == 0x00)
      ; // no auth has no method-dependent sub-negotiations
   else if (greeting[1] == 0x02)
   {
      if (Proxy.User.empty())
      {
	 _error->Error("SOCKS proxy %s negotiated user&pass auth, but we had not offered it!", ProxyInfo.c_str());
	 return ResultState::FATAL_ERROR;
      }
      // user&pass auth sub-negotiations are defined by RFC1929
      std::vector<uint8_t> auth = {{0x01, static_cast<uint8_t>(Proxy.User.length())}};
      std::copy(Proxy.User.begin(), Proxy.User.end(), std::back_inserter(auth));
      auth.push_back(static_cast<uint8_t>(Proxy.Password.length()));
      std::copy(Proxy.Password.begin(), Proxy.Password.end(), std::back_inserter(auth));
      APT_WriteOrFail("user&pass auth", auth.data(), auth.size());
      uint8_t authstatus[2];
      APT_ReadOrFail("auth report", authstatus, sizeof(authstatus));
      if (authstatus[0] != 0x01)
      {
	 _error->Error("SOCKS proxy %s auth status response with wrong version: %d", ProxyInfo.c_str(), authstatus[0]);
	 return ResultState::FATAL_ERROR;
      }
      if (authstatus[1] != 0x00)
      {
	 _error->Error("SOCKS proxy %s reported authorization failure: username or password incorrect? (%d)", ProxyInfo.c_str(), authstatus[1]);
	 return ResultState::FATAL_ERROR;
      }
   }
   else
   {
      _error->Error("SOCKS proxy %s greets back having not found a common authorization method: %d", ProxyInfo.c_str(), greeting[1]);
      return ResultState::FATAL_ERROR;
   }
   union {
      uint16_t *i;
      uint8_t *b;
   } portu;
   uint16_t port = htons(static_cast<uint16_t>(Port));
   portu.i = &port;
   std::vector<uint8_t> request = {{0x05, 0x01, 0x00, 0x03, static_cast<uint8_t>(Host.length())}};
   std::copy(Host.begin(), Host.end(), std::back_inserter(request));
   request.push_back(portu.b[0]);
   request.push_back(portu.b[1]);
   APT_WriteOrFail("request", request.data(), request.size());
   uint8_t response[4];
   APT_ReadOrFail("first part of response", response, sizeof(response));
   if (response[0] != 0x05)
   {
      _error->Error("SOCKS proxy %s response with wrong version: %d", ProxyInfo.c_str(), response[0]);
      return ResultState::FATAL_ERROR;
   }
   if (response[2] != 0x00)
   {
      _error->Error("SOCKS proxy %s has unexpected non-zero reserved field value: %d", ProxyInfo.c_str(), response[2]);
      return ResultState::FATAL_ERROR;
   }
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
   {
      _error->Error("SOCKS proxy %s destination address is of unknown type: %d",
		    ProxyInfo.c_str(), response[3]);
      return ResultState::FATAL_ERROR;
   }
   if (response[1] != 0x00)
   {
      char const *errstr = nullptr;
      auto errcode = response[1];
      bool Transient = false;
      // Tor error reporting can be a bit arcane, lets try to detect & fix it up
      if (bindaddr == "0.0.0.0:0")
      {
	 auto const lastdot = Host.rfind('.');
	 if (lastdot == std::string::npos || Host.substr(lastdot) != ".onion")
	    ;
	 else if (errcode == 0x01)
	 {
	    auto const prevdot = Host.rfind('.', lastdot - 1);
	    if (prevdot == std::string::npos && (lastdot == 16 || lastdot == 56))
	       ; // valid .onion address
	    else if (prevdot != std::string::npos && ((lastdot - prevdot) == 17 || (lastdot - prevdot) == 57))
	       ; // valid .onion address with subdomain(s)
	    else
	    {
	       errstr = "Invalid hostname: onion service name must be either 16 or 56 characters long";
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
	 case 0x01:
	    errstr = "general SOCKS server failure";
	    Owner->SetFailReason("SOCKS");
	    break;
	 case 0x02:
	    errstr = "connection not allowed by ruleset";
	    Owner->SetFailReason("SOCKS");
	    break;
	 case 0x03:
	    errstr = "Network unreachable";
	    Owner->SetFailReason("ConnectionTimedOut");
	    Transient = true;
	    break;
	 case 0x04:
	    errstr = "Host unreachable";
	    Owner->SetFailReason("ConnectionTimedOut");
	    Transient = true;
	    break;
	 case 0x05:
	    errstr = "Connection refused";
	    Owner->SetFailReason("ConnectionRefused");
	    Transient = true;
	    break;
	 case 0x06:
	    errstr = "TTL expired";
	    Owner->SetFailReason("Timeout");
	    Transient = true;
	    break;
	 case 0x07:
	    errstr = "Command not supported";
	    Owner->SetFailReason("SOCKS");
	    break;
	 case 0x08:
	    errstr = "Address type not supported";
	    Owner->SetFailReason("SOCKS");
	    break;
	 default:
	    errstr = "Unknown error";
	    Owner->SetFailReason("SOCKS");
	    break;
	 }
      }
      _error->Error("SOCKS proxy %s could not connect to %s (%s) due to: %s (%d)",
		    ProxyInfo.c_str(), Host.c_str(), bindaddr.c_str(), errstr, response[1]);
      return Transient ? ResultState::TRANSIENT_ERROR : ResultState::FATAL_ERROR;
   }
   else if (Owner->DebugEnabled())
      ioprintf(std::clog, "http: SOCKS proxy %s connection established to %s (%s)\n",
	       ProxyInfo.c_str(), Host.c_str(), bindaddr.c_str());

   if (WaitFd(Fd->Fd(), true, Timeout) == false)
   {
      _error->Error("SOCKS proxy %s reported connection to %s (%s), but timed out",
		    ProxyInfo.c_str(), Host.c_str(), bindaddr.c_str());
      return ResultState::TRANSIENT_ERROR;
   }
#undef APT_ReadOrFail
#undef APT_WriteOrFail

   return ResultState::SUCCESSFUL;
}

// UnwrapTLS - Handle TLS connections 					/*{{{*/
// ---------------------------------------------------------------------
/* Performs a TLS handshake on the socket */
#define null_error(...) (_error->Error(__VA_ARGS__), nullptr)
#define ssl_strerr() ERR_error_string(ERR_get_error(), nullptr)
struct TlsFd final : public MethodFd
{
   std::unique_ptr<MethodFd> UnderlyingFd{};

   SSL *ssl{};

   std::string hostname{};
   unsigned long Timeout{};
   bool broken{false};

   int Fd() override { return UnderlyingFd ? UnderlyingFd->Fd() : -1; }

   ssize_t Read(void *buf, size_t count) override
   {
      assert(ssl);
      return HandleError(SSL_read(ssl, buf, count));
   }
   ssize_t Write(void *buf, size_t count) override
   {
      assert(ssl);
      return HandleError(SSL_write(ssl, buf, count));
   }

   ssize_t HandleError(ssize_t r)
   {
      assert(ssl);
      if (r > 0)
	 return r;

      auto err = SSL_get_error(ssl, r);
      switch (err)
      {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
	 errno = EAGAIN;
	 return -1;
      case SSL_ERROR_ZERO_RETURN:
	 // Remote host has closed connection
	 errno = 0;
	 return 0;
      case SSL_ERROR_SYSCALL:
	 broken = true;
	 _error->Errno("openssl", "OpenSSL system call error: %s\n", ssl_strerr());
	 return -1;
      case SSL_ERROR_SSL:
	 broken = true;
	 errno = EIO;
	 _error->Error("OpenSSL error: %s\n", ssl_strerr());
	 return -1;
      default:
	 broken = true;
	 errno = EIO;
	 _error->Error("Unexpected OpenSSL error: %s\n", ssl_strerr());
	 return -1;
      }
   }

   int Close() override
   {
      int res = 0;   // 0 or 1 are success
      int lower = 0; // 0 is success

      if (ssl)
      {
	 if (not broken && res)
	    res = SSL_shutdown(ssl);
	 if (res < 0)
	    HandleError(res);
	 SSL_free(ssl);
      }
      ssl = nullptr;

      if (UnderlyingFd)
	 lower = UnderlyingFd->Close();
      UnderlyingFd = nullptr;

      // Return -1 on failure, 0 on success
      return res < 0 || lower < 0 ? -1 : 0;
   }

   bool HasPending() override
   {
      assert(ssl);
      // SSL_has_pending() can return 1 even if there are no actual bytes to read
      // post decoding, so we need to SSL_peek() too to see if there are actual
      // bytes as otherwise we end up busy looping (tested by DE -> AU https connection)
      char buf;
      return SSL_has_pending(ssl) && SSL_peek(ssl, &buf, 1) > 0;
   }
};

static BIO_METHOD *NewBioMethod()
{
   BIO_METHOD *m = BIO_meth_new(BIO_TYPE_MEM, "OpenSSL APT BIO method");
   if (not m)
   {
      _error->Error("SSL connection failed: %s - %s", ssl_strerr(), strerror(errno));
      return nullptr;
   }

   BIO_meth_set_ctrl(m, [](BIO *, int, long, void *) -> long
		     { return 1; });
   BIO_meth_set_write(m, [](BIO *bio, const char *buf, int size) -> int
		      {
      auto p = BIO_get_data(bio);
      auto res = reinterpret_cast<MethodFd *>(p)->Write((void*)buf, size);
      if (errno == EAGAIN)
	 BIO_set_retry_write(bio);
      return res; });
   BIO_meth_set_read(m, [](BIO *bio, char *buf, int size) -> int
		     {
      auto p = BIO_get_data(bio);
      auto res = reinterpret_cast<MethodFd *>(p)->Read(buf, size);
      if (errno == EAGAIN)
	 BIO_set_retry_read(bio);
      return res; });

   return m;
}

static SSL_CTX *GetContextForHost(std::string const &host, aptConfigWrapperForMethods const *const OwnerConf)
{
   static std::string lastHost;
   static SSL_CTX *ctx;
   auto Debug = OwnerConf->DebugEnabled();

   if (lastHost == host)
   {
      if (Debug)
	 std::clog << "Reusing context for " << host << std::endl;
      return ctx;
   }
   if (Debug)
      std::clog << "Creating context for " << host << std::endl;

   // Check unsupported options first, maybe we reuse the host context later, who knows
   if (not OwnerConf->ConfigFind("IssuerCert", "").empty())
      return null_error("The option '%s' is not supported anymore", "IssuerCert");
   if (not OwnerConf->ConfigFind("SslForceVersion", "").empty())
      return null_error("The option '%s' is not supported anymore", "SslForceVersion");

   // Delete the existing context and render it unusable
   lastHost = "";
   SSL_CTX_free(ctx);

   // We set the context here, but lastHost at the end to only allow reuse of fully initialized contexts
   ctx = SSL_CTX_new(TLS_client_method());
   if (ctx == nullptr)
      return null_error("Could not create new SSL context: %s", ssl_strerr());

   // Load the certificate authorities, either custom or default ones
   if (auto const fileinfo = OwnerConf->ConfigFind("CaInfo", ""); not fileinfo.empty())
   {
      if (auto res = SSL_CTX_load_verify_file(ctx, fileinfo.c_str()); res != 1)
	 return null_error("Could not load certificates from %s (CaInfo option): %s", fileinfo.c_str(), ssl_strerr());
   }
   else if (auto err = SSL_CTX_set_default_verify_paths(ctx); err != 1)
      return null_error("Could not load certificates: %s", ssl_strerr());

   // Client certificate setup, such that clients can authenticate to the server
   if (auto const cert = OwnerConf->ConfigFind("SslCert", ""); not cert.empty())
      if (auto res = SSL_CTX_use_certificate_chain_file(ctx, cert.c_str()); res != 1)
	 return null_error("Could not load client certificate (%s, SslCert option): %s", cert.c_str(), ssl_strerr());
   if (auto const key = OwnerConf->ConfigFind("SslKey", ""); not key.empty())
      if (auto res = SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM); res != 1)
	 return null_error("Could not load client or key (%s, SslKey option): %s", key.c_str(), ssl_strerr()), nullptr;

   // Custom certificate revocation lists. We surely support all the niche cases.
   if (auto const crlfile = OwnerConf->ConfigFind("CrlFile", ""); not crlfile.empty())
   {
      // tell OpenSSL where to find CRL file that is used to check certificate  revocation.
      // lifted from curl:
      // Copyright (c) 1996 - 2023, Daniel Stenberg, <daniel@haxx.se>, and many
      // contributors, see the THANKS file.
      auto lookup = X509_STORE_add_lookup(SSL_CTX_get_cert_store(ctx),
					  X509_LOOKUP_file());
      if (not lookup || not X509_load_crl_file(lookup, crlfile.c_str(), X509_FILETYPE_PEM))
	 return null_error("Could not load custom certificate revocation list %s (CrlFile option): %s", crlfile.c_str(), ssl_strerr());
      /* Everything is fine. */
      X509_STORE_set_flags(SSL_CTX_get_cert_store(ctx),
			   X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
   }

   lastHost = host;
   return ctx;
}

ResultState UnwrapTLS(std::string const &Host, std::unique_ptr<MethodFd> &Fd,
		      unsigned long const Timeout, aptMethod *const /*Owner*/,
		      aptConfigWrapperForMethods const *const OwnerConf)
{
   if (_config->FindB("Acquire::AllowTLS", true) == false)
   {
      _error->Error("TLS support has been disabled: Acquire::AllowTLS is false.");
      return ResultState::FATAL_ERROR;
   }

   TlsFd *tlsFd = new TlsFd();

   tlsFd->hostname = Host;
   tlsFd->Timeout = Timeout;

   if (auto ctx = GetContextForHost(Host, OwnerConf))
      tlsFd->ssl = SSL_new(ctx);
   else
      return ResultState::FATAL_ERROR;

   FdFd *fdfd = dynamic_cast<FdFd *>(Fd.get());
   if (fdfd != nullptr)
   {
      SSL_set_fd(tlsFd->ssl, fdfd->fd);
   }
   else
   {
      static auto m = NewBioMethod();
      if (m == nullptr)
	 return ResultState::FATAL_ERROR;

      auto bio = BIO_new(m);
      if (bio == nullptr)
	 return ResultState::FATAL_ERROR;

      BIO_set_data(bio, Fd.get());
      BIO_up_ref(bio); // the following take one reference each
      SSL_set0_rbio(tlsFd->ssl, bio);
      SSL_set0_wbio(tlsFd->ssl, bio);
   }

   if (OwnerConf->ConfigFindB("Verify-Peer", true))
   {
      SSL_set_verify(tlsFd->ssl, SSL_VERIFY_PEER, nullptr);
      if (auto res = SSL_set1_host(tlsFd->ssl, OwnerConf->ConfigFindB("Verify-Host", true) ? tlsFd->hostname.c_str() : nullptr); res != 1)
      {
	 _error->Error("Could not set hostname: %s", ssl_strerr());
	 return ResultState::FATAL_ERROR;
      }
   }

   // set SNI only if the hostname is really a name and not an address
   {
      struct in_addr addr4;
      struct in6_addr addr6;

      if (inet_pton(AF_INET, tlsFd->hostname.c_str(), &addr4) == 1 ||
	  inet_pton(AF_INET6, tlsFd->hostname.c_str(), &addr6) == 1)
	 /* not a host name */;
      else if (auto res = SSL_set_tlsext_host_name(tlsFd->ssl, tlsFd->hostname.c_str()); res != 1)
      {
	 _error->Error("Could not set host name %s to indicate to server: %s", tlsFd->hostname.c_str(), ssl_strerr());
	 return ResultState::FATAL_ERROR;
      }
   }

   while (true)
   {
      auto res = SSL_connect(tlsFd->ssl);
      if (res == 1)
	 break;
      switch (auto error = SSL_get_error(tlsFd->ssl, res))
      {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
	 if (not WaitFd(Fd->Fd(), error == SSL_ERROR_WANT_WRITE, Timeout))
	 {
	    _error->Errno("select", "Could not wait for server fd");
	    return ResultState::TRANSIENT_ERROR;
	 }
	 break;
      default:
	 _error->Error("SSL connection failed: %s / %s", ssl_strerr(), strerror(errno));
	 return ResultState::TRANSIENT_ERROR;
      }
   }

   // Set the FD now, so closing it works reliably.
   tlsFd->UnderlyingFd = std::move(Fd);
   Fd.reset(tlsFd);

   return ResultState::SUCCESSFUL;
}
