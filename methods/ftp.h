// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: ftp.h,v 1.4 2001/03/06 07:15:29 jgg Exp $
// $Id: ftp.h,v 1.4 2001/03/06 07:15:29 jgg Exp $
/* ######################################################################

   FTP Acquire Method - This is the FTP acquire method for APT.

   ##################################################################### */
									/*}}}*/
#ifndef APT_FTP_H
#define APT_FTP_H

#include <apt-pkg/strutl.h>
#include "aptmethod.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <string>

class FTPConn
{
   char Buffer[1024*10];
   unsigned long Len;
   int ServerFd;
   int DataFd;
   int DataListenFd;
   URI ServerName;
   bool ForceExtended;
   bool TryPassive;
   bool Debug;

   struct addrinfo *PasvAddr;
   
   // Generic Peer Address
   struct sockaddr_storage PeerAddr;
   socklen_t PeerAddrLen;
   
   // Generic Server Address (us)
   struct sockaddr_storage ServerAddr;
   socklen_t ServerAddrLen;
   
   // Private helper functions
   bool ReadLine(std::string &Text);
   bool Login();
   bool CreateDataFd();
   bool Finalize();
   
   public:

   bool Comp(URI Other) {return Other.Host == ServerName.Host && Other.Port == ServerName.Port && Other.User == ServerName.User && Other.Password == ServerName.Password; };
   
   // Raw connection IO
   bool ReadResp(unsigned int &Ret,std::string &Text);
   bool WriteMsg(unsigned int &Ret,std::string &Text,const char *Fmt,...);
   
   // Connection control
   bool Open(pkgAcqMethod *Owner);
   void Close();   
   bool GoPasv();
   bool ExtGoPasv();
   
   // Query
   bool Size(const char *Path,unsigned long long &Size);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long long Resume,
	    Hashes &MD5,bool &Missing, unsigned long long MaximumSize,
            pkgAcqMethod *Owner);
   
   explicit FTPConn(URI Srv);
   ~FTPConn();
};

class FtpMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;
   virtual bool Configuration(std::string Message) APT_OVERRIDE;
   
   FTPConn *Server;
   
   static std::string FailFile;
   static int FailFd;
   static time_t FailTime;
   static APT_NORETURN void SigTerm(int);
   
   public:
   
   FtpMethod();
};

#endif
