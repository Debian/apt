// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.cc,v 1.10.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Connect - Replacement connect call

   This was originally authored by Jason Gunthorpe <jgg@debian.org>
   and is placed in the Public Domain, do with it what you will.
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "connect.h"
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include<set>
#include<string>

// Internet stuff
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "rfc2553emu.h"
#include <apti18n.h>
									/*}}}*/

static string LastHost;
static int LastPort = 0;
static struct addrinfo *LastHostAddr = 0;
static struct addrinfo *LastUsed = 0;

// Set of IP/hostnames that we timed out before or couldn't resolve
static std::set<string> bad_addr;

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
// DoConnect - Attempt a connect operation				/*{{{*/
// ---------------------------------------------------------------------
/* This helper function attempts a connection to a single address. */
static bool DoConnect(struct addrinfo *Addr,string Host,
		      unsigned long TimeOut,int &Fd,pkgAcqMethod *Owner)
{
   // Show a status indicator
   char Name[NI_MAXHOST];
   char Service[NI_MAXSERV];
   
   Name[0] = 0;   
   Service[0] = 0;
   getnameinfo(Addr->ai_addr,Addr->ai_addrlen,
	       Name,sizeof(Name),Service,sizeof(Service),
	       NI_NUMERICHOST|NI_NUMERICSERV);
   Owner->Status(_("Connecting to %s (%s)"),Host.c_str(),Name);

   // if that addr did timeout before, we do not try it again
   if(bad_addr.find(string(Name)) != bad_addr.end()) 
      return false;

   /* If this is an IP rotation store the IP we are using.. If something goes
      wrong this will get tacked onto the end of the error message */
   if (LastHostAddr->ai_next != 0)
   {
      char Name2[NI_MAXHOST + NI_MAXSERV + 10];
      snprintf(Name2,sizeof(Name2),_("[IP: %s %s]"),Name,Service);
      Owner->SetFailExtraMsg(string(Name2));
   }   
   else
      Owner->SetFailExtraMsg("");
      
   // Get a socket
   if ((Fd = socket(Addr->ai_family,Addr->ai_socktype,
		    Addr->ai_protocol)) < 0)
      return _error->Errno("socket",_("Could not create a socket for %s (f=%u t=%u p=%u)"),
			   Name,Addr->ai_family,Addr->ai_socktype,Addr->ai_protocol);
   
   SetNonBlock(Fd,true);
   if (connect(Fd,Addr->ai_addr,Addr->ai_addrlen) < 0 &&
       errno != EINPROGRESS)
      return _error->Errno("connect",_("Cannot initiate the connection "
			   "to %s:%s (%s)."),Host.c_str(),Service,Name);
   
   /* This implements a timeout for connect by opening the connection
      nonblocking */
   if (WaitFd(Fd,true,TimeOut) == false) {
      bad_addr.insert(bad_addr.begin(), string(Name));
      Owner->SetFailExtraMsg("\nFailReason: Timeout");
      return _error->Error(_("Could not connect to %s:%s (%s), "
			   "connection timed out"),Host.c_str(),Service,Name);
   }

   // Check the socket for an error condition
   unsigned int Err;
   unsigned int Len = sizeof(Err);
   if (getsockopt(Fd,SOL_SOCKET,SO_ERROR,&Err,&Len) != 0)
      return _error->Errno("getsockopt",_("Failed"));
   
   if (Err != 0)
   {
      errno = Err;
      if(errno == ECONNREFUSED)
         Owner->SetFailExtraMsg("\nFailReason: ConnectionRefused");
      return _error->Errno("connect",_("Could not connect to %s:%s (%s)."),Host.c_str(),
			   Service,Name);
   }
   
   return true;
}
									/*}}}*/
// Connect - Connect to a server					/*{{{*/
// ---------------------------------------------------------------------
/* Performs a connection to the server */
bool Connect(string Host,int Port,const char *Service,int DefPort,int &Fd,
	     unsigned long TimeOut,pkgAcqMethod *Owner)
{
   if (_error->PendingError() == true)
      return false;

   // Convert the port name/number
   char ServStr[300];
   if (Port != 0)
      snprintf(ServStr,sizeof(ServStr),"%u",Port);
   else
      snprintf(ServStr,sizeof(ServStr),"%s",Service);
   
   /* We used a cached address record.. Yes this is against the spec but
      the way we have setup our rotating dns suggests that this is more
      sensible */
   if (LastHost != Host || LastPort != Port)
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
      Hints.ai_protocol = 0;
      
      // if we couldn't resolve the host before, we don't try now
      if(bad_addr.find(Host) != bad_addr.end()) 
	 return _error->Error(_("Could not resolve '%s'"),Host.c_str());

      // Resolve both the host and service simultaneously
      while (1)
      {
	 int Res;
	 if ((Res = getaddrinfo(Host.c_str(),ServStr,&Hints,&LastHostAddr)) != 0 ||
	     LastHostAddr == 0)
	 {
	    if (Res == EAI_NONAME || Res == EAI_SERVICE)
	    {
	       if (DefPort != 0)
	       {
		  snprintf(ServStr,sizeof(ServStr),"%u",DefPort);
		  DefPort = 0;
		  continue;
	       }
	       bad_addr.insert(bad_addr.begin(), Host);
	       Owner->SetFailExtraMsg("\nFailReason: ResolveFailure");
	       return _error->Error(_("Could not resolve '%s'"),Host.c_str());
	    }
	    
	    if (Res == EAI_AGAIN)
	    {
	       Owner->SetFailExtraMsg("\nFailReason: TmpResolveFailure");
	       return _error->Error(_("Temporary failure resolving '%s'"),
				    Host.c_str());
	    }
	    return _error->Error(_("Something wicked happened resolving '%s:%s' (%i)"),
				 Host.c_str(),ServStr,Res);
	 }
	 break;
      }
      
      LastHost = Host;
      LastPort = Port;
   }

   // When we have an IP rotation stay with the last IP.
   struct addrinfo *CurHost = LastHostAddr;
   if (LastUsed != 0)
       CurHost = LastUsed;
   
   while (CurHost != 0)
   {
      if (DoConnect(CurHost,Host,TimeOut,Fd,Owner) == true)
      {
	 LastUsed = CurHost;
	 return true;
      }      
      close(Fd);
      Fd = -1;
      
      // Ignore UNIX domain sockets
      do
      {
	 CurHost = CurHost->ai_next;
      }
      while (CurHost != 0 && CurHost->ai_family == AF_UNIX);

      /* If we reached the end of the search list then wrap around to the
         start */
      if (CurHost == 0 && LastUsed != 0)
	 CurHost = LastHostAddr;
      
      // Reached the end of the search cycle
      if (CurHost == LastUsed)
	 break;
      
      if (CurHost != 0)
	 _error->Discard();
   }   

   if (_error->PendingError() == true)
      return false;   
   return _error->Error(_("Unable to connect to %s %s:"),Host.c_str(),ServStr);
}
									/*}}}*/
