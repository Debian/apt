// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: ftp.cc,v 1.17 1999/12/09 03:45:56 jgg Exp $
/* ######################################################################

   HTTP Aquire Method - This is the FTP aquire method for APT.

   This is a very simple implementation that does not try to optimize
   at all. Commands are sent syncronously with the FTP server (as the
   rfc recommends, but it is not really necessary..) and no tricks are
   done to speed things along.
			
   RFC 2428 describes the IPv6 FTP behavior
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/md5.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

// Internet stuff
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "rfc2553emu.h"
#include "connect.h"
#include "ftp.h"
									/*}}}*/

unsigned long TimeOut = 120;
URI Proxy;
string FtpMethod::FailFile;
int FtpMethod::FailFd = -1;
time_t FtpMethod::FailTime = 0;

// FTPConn::FTPConn - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
FTPConn::FTPConn(URI Srv) : Len(0), ServerFd(-1), DataFd(-1), 
                            DataListenFd(-1), ServerName(Srv)
{
   Debug = _config->FindB("Debug::Acquire::Ftp",false);
   memset(&PasvAddr,0,sizeof(PasvAddr));
}
									/*}}}*/
// FTPConn::~FTPConn - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
FTPConn::~FTPConn()
{
   Close();
}
									/*}}}*/
// FTPConn::Close - Close down the connection				/*{{{*/
// ---------------------------------------------------------------------
/* Just tear down the socket and data socket */
void FTPConn::Close()
{
   close(ServerFd);
   ServerFd = -1;
   close(DataFd);
   DataFd = -1;
   close(DataListenFd);
   DataListenFd = -1;
   memset(&PasvAddr,0,sizeof(PasvAddr));
}
									/*}}}*/
// FTPConn::Open - Open a new connection				/*{{{*/
// ---------------------------------------------------------------------
/* Connect to the server using a non-blocking connection and perform a 
   login. */
bool FTPConn::Open(pkgAcqMethod *Owner)
{
   // Use the already open connection if possible.
   if (ServerFd != -1)
      return true;
   
   Close();

   // Determine the proxy setting
   if (getenv("ftp_proxy") == 0)
   {
      string DefProxy = _config->Find("Acquire::ftp::Proxy");
      string SpecificProxy = _config->Find("Acquire::ftp::Proxy::" + ServerName.Host);
      if (SpecificProxy.empty() == false)
      {
	 if (SpecificProxy == "DIRECT")
	    Proxy = "";
	 else
	    Proxy = SpecificProxy;
      }   
      else
	 Proxy = DefProxy;
   }
   else
      Proxy = getenv("ftp_proxy");
   
   // Determine what host and port to use based on the proxy settings
   int Port = 0;
   string Host;   
   if (Proxy.empty() == true)
   {
      if (ServerName.Port != 0)
	 Port = ServerName.Port;
      Host = ServerName.Host;
   }
   else
   {
      if (Proxy.Port != 0)
	 Port = Proxy.Port;
      Host = Proxy.Host;
   }

   // Connect to the remote server
   if (Connect(Host,Port,"ftp",21,ServerFd,TimeOut,Owner) == false)
      return false;
   socklen_t Len = sizeof(Peer);
   if (getpeername(ServerFd,(sockaddr *)&Peer,&Len) != 0)
      return _error->Errno("getpeername","Unable to determine the peer name");
   
   Owner->Status("Logging in");
   return Login();
}
									/*}}}*/
// FTPConn::Login - Login to the remote server				/*{{{*/
// ---------------------------------------------------------------------
/* This performs both normal login and proxy login using a simples script
   stored in the config file. */
bool FTPConn::Login()
{
   unsigned int Tag;
   string Msg;
   
   // Setup the variables needed for authentication
   string User = "anonymous";
   string Pass = "apt_get_ftp_2.0@debian.linux.user";

   // Fill in the user/pass
   if (ServerName.User.empty() == false)
      User = ServerName.User;
   if (ServerName.Password.empty() == false)
      Pass = ServerName.Password;
       
   // Perform simple login
   if (Proxy.empty() == true)
   {
      // Read the initial response
      if (ReadResp(Tag,Msg) == false)
	 return false;
      if (Tag >= 400)
	 return _error->Error("Server refused our connection and said: %s",Msg.c_str());
      
      // Send the user
      if (WriteMsg(Tag,Msg,"USER %s",User.c_str()) == false)
	 return false;
      if (Tag >= 400)
	 return _error->Error("USER failed, server said: %s",Msg.c_str());
      
      // Send the Password
      if (WriteMsg(Tag,Msg,"PASS %s",Pass.c_str()) == false)
	 return false;
      if (Tag >= 400)
	 return _error->Error("PASS failed, server said: %s",Msg.c_str());
      
      // Enter passive mode
      if (_config->Exists("Acquire::FTP::Passive::" + ServerName.Host) == true)
	 TryPassive = _config->FindB("Acquire::FTP::Passive::" + ServerName.Host,true);
      else
	 TryPassive = _config->FindB("Acquire::FTP::Passive",true);
   }
   else
   {      
      // Read the initial response
      if (ReadResp(Tag,Msg) == false)
	 return false;
      if (Tag >= 400)
	 return _error->Error("Server refused our connection and said: %s",Msg.c_str());
      
      // Perform proxy script execution
      Configuration::Item const *Opts = _config->Tree("Acquire::ftp::ProxyLogin");
      if (Opts == 0 || Opts->Child == 0)
	 return _error->Error("A proxy server was specified but no login "
			      "script, Acquire::ftp::ProxyLogin is empty.");
      Opts = Opts->Child;

      // Iterate over the entire login script
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 
	 // Substitute the variables into the command
	 char SitePort[20];
	 if (ServerName.Port != 0)
	    sprintf(SitePort,"%u",ServerName.Port);
	 else
	    strcpy(SitePort,"21");
	 string Tmp = Opts->Value;
	 Tmp = SubstVar(Tmp,"$(PROXY_USER)",Proxy.User);
	 Tmp = SubstVar(Tmp,"$(PROXY_PASS)",Proxy.Password);
	 Tmp = SubstVar(Tmp,"$(SITE_USER)",User);
	 Tmp = SubstVar(Tmp,"$(SITE_PASS)",Pass);
	 Tmp = SubstVar(Tmp,"$(SITE_PORT)",SitePort);
	 Tmp = SubstVar(Tmp,"$(SITE)",ServerName.Host);

	 // Send the command
	 if (WriteMsg(Tag,Msg,"%s",Tmp.c_str()) == false)
	    return false;
	 if (Tag >= 400)
	    return _error->Error("Login script command '%s' failed, server said: %s",Tmp.c_str(),Msg.c_str());	 
      }
      
      // Enter passive mode
      TryPassive = false;
      if (_config->Exists("Acquire::FTP::Passive::" + ServerName.Host) == true)
	 TryPassive = _config->FindB("Acquire::FTP::Passive::" + ServerName.Host,true);
      else
      {
	 if (_config->Exists("Acquire::FTP::Proxy::Passive") == true)
	    TryPassive = _config->FindB("Acquire::FTP::Proxy::Passive",true);
	 else
	    TryPassive = _config->FindB("Acquire::FTP::Passive",true);
      }            
   }

   // Binary mode
   if (WriteMsg(Tag,Msg,"TYPE I") == false)
      return false;
   if (Tag >= 400)
      return _error->Error("TYPE failed, server said: %s",Msg.c_str());
   
   return true;
}
									/*}}}*/
// FTPConn::ReadLine - Read a line from the server			/*{{{*/
// ---------------------------------------------------------------------
/* This performs a very simple buffered read. */
bool FTPConn::ReadLine(string &Text)
{
   if (ServerFd == -1)
      return false;
   
   // Suck in a line
   while (Len < sizeof(Buffer))
   {
      // Scan the buffer for a new line
      for (unsigned int I = 0; I != Len; I++)
      {
	 // Escape some special chars
	 if (Buffer[I] == 0)
	    Buffer[I] = '?';
	 
	 // End of line?
	 if (Buffer[I] != '\n')
	    continue;
	 
	 I++;
	 Text = string(Buffer,I);
	 memmove(Buffer,Buffer+I,Len - I);
	 Len -= I;	 
	 return true;
      }

      // Wait for some data..
      if (WaitFd(ServerFd,false,TimeOut) == false)
      {
	 Close();
	 return _error->Error("Connection timeout");
      }
      
      // Suck it back
      int Res = read(ServerFd,Buffer + Len,sizeof(Buffer) - Len);
      if (Res <= 0)
      {
	 _error->Errno("read","Read error");
	 Close();
	 return false;
      }      
      Len += Res;
   }

   return _error->Error("A response overflowed the buffer.");
}
									/*}}}*/
// FTPConn::ReadResp - Read a full response from the server		/*{{{*/
// ---------------------------------------------------------------------
/* This reads a reply code from the server, it handles both p */
bool FTPConn::ReadResp(unsigned int &Ret,string &Text)
{
   // Grab the first line of the response
   string Msg;
   if (ReadLine(Msg) == false)
       return false;
   
   // Get the ID code
   char *End;   
   Ret = strtol(Msg.c_str(),&End,10);
   if (End - Msg.c_str() != 3)
      return _error->Error("Protocol corruption");

   // All done ?
   Text = Msg.c_str()+4;
   if (*End == ' ')
   {
      if (Debug == true)
	 cerr << "<- '" << QuoteString(Text,"") << "'" << endl;
      return true;
   }
   
   if (*End != '-')
      return _error->Error("Protocol corruption");
   
   /* Okay, here we do the continued message trick. This is foolish, but
      proftpd follows the protocol as specified and wu-ftpd doesn't, so 
      we filter. I wonder how many clients break if you use proftpd and
      put a '- in the 3rd spot in the message? */
   char Leader[4];
   strncpy(Leader,Msg.c_str(),3);
   Leader[3] = 0;
   while (ReadLine(Msg) == true)
   {
      // Short, it must be using RFC continuation..
      if (Msg.length() < 4)
      {
	 Text += Msg;
	 continue;
      }
      
      // Oops, finished
      if (strncmp(Msg.c_str(),Leader,3) == 0 && Msg[3] == ' ')
      {
	 Text += Msg.c_str()+4;
	 break;
      }
      
      // This message has the wu-ftpd style reply code prefixed
      if (strncmp(Msg.c_str(),Leader,3) == 0 && Msg[3] == '-')
      {
	 Text += Msg.c_str()+4;
	 continue;
      }
      
      // Must be RFC style prefixing
      Text += Msg;
   }	   

   if (Debug == true && _error->PendingError() == false)
      cerr << "<- '" << QuoteString(Text,"") << "'" << endl;
      
   return !_error->PendingError();
}
									/*}}}*/
// FTPConn::WriteMsg - Send a message to the server			/*{{{*/
// ---------------------------------------------------------------------
/* Simple printf like function.. */
bool FTPConn::WriteMsg(unsigned int &Ret,string &Text,const char *Fmt,...)
{
   va_list args;
   va_start(args,Fmt);

   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S) - 4,Fmt,args);
   strcat(S,"\r\n");
 
   if (Debug == true)
      cerr << "-> '" << QuoteString(S,"") << "'" << endl;

   // Send it off
   unsigned long Len = strlen(S);
   unsigned long Start = 0;
   while (Len != 0)
   {
      if (WaitFd(ServerFd,true,TimeOut) == false)
      {
	 Close();
	 return _error->Error("Connection timeout");
      }
      
      int Res = write(ServerFd,S + Start,Len);
      if (Res <= 0)
      {
	 _error->Errno("write","Write Error");
	 Close();
	 return false;
      }
      
      Len -= Res;
      Start += Res;
   }
   
   return ReadResp(Ret,Text);
}
									/*}}}*/
// FTPConn::GoPasv - Enter Passive mode					/*{{{*/
// ---------------------------------------------------------------------
/* Try to enter passive mode, the return code does not indicate if passive
   mode could or could not be established, only if there was a fatal error. 
   Borrowed mostly from lftp. We have to enter passive mode every time 
 we make a data connection :| */
bool FTPConn::GoPasv()
{
   // Try to enable pasv mode
   unsigned int Tag;
   string Msg;
   if (WriteMsg(Tag,Msg,"PASV") == false)
      return false;
   
   // Unsupported function
   string::size_type Pos = Msg.find('(');
   if (Tag >= 400 || Pos == string::npos)
   {
      memset(&PasvAddr,0,sizeof(PasvAddr));
      return true;
   }

   // Scan it
   unsigned a0,a1,a2,a3,p0,p1;
   if (sscanf(Msg.c_str() + Pos,"(%u,%u,%u,%u,%u,%u)",&a0,&a1,&a2,&a3,&p0,&p1) != 6)
   {
      memset(&PasvAddr,0,sizeof(PasvAddr));
      return true;
   }
   
   // lftp used this horrid byte order manipulation.. Ik.
   PasvAddr.sin_family = AF_INET;
   unsigned char *a;
   unsigned char *p;
   a = (unsigned char *)&PasvAddr.sin_addr;
   p = (unsigned char *)&PasvAddr.sin_port;
   
   // Some evil servers return 0 to mean their addr
   if (a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0)
   {
      PasvAddr.sin_addr = Peer.sin_addr;
   }
   else
   {
      a[0] = a0; 
      a[1] = a1; 
      a[2] = a2; 
      a[3] = a3;
   }
   
   p[0] = p0;
   p[1] = p1;
   
   return true;
}
									/*}}}*/
// FTPConn::Size - Return the size of a file				/*{{{*/
// ---------------------------------------------------------------------
/* Grab the file size from the server, 0 means no size or empty file */
bool FTPConn::Size(const char *Path,unsigned long &Size)
{
   // Query the size
   unsigned int Tag;
   string Msg;
   Size = 0;
   if (WriteMsg(Tag,Msg,"SIZE %s",Path) == false)
      return false;
   
   char *End;
   Size = strtol(Msg.c_str(),&End,10);
   if (Tag >= 400 || End == Msg.c_str())
      Size = 0;
   return true;
}
									/*}}}*/
// FTPConn::ModTime - Return the modification time of the file		/*{{{*/
// ---------------------------------------------------------------------
/* Like Size no error is returned if the command is not supported. If the
   command fails then time is set to the current time of day to fool 
   date checks. */
bool FTPConn::ModTime(const char *Path, time_t &Time)
{
   Time = time(&Time);
   
   // Query the mod time
   unsigned int Tag;
   string Msg;
   if (WriteMsg(Tag,Msg,"MDTM %s",Path) == false)
      return false;
   if (Tag >= 400 || Msg.empty() == true || isdigit(Msg[0]) == 0)
      return true;
   
   // Parse it
   struct tm tm;
   memset(&tm,0,sizeof(tm));   
   if (sscanf(Msg.c_str(),"%4d%2d%2d%2d%2d%2d",&tm.tm_year,&tm.tm_mon,
	      &tm.tm_mday,&tm.tm_hour,&tm.tm_min,&tm.tm_sec) != 6)
      return true;
   
   tm.tm_year -= 1900;
   tm.tm_mon--;
   
   /* We use timegm from the GNU C library, libapt-pkg will provide this
      symbol if it does not exist */
   Time = timegm(&tm);
   return true;
}
									/*}}}*/
// FTPConn::CreateDataFd - Get a data connection			/*{{{*/
// ---------------------------------------------------------------------
/* Create the data connection. Call FinalizeDataFd after this though.. */
bool FTPConn::CreateDataFd()
{
   close(DataFd);
   DataFd = -1;
   
   // Attempt to enter passive mode.
   if (TryPassive == true)
   {
      if (GoPasv() == false)
	 return false;
      
      // Oops, didn't work out, don't bother trying again.
      if (PasvAddr.sin_port == 0)
	 TryPassive = false;
   }
   
   // Passive mode?
   if (PasvAddr.sin_port != 0)
   {
      // Get a socket
      if ((DataFd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	 return _error->Errno("socket","Could not create a socket");
      
      // Connect to the server
      SetNonBlock(DataFd,true);
      if (connect(DataFd,(sockaddr *)&PasvAddr,sizeof(PasvAddr)) < 0 &&
	  errno != EINPROGRESS)
	 return _error->Errno("socket","Could not create a socket");
   
      /* This implements a timeout for connect by opening the connection
         nonblocking */
      if (WaitFd(ServerFd,true,TimeOut) == false)
	 return _error->Error("Could not connect data socket, connection timed out");
      unsigned int Err;
      unsigned int Len = sizeof(Err);
      if (getsockopt(ServerFd,SOL_SOCKET,SO_ERROR,&Err,&Len) != 0)
	 return _error->Errno("getsockopt","Failed");
      if (Err != 0)
	 return _error->Error("Could not connect.");
      
      return true;
   }
   
   // Port mode :<
   close(DataListenFd);
   DataListenFd = -1;

   // Get a socket
   if ((DataListenFd = socket(AF_INET,SOCK_STREAM,0)) < 0)
      return _error->Errno("socket","Could not create a socket");
   
   // Bind and listen
   sockaddr_in Addr;
   memset(&Addr,0,sizeof(Addr));
   if (bind(DataListenFd,(sockaddr *)&Addr,sizeof(Addr)) < 0)
      return _error->Errno("bind","Could not bind a socket");
   if (listen(DataListenFd,1) < 0)
      return _error->Errno("listen","Could not listen on the socket");
   SetNonBlock(DataListenFd,true);
   
   // Determine the name to send to the remote
   sockaddr_in Addr2;
   socklen_t Jnk = sizeof(Addr);
   if (getsockname(DataListenFd,(sockaddr *)&Addr,&Jnk) < 0)
      return _error->Errno("getsockname","Could not determine the socket's name");
   Jnk = sizeof(Addr2);
   if (getsockname(ServerFd,(sockaddr *)&Addr2,&Jnk) < 0)
      return _error->Errno("getsockname","Could not determine the socket's name");
   
   // This bit ripped from qftp
   unsigned long badr = ntohl(*(unsigned long *)&Addr2.sin_addr);
   unsigned long bp = ntohs(Addr.sin_port);

   // Send the port command
   unsigned int Tag;
   string Msg;
   if (WriteMsg(Tag,Msg,"PORT %d,%d,%d,%d,%d,%d", 
		(int) (badr >> 24) & 0xff, (int) (badr >> 16) & 0xff, 
		(int) (badr >> 8) & 0xff,  (int) badr & 0xff, 
		(int) (bp >> 8) & 0xff, (int) bp & 0xff) == false)
      return false;
   if (Tag >= 400)
      return _error->Error("Unable to send port command");

   return true;
}
									/*}}}*/
// FTPConn::Finalize - Complete the Data connection			/*{{{*/
// ---------------------------------------------------------------------
/* If the connection is in port mode this waits for the other end to hook
   up to us. */
bool FTPConn::Finalize()
{
   // Passive mode? Do nothing
   if (PasvAddr.sin_port != 0)
      return true;
   
   // Close any old socket..
   close(DataFd);
   DataFd = -1;
   
   // Wait for someone to connect..
   if (WaitFd(DataListenFd,false,TimeOut) == false)
      return _error->Error("Data socket connect timed out");
      
   // Accept the connection
   struct sockaddr_in Addr;
   socklen_t Len = sizeof(Addr);
   DataFd = accept(DataListenFd,(struct sockaddr *)&Addr,&Len);
   if (DataFd < 0)
      return _error->Errno("accept","Unable to accept connection");

   close(DataListenFd);
   DataListenFd = -1;
   
   return true;
}
									/*}}}*/
// FTPConn::Get - Get a file						/*{{{*/
// ---------------------------------------------------------------------
/* This opens a data connection, sends REST and RETR and then
   transfers the file over. */
bool FTPConn::Get(const char *Path,FileFd &To,unsigned long Resume,
		  MD5Summation &MD5,bool &Missing)
{
   Missing = false;
   if (CreateDataFd() == false)
      return false;

   unsigned int Tag;
   string Msg;   
   if (Resume != 0)
   {      
      if (WriteMsg(Tag,Msg,"REST %u",Resume) == false)
	 return false;
      if (Tag >= 400)
	 Resume = 0;
   }
   
   if (To.Truncate(Resume) == false)
      return false;

   if (To.Seek(0) == false)
      return false;
   
   if (Resume != 0)
   {
      if (MD5.AddFD(To.Fd(),Resume) == false)
      {
	 _error->Errno("read","Problem hashing file");
	 return false;
      }
   }
   
   // Send the get command
   if (WriteMsg(Tag,Msg,"RETR %s",Path) == false)
      return false;
   
   if (Tag >= 400)
   {
      if (Tag == 550)
	 Missing = true;
      return _error->Error("Unable to fetch file, server said '%s'",Msg.c_str());
   }
   
   // Finish off the data connection
   if (Finalize() == false)
      return false;
   
   // Copy loop
   unsigned char Buffer[4096];
   while (1)
   {
      // Wait for some data..
      if (WaitFd(DataFd,false,TimeOut) == false)
      {
	 Close();
	 return _error->Error("Data socket timed out");
      }
      
      // Read the data..
      int Res = read(DataFd,Buffer,sizeof(Buffer));
      if (Res == 0)
	 break;
      if (Res < 0)
      {
	 if (errno == EAGAIN)
	    continue;
	 break;
      }
   
      MD5.Add(Buffer,Res);
      if (To.Write(Buffer,Res) == false)
      {
	 Close();
	 return false;
      }      
   }

   // All done
   close(DataFd);
   DataFd = -1;
   
   // Read the closing message from the server
   if (ReadResp(Tag,Msg) == false)
      return false;
   if (Tag >= 400)
      return _error->Error("Data transfer failed, server said '%s'",Msg.c_str());
   return true;
}
									/*}}}*/

// FtpMethod::FtpMethod - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
FtpMethod::FtpMethod() : pkgAcqMethod("1.0",SendConfig)
{
   signal(SIGTERM,SigTerm);
   signal(SIGINT,SigTerm);
   
   Server = 0;
   FailFd = -1;
}
									/*}}}*/
// FtpMethod::SigTerm - Handle a fatal signal				/*{{{*/
// ---------------------------------------------------------------------
/* This closes and timestamps the open file. This is neccessary to get 
   resume behavoir on user abort */
void FtpMethod::SigTerm(int)
{
   if (FailFd == -1)
      _exit(100);
   close(FailFd);
   
   // Timestamp
   struct utimbuf UBuf;
   UBuf.actime = FailTime;
   UBuf.modtime = FailTime;
   utime(FailFile.c_str(),&UBuf);
   
   _exit(100);
}
									/*}}}*/
// FtpMethod::Configuration - Handle a configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* We stash the desired pipeline depth */
bool FtpMethod::Configuration(string Message)
{
   if (pkgAcqMethod::Configuration(Message) == false)
      return false;
   
   TimeOut = _config->FindI("Acquire::Ftp::Timeout",TimeOut);
   return true;
}
									/*}}}*/
// FtpMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* Fetch a single file, called by the base class..  */
bool FtpMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   const char *File = Get.Path.c_str();
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   Res.IMSHit = false;
   
   // Connect to the server
   if (Server == 0 || Server->Comp(Get) == false)
   {
      delete Server;
      Server = new FTPConn(Get);
   }
  
   // Could not connect is a transient error..
   if (Server->Open(this) == false)
   {
      Server->Close();
      Fail(true);
      return true;
   }
   
   // Get the files information
   Status("Query");
   unsigned long Size;
   if (Server->Size(File,Size) == false ||
       Server->ModTime(File,FailTime) == false)
   {
      Fail(true);
      return true;
   }
   Res.Size = Size;

   // See if it is an IMS hit
   if (Itm->LastModified == FailTime)
   {
      Res.Size = 0;
      Res.IMSHit = true;
      URIDone(Res);
      return true;
   }
   
   // See if the file exists
   struct stat Buf;
   if (stat(Itm->DestFile.c_str(),&Buf) == 0)
   {
      if (Size == (unsigned)Buf.st_size && FailTime == Buf.st_mtime)
      {
	 Res.Size = Buf.st_size;
	 Res.LastModified = Buf.st_mtime;
	 URIDone(Res);
	 return true;
      }
      
      // Resume?
      if (FailTime == Buf.st_mtime && Size > (unsigned)Buf.st_size)
	 Res.ResumePoint = Buf.st_size;
   }
   
   // Open the file
   MD5Summation MD5;
   {
      FileFd Fd(Itm->DestFile,FileFd::WriteAny);
      if (_error->PendingError() == true)
	 return false;
      
      URIStart(Res);
      
      FailFile = Itm->DestFile;
      FailFile.c_str();   // Make sure we dont do a malloc in the signal handler
      FailFd = Fd.Fd();
      
      bool Missing;
      if (Server->Get(File,Fd,Res.ResumePoint,MD5,Missing) == false)
      {
	 Fd.Close();
	 
	 // Timestamp
	 struct utimbuf UBuf;
	 time(&UBuf.actime);
	 UBuf.actime = FailTime;
	 UBuf.modtime = FailTime;
	 utime(FailFile.c_str(),&UBuf);
	 
	 // If the file is missing we hard fail otherwise transient fail
	 if (Missing == true)
	    return false;
	 Fail(true);
	 return true;
      }

      Res.Size = Fd.Size();
   }
   
   Res.LastModified = FailTime;
   Res.MD5Sum = MD5.Result();
   
   // Timestamp
   struct utimbuf UBuf;
   time(&UBuf.actime);
   UBuf.actime = FailTime;
   UBuf.modtime = FailTime;
   utime(Queue->DestFile.c_str(),&UBuf);
   FailFd = -1;

   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main(int argc,const char *argv[])
{ 
   /* See if we should be come the http client - we do this for http
      proxy urls */
   if (getenv("ftp_proxy") != 0)
   {
      URI Proxy = string(getenv("ftp_proxy"));
      if (Proxy.Access == "http")
      {
	 // Copy over the environment setting
	 char S[300];
	 snprintf(S,sizeof(S),"http_proxy=%s",getenv("ftp_proxy"));
	 putenv(S);
	 
	 // Run the http method
	 string Path = flNotFile(argv[0]) + "/http";
	 execl(Path.c_str(),Path.c_str(),0);
	 cerr << "Unable to invoke " << Path << endl;
	 exit(100);
      }      
   }
   
   FtpMethod Mth;
   
   return Mth.Run();
}
