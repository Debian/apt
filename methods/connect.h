// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Connect - Replacement connect call
   
   ##################################################################### */
									/*}}}*/
#ifndef CONNECT_H
#define CONNECT_H

#include <memory>
#include <string>
#include <stddef.h>

#include "aptmethod.h"

/**
 * \brief Small representation of a file descriptor for network traffic.
 *
 * This provides support for TLS, SOCKS, and HTTP CONNECT proxies.
 */
struct MethodFd
{
   /// \brief Returns -1 for unusable, or an fd to select() on otherwise
   virtual int Fd() = 0;
   /// \brief Should behave like read(2)
   virtual ssize_t Read(void *buf, size_t count) = 0;
   /// \brief Should behave like write(2)
   virtual ssize_t Write(void *buf, size_t count) = 0;
   /// \brief Closes the file descriptor. Can be called multiple times.
   virtual int Close() = 0;
   /// \brief Destructor
   virtual ~MethodFd(){};
   /// \brief Construct a MethodFd from a UNIX file descriptor
   static std::unique_ptr<MethodFd> FromFd(int iFd);
   /// \brief If there is pending data.
   virtual bool HasPending();
};

ResultState Connect(std::string To, int Port, const char *Service, int DefPort,
		    std::unique_ptr<MethodFd> &Fd, unsigned long TimeOut, aptMethod *Owner);

ResultState UnwrapSocks(std::string To, int Port, URI Proxy, std::unique_ptr<MethodFd> &Fd, unsigned long Timeout, aptMethod *Owner);
ResultState UnwrapTLS(std::string const &To, std::unique_ptr<MethodFd> &Fd, unsigned long Timeout, aptMethod *Owner,
		      aptConfigWrapperForMethods const * OwnerConf);

void RotateDNS();

#endif
