// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.cc,v 1.6 2000/05/28 04:34:44 jgg Exp $
/* ######################################################################

   Connect - Replacement connect call
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "connect.h"
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

// Internet stuff
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "rfc2553emu.h"
									/*}}}*/

static string LastHost;
static int LastPort = 0;
static struct addrinfo *LastHostAddr = 0;
static struct addrinfo *LastUsed = 0;

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
   Owner->Status("Connecting to %s (%s)",Host.c_str(),Name);
   
   // Get a socket
   if ((Fd = socket(Addr->ai_family,Addr->ai_socktype,
		    Addr->ai_protocol)) < 0)
      return _error->Errno("socket","Could not create a socket");
   
   SetNonBlock(Fd,true);
   if (connect(Fd,Addr->ai_addr,Addr->ai_addrlen) < 0 &&
       errno != EINPROGRESS)
      return _error->Errno("connect","Cannot initiate the connection "
			   "to %s:%s (%s).",Host.c_str(),Service,Name);
   
   /* This implements a timeout for connect by opening the connection
      nonblocking */
   if (WaitFd(Fd,true,TimeOut) == false)
      return _error->Error("Could not connect to %s:%s (%s), "
			   "connection timed out",Host.c_str(),Service,Name);
   
   // Check the socket for an error condition
   unsigned int Err;
   unsigned int Len = sizeof(Err);
   if (getsockopt(Fd,SOL_SOCKET,SO_ERROR,&Err,&Len) != 0)
      return _error->Errno("getsockopt","Failed");
   
   if (Err != 0)
   {
      errno = Err;
      return _error->Errno("connect","Could not connect to %s:%s (%s).",Host.c_str(),
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
      Owner->Status("Connecting to %s",Host.c_str());

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
	       return _error->Error("Could not resolve '%s'",Host.c_str());
	    }
	    
	    return _error->Error("Something wicked happend resolving '%s:%s'",
				 Host.c_str(),ServStr);
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
	 
      LastUsed = 0;
      if (CurHost != 0)
	 _error->Discard();
   }

   if (_error->PendingError() == true)
      return false;
   return _error->Error("Unable to connect to %s:",Host.c_str(),ServStr);
}
									/*}}}*/
