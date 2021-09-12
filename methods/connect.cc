// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Connect - Replacement connect call

   This was originally authored by Jason Gunthorpe <jgg@debian.org>
   and is placed in the Public Domain, do with it what you will.
      
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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include <list>
#include <set>
#include <sstream>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <string.h>
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
struct FdFd : public MethodFd
{
   int fd = -1;
   int Fd() APT_OVERRIDE { return fd; }
   ssize_t Read(void *buf, size_t count) APT_OVERRIDE { return ::read(fd, buf, count); }
   ssize_t Write(void *buf, size_t count) APT_OVERRIDE { return ::write(fd, buf, count); }
   int Close() APT_OVERRIDE
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
									/*}}}*/
// UnwrapTLS - Handle TLS connections 					/*{{{*/
// ---------------------------------------------------------------------
/* Performs a TLS handshake on the socket */
struct TlsFd : public MethodFd
{
   std::unique_ptr<MethodFd> UnderlyingFd;
   gnutls_session_t session;
   gnutls_certificate_credentials_t credentials;
   std::string hostname;
   unsigned long Timeout;

   int Fd() APT_OVERRIDE { return UnderlyingFd->Fd(); }

   ssize_t Read(void *buf, size_t count) APT_OVERRIDE
   {
      return HandleError(gnutls_record_recv(session, buf, count));
   }
   ssize_t Write(void *buf, size_t count) APT_OVERRIDE
   {
      return HandleError(gnutls_record_send(session, buf, count));
   }

   ssize_t DoTLSHandshake()
   {
      int err;
      // Do the handshake. Our socket is non-blocking, so we need to call WaitFd()
      // accordingly.
      do
      {
         err = gnutls_handshake(session);
         if ((err == GNUTLS_E_INTERRUPTED || err == GNUTLS_E_AGAIN) &&
             WaitFd(this->Fd(), gnutls_record_get_direction(session) == 1, Timeout) == false)
         {
            _error->Errno("select", "Could not wait for server fd");
            return err;
         }
      } while (err < 0 && gnutls_error_is_fatal(err) == 0);

      if (err < 0)
      {
         // Print reason why validation failed.
         if (err == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR)
         {
            gnutls_datum_t txt;
            auto type = gnutls_certificate_type_get(session);
            auto status = gnutls_session_get_verify_cert_status(session);
            if (gnutls_certificate_verification_status_print(status, type, &txt, 0) == 0)
            {
               _error->Error("Certificate verification failed: %s", txt.data);
            }
            gnutls_free(txt.data);
         }
         _error->Error("Could not handshake: %s", gnutls_strerror(err));
      }
      return err;
   }

   template <typename T>
   T HandleError(T err)
   {
      // Server may request re-handshake if client certificates need to be provided
      // based on resource requested
      if (err == GNUTLS_E_REHANDSHAKE)
      {
        int rc = DoTLSHandshake();
	// Only reset err if DoTLSHandshake() fails.
        // Otherwise, we want to follow the original error path and set errno to EAGAIN
        // so that the request is retried.
        if (rc < 0)
          err = rc;
      }

      if (err < 0 && gnutls_error_is_fatal(err))
	 errno = EIO;
      else if (err < 0)
	 errno = EAGAIN;
      else
	 errno = 0;
      return err;
   }

   int Close() APT_OVERRIDE
   {
      auto err = HandleError(gnutls_bye(session, GNUTLS_SHUT_RDWR));
      auto lower = UnderlyingFd->Close();
      return err < 0 ? HandleError(err) : lower;
   }

   bool HasPending() APT_OVERRIDE
   {
      return gnutls_record_check_pending(session) > 0;
   }
};

ResultState UnwrapTLS(std::string const &Host, std::unique_ptr<MethodFd> &Fd,
		      unsigned long const Timeout, aptMethod * const Owner,
		      aptConfigWrapperForMethods const * const OwnerConf)
{
   if (_config->FindB("Acquire::AllowTLS", true) == false)
   {
      _error->Error("TLS support has been disabled: Acquire::AllowTLS is false.");
      return ResultState::FATAL_ERROR;
   }

   int err;
   TlsFd *tlsFd = new TlsFd();

   tlsFd->hostname = Host;
   tlsFd->UnderlyingFd = MethodFd::FromFd(-1); // For now
   tlsFd->Timeout = Timeout;

   if ((err = gnutls_init(&tlsFd->session, GNUTLS_CLIENT | GNUTLS_NONBLOCK)) < 0)
   {
      _error->Error("Internal error: could not allocate credentials: %s", gnutls_strerror(err));
      return ResultState::FATAL_ERROR;
   }

   FdFd *fdfd = dynamic_cast<FdFd *>(Fd.get());
   if (fdfd != nullptr)
   {
      gnutls_transport_set_int(tlsFd->session, fdfd->fd);
   }
   else
   {
      gnutls_transport_set_ptr(tlsFd->session, Fd.get());
      gnutls_transport_set_pull_function(tlsFd->session,
					 [](gnutls_transport_ptr_t p, void *buf, size_t size) -> ssize_t {
					    return reinterpret_cast<MethodFd *>(p)->Read(buf, size);
					 });
      gnutls_transport_set_push_function(tlsFd->session,
					 [](gnutls_transport_ptr_t p, const void *buf, size_t size) -> ssize_t {
					    return reinterpret_cast<MethodFd *>(p)->Write((void *)buf, size);
					 });
   }

   if ((err = gnutls_certificate_allocate_credentials(&tlsFd->credentials)) < 0)
   {
      _error->Error("Internal error: could not allocate credentials: %s", gnutls_strerror(err));
      return ResultState::FATAL_ERROR;
   }

   // Credential setup
   std::string fileinfo = OwnerConf->ConfigFind("CaInfo", "");
   if (fileinfo.empty())
   {
      // No CaInfo specified, use system trust store.
      err = gnutls_certificate_set_x509_system_trust(tlsFd->credentials);
      if (err == 0)
	 Owner->Warning("No system certificates available. Try installing ca-certificates.");
      else if (err < 0)
      {
	 _error->Error("Could not load system TLS certificates: %s", gnutls_strerror(err));
	 return ResultState::FATAL_ERROR;
      }
   }
   else
   {
      // CA location has been set, use the specified one instead
      gnutls_certificate_set_verify_flags(tlsFd->credentials, GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);
      err = gnutls_certificate_set_x509_trust_file(tlsFd->credentials, fileinfo.c_str(), GNUTLS_X509_FMT_PEM);
      if (err < 0)
      {
	 _error->Error("Could not load certificates from %s (CaInfo option): %s", fileinfo.c_str(), gnutls_strerror(err));
	 return ResultState::FATAL_ERROR;
      }
   }

   if (not OwnerConf->ConfigFind("IssuerCert", "").empty())
   {
      _error->Error("The option '%s' is not supported anymore", "IssuerCert");
      return ResultState::FATAL_ERROR;
   }
   if (not OwnerConf->ConfigFind("SslForceVersion", "").empty())
   {
      _error->Error("The option '%s' is not supported anymore", "SslForceVersion");
      return ResultState::FATAL_ERROR;
   }

   // For client authentication, certificate file ...
   std::string const cert = OwnerConf->ConfigFind("SslCert", "");
   std::string const key = OwnerConf->ConfigFind("SslKey", "");
   if (cert.empty() == false)
   {
      if ((err = gnutls_certificate_set_x509_key_file(
	       tlsFd->credentials,
	       cert.c_str(),
	       key.empty() ? cert.c_str() : key.c_str(),
	       GNUTLS_X509_FMT_PEM)) < 0)
      {
	 _error->Error("Could not load client certificate (%s, SslCert option) or key (%s, SslKey option): %s", cert.c_str(), key.c_str(), gnutls_strerror(err));
	 return ResultState::FATAL_ERROR;
      }
   }

   // CRL file
   std::string const crlfile = OwnerConf->ConfigFind("CrlFile", "");
   if (crlfile.empty() == false)
   {
      if ((err = gnutls_certificate_set_x509_crl_file(tlsFd->credentials,
						      crlfile.c_str(),
						      GNUTLS_X509_FMT_PEM)) < 0)
      {
	 _error->Error("Could not load custom certificate revocation list %s (CrlFile option): %s", crlfile.c_str(), gnutls_strerror(err));
	 return ResultState::FATAL_ERROR;
      }
   }

   if ((err = gnutls_credentials_set(tlsFd->session, GNUTLS_CRD_CERTIFICATE, tlsFd->credentials)) < 0)
   {
      _error->Error("Internal error: Could not add certificates to session: %s", gnutls_strerror(err));
      return ResultState::FATAL_ERROR;
   }

   if ((err = gnutls_set_default_priority(tlsFd->session)) < 0)
   {
      _error->Error("Internal error: Could not set algorithm preferences: %s", gnutls_strerror(err));
      return ResultState::FATAL_ERROR;
   }

   if (OwnerConf->ConfigFindB("Verify-Peer", true))
   {
      gnutls_session_set_verify_cert(tlsFd->session, OwnerConf->ConfigFindB("Verify-Host", true) ? tlsFd->hostname.c_str() : nullptr, 0);
   }

   // set SNI only if the hostname is really a name and not an address
   {
      struct in_addr addr4;
      struct in6_addr addr6;

      if (inet_pton(AF_INET, tlsFd->hostname.c_str(), &addr4) == 1 ||
	  inet_pton(AF_INET6, tlsFd->hostname.c_str(), &addr6) == 1)
	 /* not a host name */;
      else if ((err = gnutls_server_name_set(tlsFd->session, GNUTLS_NAME_DNS, tlsFd->hostname.c_str(), tlsFd->hostname.length())) < 0)
      {
	 _error->Error("Could not set host name %s to indicate to server: %s", tlsFd->hostname.c_str(), gnutls_strerror(err));
	 return ResultState::FATAL_ERROR;
      }
   }

   // Set the FD now, so closing it works reliably.
   tlsFd->UnderlyingFd = std::move(Fd);
   Fd.reset(tlsFd);

   // Do the handshake.
   err = tlsFd->DoTLSHandshake();

   if (err < 0)
      return ResultState::TRANSIENT_ERROR;

   return ResultState::SUCCESSFUL;
}
									/*}}}*/
