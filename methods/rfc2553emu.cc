// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: rfc2553emu.cc,v 1.1 1999/05/25 05:56:24 jgg Exp $
/* ######################################################################

   RFC 2553 Emulation - Provides emulation for RFC 2553 getaddrinfo,
                        freeaddrinfo and getnameinfo
   
   Originally written by Jason Gunthorpe <jgg@debian.org> and placed into
   the Public Domain, do with it what you will.
   
   ##################################################################### */
									/*}}}*/
#include "rfc2553emu.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <iostream.h>

#ifndef HAVE_GETADDRINFO
int getaddrinfo(const char *nodename, const char *servname,
		const struct addrinfo *hints,
		struct addrinfo **res)
{
   struct addrinfo **Result;
   hostent *Addr;
   unsigned int Port;
   int Proto;
   const char *End;
   char **CurAddr;
   
   Addr = gethostbyname(nodename);
   if (Addr == 0)
   {
      if (h_errno == TRY_AGAIN)
	 return EAI_AGAIN;
      if (h_errno == NO_RECOVERY)
	 return EAI_FAIL;
      return EAI_NONAME;
   }
   
   // No A records 
   if (Addr->h_addr_list[0] == 0)
      return EAI_NONAME;

   // Try to convert the service as a number
   Port = htons(strtol(servname,(char **)&End,0));
   Proto = SOCK_STREAM;
   
   if (hints != 0 && hints->ai_socktype != 0)
      Proto = hints->ai_socktype;
   
   // Not a number, must be a name.
   if (End != servname + strlen(End))
   {
      struct servent *Srv = 0;
      
      // Do a lookup in the service database
      if (hints == 0 || hints->ai_socktype == SOCK_STREAM)
	 Srv = getservbyname(servname,"tcp");
      if (hints != 0 && hints->ai_socktype == SOCK_DGRAM)
	 Srv = getservbyname(servname,"udp");
      if (Srv == 0)
	 return EAI_NONAME;  
      
      // Get the right protocol
      Port = Srv->s_port;
      if (strcmp(Srv->s_proto,"tcp") == 0)
	 Proto = SOCK_STREAM;
      else
      {
	 if (strcmp(Srv->s_proto,"udp") == 0)
	    Proto = SOCK_DGRAM;
         else
	    return EAI_NONAME;
      }      
      
      if (hints != 0 && hints->ai_socktype != Proto && 
	  hints->ai_socktype != 0)
	 return EAI_SERVICE;
   }
   
   // Start constructing the linked list
   *res = 0;
   for (CurAddr = Addr->h_addr_list; *CurAddr != 0; CurAddr++)
   {
      // New result structure
      *Result = (struct addrinfo *)calloc(sizeof(**Result),1);
      if (*Result == 0)
      {
	 freeaddrinfo(*res);
	 return EAI_MEMORY;
      }
      if (*res == 0)
	 *res = *Result;
      
      (*Result)->ai_family = AF_INET;
      (*Result)->ai_socktype = Proto;

      // If we have the IPPROTO defines we can set the protocol field
      #ifdef IPPROTO_TCP
      if (Proto == SOCK_STREAM)
	 (*Result)->ai_protocol = IPPROTO_TCP;
      if (Proto == SOCK_DGRAM)
	 (*Result)->ai_protocol = IPPROTO_UDP;
      #endif

      // Allocate space for the address
      (*Result)->ai_addrlen = sizeof(struct sockaddr_in);
      (*Result)->ai_addr = (struct sockaddr *)calloc(sizeof(sockaddr_in),1);
      if ((*Result)->ai_addr == 0)
      {
	 freeaddrinfo(*res);
	 return EAI_MEMORY;
      }
      
      // Set the address
      ((struct sockaddr_in *)(*Result)->ai_addr)->sin_family = AF_INET;
      ((struct sockaddr_in *)(*Result)->ai_addr)->sin_port = Port;
      ((struct sockaddr_in *)(*Result)->ai_addr)->sin_addr = *(in_addr *)(*CurAddr);

      Result = &(*Result)->ai_next;
   }
   
   return 0;
}

void freeaddrinfo(struct addrinfo *ai)
{
   struct addrinfo *Tmp;
   while (ai != 0)
   {
      free(ai->ai_addr);
      Tmp = ai;
      ai = ai->ai_next;
      free(ai);
   }
}

#endif // HAVE_GETADDRINFO
