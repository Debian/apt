// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: rsh.h,v 1.4 2002/11/09 23:33:26 doogie Exp $
// $Id: rsh.h,v 1.4 2002/11/09 23:33:26 doogie Exp $
/* ######################################################################

   RSH method - Transfer files via rsh compatible program

   ##################################################################### */
									/*}}}*/
#ifndef APT_RSH_H
#define APT_RSH_H

#include <string>
#include <time.h>

#include <apt-pkg/strutl.h>

class Hashes;
class FileFd;

class RSHConn
{
   char Buffer[1024*10];
   unsigned long Len;
   int WriteFd;
   int ReadFd;
   URI ServerName;
   std::string const Prog;

   // Private helper functions
   bool ReadLine(std::string &Text);

   public:

   pid_t Process;

   // Raw connection IO
   bool WriteMsg(std::string &Text,bool Sync,const char *Fmt,...);
   bool Connect(std::string Host, std::string User);
   bool Connect(std::string Host, unsigned int Port, std::string User);
   bool Comp(URI Other) const {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};

   // Connection control
   bool Open();
   void Close();

   // Query
   bool Size(const char *Path,unsigned long long &Size);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long long Resume,
            Hashes &Hash,bool &Missing, unsigned long long Size);

   RSHConn(std::string const &Prog, URI Srv);
   ~RSHConn();
};

#include "aptmethod.h"

class RSHMethod : public aptMethod
{
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;
   virtual bool Configuration(std::string Message) APT_OVERRIDE;

   RSHConn *Server;

   static std::string FailFile;
   static int FailFd;
   static time_t FailTime;
   static APT_NORETURN void SigTerm(int);

   public:

   explicit RSHMethod(std::string &&Prog);
};

#endif
