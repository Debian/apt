// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.cc,v 1.1 1999/05/29 03:25:03 jgg Exp $
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
   Name[0] = 0;
   getnameinfo(Addr->ai_addr,Addr->ai_addrlen,
	       Name,sizeof(Name),0,0,NI_NUMERICHOST);
   Owner->Status("Connecting to %s (%s)",Host.c_str(),Name);
   
   // Get a socket
   if ((Fd = socket(Addr->ai_family,Addr->ai_socktype,
		    Addr->ai_protocol)) < 0)
      return _error->Errno("socket","Could not create a socket");
   
   SetNonBlock(Fd,true);
   if (connect(Fd,Addr->ai_addr,Addr->ai_addrlen) < 0 &&
       errno != EINPROGRESS)
      return _error->Errno("connect","Cannot initiate the connection "
			   "to %s (%s).",Host.c_str(),Name);
   
   /* This implements a timeout for connect by opening the connection
      nonblocking */
   if (WaitFd(Fd,true,TimeOut) == false)
      return _error->Error("Could not connect to %s (%s), "
			   "connection timed out",Host.c_str(),Name);
   
   // Check the socket for an error condition
   unsigned int Err;
   unsigned int Len = sizeof(Err);
   if (getsockopt(Fd,SOL_SOCKET,SO_ERROR,&Err,&Len) != 0)
      return _error->Errno("getsockopt","Failed");
   
   if (Err != 0)
      return _error->Error("Could not connect to %s (%s).",Host.c_str(),Name);

   return true;
}
									/*}}}*/
// Connect - Connect to a server						/*{{{*/
// ---------------------------------------------------------------------
/* Performs a connection to the server */
bool Connect(string Host,int Port,const char *Service,int &Fd,
	     unsigned long TimeOut,pkgAcqMethod *Owner)
{
   if (_error->PendingError() == true)
      return false;
   
   /* We used a cached address record.. Yes this is against the spec but
      the way we have setup our rotating dns suggests that this is more
      sensible */
   if (LastHost != Host || LastPort != Port)
   {
      Owner->Status("Connecting to %s",Host.c_str());

      // Lookup the host
      char S[300];
      if (Port != 0)
	 snprintf(S,sizeof(S),"%u",Port);
      else
	 snprintf(S,sizeof(S),"%s",Service);

      // Free the old address structure
      if (LastHostAddr != 0)
      {
	 freeaddrinfo(LastHostAddr);
	 LastHostAddr = 0;
      }
      
      // We only understand SOCK_STREAM sockets.
      struct addrinfo Hints;
      memset(&Hints,0,sizeof(Hints));
      Hints.ai_socktype = SOCK_STREAM;
      
      // Resolve both the host and service simultaneously
      if (getaddrinfo(Host.c_str(),S,&Hints,&LastHostAddr) != 0 ||
	  LastHostAddr == 0)
	 return _error->Error("Could not resolve '%s'",Host.c_str());

      LastHost = Host;
      LastPort = Port;
      LastUsed = 0;
   }

   // Get the printable IP address
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
      
      CurHost = CurHost->ai_next;
      LastUsed = 0;
      if (CurHost != 0)
	 _error->Discard();
   }
   
   return false;
}
									/*}}}*/
