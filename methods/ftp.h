// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: ftp.h,v 1.1 1999/03/15 06:01:00 jgg Exp $
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
   
   struct sockaddr_in PasvAddr;
   struct sockaddr_in Peer;

   // Private helper functions
   bool ReadLine(string &Text);      
   bool Login();
   bool CreateDataFd();
   bool Finalize();
   
   public:

   // Raw connection IO
   bool ReadResp(unsigned int &Ret,string &Text);
   bool WriteMsg(unsigned int &Ret,string &Text,const char *Fmt,...);
   
   // Connection control
   bool Open();
   void Close();   
   bool GoPasv();
   
   // Query
   unsigned long Size(const char *Path);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long Resume = 0);
   
   FTPConn(URI Srv);
   ~FTPConn();
};

#endif
