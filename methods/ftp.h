// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: ftp.h,v 1.2 1999/03/15 07:20:41 jgg Exp $
/* ######################################################################

   FTP Aquire Method - This is the FTP aquire method for APT.

   ##################################################################### */
									/*}}}*/
#ifndef APT_FTP_H
#define APT_FTP_H

class FTPConn
{
   char Buffer[1024*10];
   unsigned long Len;
   int ServerFd;
   int DataFd;
   int DataListenFd;
   URI ServerName;
   bool TryPassive;
   bool Debug;
   
   struct sockaddr_in PasvAddr;
   struct sockaddr_in Peer;

   // Private helper functions
   bool ReadLine(string &Text);      
   bool Login();
   bool CreateDataFd();
   bool Finalize();
   
   public:

   bool Comp(URI Other) {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   
   // Raw connection IO
   bool ReadResp(unsigned int &Ret,string &Text);
   bool WriteMsg(unsigned int &Ret,string &Text,const char *Fmt,...);
   
   // Connection control
   bool Open(pkgAcqMethod *Owner);
   void Close();   
   bool GoPasv();
   
   // Query
   bool Size(const char *Path,unsigned long &Size);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long Resume,
	    MD5Summation &MD5,bool &Missing);
   
   FTPConn(URI Srv);
   ~FTPConn();
};

class FtpMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   virtual bool Configuration(string Message);
   
   FTPConn *Server;
   
   static string FailFile;
   static int FailFd;
   static time_t FailTime;
   static void SigTerm(int);
   
   public:
   
   FtpMethod();
};

#endif
