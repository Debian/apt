// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: rfc2553emu.cc,v 1.8 2001/02/20 07:03:18 jgg Exp $
/* ######################################################################

   RFC 2553 Emulation - Provides emulation for RFC 2553 getaddrinfo,
                        freeaddrinfo and getnameinfo

   This is really C code, it just has a .cc extensions to play nicer with
   the rest of APT.
   
   Originally written by Jason Gunthorpe <jgg@debian.org> and placed into
   the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/
#include "rfc2553emu.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

#ifndef HAVE_GETADDRINFO
// getaddrinfo - Resolve a hostname					/*{{{*/
// ---------------------------------------------------------------------
/* */
int getaddrinfo(const char *nodename, const char *servname,
		const struct addrinfo *hints,
		struct addrinfo **res)
{
   struct addrinfo **Result = res;
   hostent *Addr;
   unsigned int Port;
   int Proto;
   const char *End;
   char **CurAddr;
   
   // Try to convert the service as a number
   Port = htons(strtol(servname,(char **)&End,0));
   Proto = SOCK_STREAM;
   
   if (hints != 0 && hints->ai_socktype != 0)
      Proto = hints->ai_socktype;
   
   // Not a number, must be a name.
   if (End != servname + strlen(servname))
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
      
   // Hostname lookup, only if this is not a listening socket
   if (hints != 0 && (hints->ai_flags & AI_PASSIVE) != AI_PASSIVE)
   {
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
      
      CurAddr = Addr->h_addr_list;
   }
   else
      CurAddr = (char **)&End;    // Fake!
   
   // Start constructing the linked list
   *res = 0;
   for (; *CurAddr != 0; CurAddr++)
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
      
      if (hints != 0 && (hints->ai_flags & AI_PASSIVE) != AI_PASSIVE)
	 ((struct sockaddr_in *)(*Result)->ai_addr)->sin_addr = *(in_addr *)(*CurAddr);
      else
      {
         // Already zerod by calloc.
	 break;
      }
      
      Result = &(*Result)->ai_next;
   }
   
   return 0;
}
									/*}}}*/
// freeaddrinfo - Free the result of getaddrinfo			/*{{{*/
// ---------------------------------------------------------------------
/* */
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
									/*}}}*/
#endif // HAVE_GETADDRINFO

#ifndef HAVE_GETNAMEINFO
// getnameinfo - Convert a sockaddr to a string 			/*{{{*/
// ---------------------------------------------------------------------
/* */
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
		char *host, size_t hostlen,
		char *serv, size_t servlen,
		int flags)
{
   struct sockaddr_in *sin = (struct sockaddr_in *)sa;
   
   // This routine only supports internet addresses
   if (sa->sa_family != AF_INET)
      return EAI_ADDRFAMILY;
   
   if (host != 0)
   {
      // Try to resolve the hostname
      if ((flags & NI_NUMERICHOST) != NI_NUMERICHOST)
      {
	 struct hostent *Ent = gethostbyaddr((char *)&sin->sin_addr,sizeof(sin->sin_addr),
					     AF_INET);
	 if (Ent != 0)
	    strncpy(host,Ent->h_name,hostlen);
	 else
	 {
	    if ((flags & NI_NAMEREQD) == NI_NAMEREQD)
	    {
	       if (h_errno == TRY_AGAIN)
		  return EAI_AGAIN;
	       if (h_errno == NO_RECOVERY)
		  return EAI_FAIL;
	       return EAI_NONAME;
	    }

	    flags |= NI_NUMERICHOST;
	 }
      }
      
      // Resolve as a plain numberic
      if ((flags & NI_NUMERICHOST) == NI_NUMERICHOST)
      {
	 strncpy(host,inet_ntoa(sin->sin_addr),hostlen);
      }
   }
   
   if (serv != 0)
   {
      // Try to resolve the hostname
      if ((flags & NI_NUMERICSERV) != NI_NUMERICSERV)
      {
	 struct servent *Ent;
	 if ((flags & NI_DATAGRAM) == NI_DATAGRAM)
	    Ent = getservbyport(ntohs(sin->sin_port),"udp");
	 else
	    Ent = getservbyport(ntohs(sin->sin_port),"tcp");
	 
	 if (Ent != 0)
	    strncpy(serv,Ent->s_name,servlen);
	 else
	 {
	    if ((flags & NI_NAMEREQD) == NI_NAMEREQD)
	       return EAI_NONAME;

	    flags |= NI_NUMERICSERV;
	 }
      }
      
      // Resolve as a plain numberic
      if ((flags & NI_NUMERICSERV) == NI_NUMERICSERV)
      {
	 snprintf(serv,servlen,"%u",ntohs(sin->sin_port));
      }
   }
   
   return 0;
}
									/*}}}*/
#endif // HAVE_GETNAMEINFO
