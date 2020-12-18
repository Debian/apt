// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   RSH method - Transfer files via rsh compatible program

   Written by Ben Collins <bcollins@debian.org>, Copyright (c) 2000
   Licensed under the GNU General Public License v2 [no exception clauses]

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include "rsh.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

unsigned long TimeOut = 30;
Configuration::Item const *RshOptions = 0;
time_t RSHMethod::FailTime = 0;
std::string RSHMethod::FailFile;
int RSHMethod::FailFd = -1;

// RSHConn::RSHConn - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
RSHConn::RSHConn(std::string const &pProg, URI Srv) : Len(0), WriteFd(-1), ReadFd(-1),
                            ServerName(Srv), Prog(pProg), Process(-1) {
   Buffer[0] = '\0';
}
									/*}}}*/
// RSHConn::RSHConn - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
RSHConn::~RSHConn()
{
   Close();
}
									/*}}}*/
// RSHConn::Close - Forcibly terminate the connection			/*{{{*/
// ---------------------------------------------------------------------
/* Often this is called when things have gone wrong to indicate that the
   connection is no longer usable. */
void RSHConn::Close()
{
   if (Process == -1)
      return;
   
   close(WriteFd);
   close(ReadFd);
   kill(Process,SIGINT);
   ExecWait(Process,"",true);
   WriteFd = -1;
   ReadFd = -1;
   Process = -1;
}
									/*}}}*/
// RSHConn::Open - Connect to a host					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHConn::Open()
{
   // Use the already open connection if possible.
   if (Process != -1)
      return true;

   if (Connect(ServerName.Host,ServerName.Port,ServerName.User) == false)
      return false;

   return true;
}
									/*}}}*/
// RSHConn::Connect - Fire up rsh and connect				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHConn::Connect(std::string const &Host, unsigned int Port, std::string const &User)
{
   char *PortStr = NULL;
   if (Port != 0)
   {
      if (asprintf (&PortStr, "%d", Port) == -1 || PortStr == NULL)
         return _error->Errno("asprintf", _("Failed"));
   }

   // Create the pipes
   int Pipes[4] = {-1,-1,-1,-1};
   if (pipe(Pipes) != 0 || pipe(Pipes+2) != 0)
   {
      _error->Errno("pipe",_("Failed to create IPC pipe to subprocess"));
      for (int I = 0; I != 4; I++)
	 close(Pipes[I]);
      return false;
   }
   for (int I = 0; I != 4; I++)
      SetCloseExec(Pipes[I],true);
   
   Process = ExecFork();

   // The child
   if (Process == 0)
   {
      const char *Args[400];
      unsigned int i = 0;

      dup2(Pipes[1],STDOUT_FILENO);
      dup2(Pipes[2],STDIN_FILENO);

      // Probably should do
      // dup2(open("/dev/null",O_RDONLY),STDERR_FILENO);

      Args[i++] = Prog.c_str();

      // Insert user-supplied command line options
      Configuration::Item const *Opts = RshOptions;
      if (Opts != 0)
      {
         Opts = Opts->Child;
	 for (; Opts != 0; Opts = Opts->Next)
         {
            if (Opts->Value.empty() == true)
               continue;
            Args[i++] = Opts->Value.c_str();
         }
      }

      if (User.empty() == false) {
         Args[i++] = "-l";
	 Args[i++] = User.c_str();
      }
      if (PortStr != NULL) {
         Args[i++] = "-p";
         Args[i++] = PortStr;
      }
      if (Host.empty() == false) {
         Args[i++] = Host.c_str();
      }
      Args[i++] = "/bin/sh";
      Args[i] = 0;
      execvp(Args[0],(char **)Args);
      exit(100);
   }

   if (PortStr != NULL)
      free(PortStr);

   ReadFd = Pipes[0];
   WriteFd = Pipes[3];
   SetNonBlock(Pipes[0],true);
   SetNonBlock(Pipes[3],true);
   close(Pipes[1]);
   close(Pipes[2]);
   
   return true;
}
bool RSHConn::Connect(std::string const &Host, std::string const &User)
{
   return Connect(Host, 0, User);
}
									/*}}}*/
// RSHConn::ReadLine - Very simple buffered read with timeout		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHConn::ReadLine(std::string &Text)
{
   if (Process == -1 || ReadFd == -1)
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
         Text = std::string(Buffer,I);
         memmove(Buffer,Buffer+I,Len - I);
         Len -= I;
         return true;
      }

      // Wait for some data..
      if (WaitFd(ReadFd,false,TimeOut) == false)
      {
         Close();
         return _error->Error(_("Connection timeout"));
      }

      // Suck it back
      int Res = read(ReadFd,Buffer + Len,sizeof(Buffer) - Len);
      if (Res <= 0)
      {
         _error->Errno("read",_("Read error"));
         Close();
         return false;
      }
      Len += Res;
   }

   return _error->Error(_("A response overflowed the buffer."));
}
									/*}}}*/
// RSHConn::WriteMsg - Send a message with optional remote sync.	/*{{{*/
// ---------------------------------------------------------------------
/* The remote sync flag appends a || echo which will insert blank line
   once the command completes. */
bool RSHConn::WriteMsg(std::string &Text,bool Sync,const char *Fmt,...)
{
   va_list args;
   va_start(args,Fmt);

   // sprintf into a buffer
   char Tmp[1024];
   vsnprintf(Tmp,sizeof(Tmp),Fmt,args);
   va_end(args);

   // concat to create the real msg
   std::string Msg;
   if (Sync == true)
      Msg = std::string(Tmp) + " 2> /dev/null || echo\n";
   else
      Msg = std::string(Tmp) + " 2> /dev/null\n";

   // Send it off
   const char *S = Msg.c_str();
   unsigned long Len = strlen(S);
   unsigned long Start = 0;
   while (Len != 0)
   {
      if (WaitFd(WriteFd,true,TimeOut) == false)
      {
	 
	 Close();
	 return _error->Error(_("Connection timeout"));
      }      
      
      int Res = write(WriteFd,S + Start,Len);
      if (Res <= 0)
      {
         _error->Errno("write",_("Write error"));
         Close();
         return false;
      }

      Len -= Res;
      Start += Res;
   }

   if (Sync == true)
      return ReadLine(Text);
   return true;
}
									/*}}}*/
// RSHConn::Size - Return the size of the file				/*{{{*/
// ---------------------------------------------------------------------
/* Right now for successful transfer the file size must be known in
   advance. */
bool RSHConn::Size(const char *Path,unsigned long long &Size)
{
   // Query the size
	std::string Msg;
   Size = 0;

   if (WriteMsg(Msg,true,"find %s -follow -printf '%%s\\n'",Path) == false)
      return false;
   
   // FIXME: Sense if the bad reply is due to a File Not Found. 
   
   char *End;
   Size = strtoull(Msg.c_str(),&End,10);
   if (End == Msg.c_str())
      return _error->Error(_("File not found"));
   return true;
}
									/*}}}*/
// RSHConn::ModTime - Get the modification time in UTC			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHConn::ModTime(const char *Path, time_t &Time)
{
   Time = time(&Time);
   // Query the mod time
   std::string Msg;

   if (WriteMsg(Msg,true,"TZ=UTC find %s -follow -printf '%%TY%%Tm%%Td%%TH%%TM%%TS\\n'",Path) == false)
      return false;

   // Parse it
   return FTPMDTMStrToTime(Msg.c_str(), Time);
}
									/*}}}*/
// RSHConn::Get - Get a file						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHConn::Get(const char *Path,FileFd &To,unsigned long long Resume,
                  Hashes &Hash,bool &Missing, unsigned long long Size)
{
   Missing = false;

   // Round to a 2048 byte block
   Resume = Resume - (Resume % 2048);

   if (To.Truncate(Resume) == false)
      return false;
   if (To.Seek(0) == false)
      return false;

   if (Resume != 0) {
      if (Hash.AddFD(To,Resume) == false) {
	 _error->Errno("read",_("Problem hashing file"));
	 return false;
      }
   }
   
   // FIXME: Detect file-not openable type errors.
   std::string Jnk;
   if (WriteMsg(Jnk,false,"dd if=%s bs=2048 skip=%u", Path, Resume / 2048) == false)
      return false;

   // Copy loop
   unsigned long long MyLen = Resume;
   unsigned char Buffer[4096];
   while (MyLen < Size)
   {
      // Wait for some data..
      if (WaitFd(ReadFd,false,TimeOut) == false)
      {
         Close();
         return _error->Error(_("Data socket timed out"));
      }

      // Read the data..
      int Res = read(ReadFd,Buffer,sizeof(Buffer));
      if (Res == 0)
      {
	 Close();
	 return _error->Error(_("Connection closed prematurely"));
      }
      
      if (Res < 0)
      {
         if (errno == EAGAIN)
            continue;
         break;
      }
      MyLen += Res;

      Hash.Add(Buffer,Res);
      if (To.Write(Buffer,Res) == false)
      {
         Close();
         return false;
      }
   }

   return true;
}
									/*}}}*/

// RSHMethod::RSHMethod - Constructor					/*{{{*/
RSHMethod::RSHMethod(std::string &&pProg) : aptMethod(std::move(pProg),"1.0",SendConfig | SendURIEncoded)
{
   signal(SIGTERM,SigTerm);
   signal(SIGINT,SigTerm);
   Server = 0;
   FailFd = -1;
}
									/*}}}*/
// RSHMethod::Configuration - Handle a configuration message		/*{{{*/
// ---------------------------------------------------------------------
bool RSHMethod::Configuration(std::string Message)
{
   // enabling privilege dropping for this method requires configuration…
   // … which is otherwise lifted straight from root, so use it by default.
   _config->Set(std::string("Binary::") + Binary + "::APT::Sandbox::User", "");

   if (aptMethod::Configuration(Message) == false)
      return false;

   std::string const timeconf = std::string("Acquire::") + Binary + "::Timeout";
   TimeOut = _config->FindI(timeconf, TimeOut);
   std::string const optsconf = std::string("Acquire::") + Binary + "::Options";
   RshOptions = _config->Tree(optsconf.c_str());

   return true;
}
									/*}}}*/
// RSHMethod::SigTerm - Clean up and timestamp the files on exit	/*{{{*/
// ---------------------------------------------------------------------
/* */
void RSHMethod::SigTerm(int)
{
   if (FailFd == -1)
      _exit(100);

   // Transfer the modification times
   struct timeval times[2];
   times[0].tv_sec = FailTime;
   times[1].tv_sec = FailTime;
   times[0].tv_usec = times[1].tv_usec = 0;
   utimes(FailFile.c_str(), times);
   close(FailFd);

   _exit(100);
}
									/*}}}*/
// RSHMethod::Fetch - Fetch a URI					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RSHMethod::Fetch(FetchItem *Itm)
{
   URI Get(Itm->Uri);
   auto const File = DecodeSendURI(Get.Path);
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   Res.IMSHit = false;

   // Connect to the server
   if (Server == 0 || Server->Comp(Get) == false) {
      delete Server;
      Server = new RSHConn(Binary, Get);
   }

   // Could not connect is a transient error..
   if (Server->Open() == false) {
      Server->Close();
      Fail(true);
      return true;
   }

   // We say this mainly because the pause here is for the
   // ssh connection that is still going
   Status(_("Connecting to %s"), Get.Host.c_str());

   // Get the files information
   unsigned long long Size;
   if (not Server->Size(File.c_str(), Size) ||
       not Server->ModTime(File.c_str(), FailTime))
   {
      //Fail(true);
      //_error->Error(_("File not found")); // Will be handled by Size
      return false;
   }
   Res.Size = Size;

   // See if it is an IMS hit
   if (Itm->LastModified == FailTime) {
      Res.Size = 0;
      Res.IMSHit = true;
      URIDone(Res);
      return true;
   }

   // See if the file exists
   struct stat Buf;
   if (stat(Itm->DestFile.c_str(),&Buf) == 0) {
      if (Size == (unsigned long long)Buf.st_size && FailTime == Buf.st_mtime) {
	 Res.Size = Buf.st_size;
	 Res.LastModified = Buf.st_mtime;
	 Res.ResumePoint = Buf.st_size;
	 URIDone(Res);
	 return true;
      }

      // Resume?
      if (FailTime == Buf.st_mtime && Size > (unsigned long long)Buf.st_size)
	 Res.ResumePoint = Buf.st_size;
   }

   // Open the file
   Hashes Hash(Itm->ExpectedHashes);
   {
      FileFd Fd(Itm->DestFile,FileFd::WriteAny);
      if (_error->PendingError() == true)
	 return false;
      
      URIStart(Res);

      FailFile = Itm->DestFile;
      FailFile.c_str();   // Make sure we don't do a malloc in the signal handler
      FailFd = Fd.Fd();

      bool Missing;
      if (not Server->Get(File.c_str(), Fd, Res.ResumePoint, Hash, Missing, Res.Size))
      {
	 Fd.Close();

	 // Timestamp
	 struct timeval times[2];
	 times[0].tv_sec = FailTime;
	 times[1].tv_sec = FailTime;
	 times[0].tv_usec = times[1].tv_usec = 0;
	 utimes(FailFile.c_str(), times);

	 // If the file is missing we hard fail otherwise transient fail
	 if (Missing == true)
	    return false;
	 Fail(true);
	 return true;
      }

      Res.Size = Fd.Size();
      struct timeval times[2];
      times[0].tv_sec = FailTime;
      times[1].tv_sec = FailTime;
      times[0].tv_usec = times[1].tv_usec = 0;
      utimes(Fd.Name().c_str(), times);
      FailFd = -1;
   }

   Res.LastModified = FailTime;
   Res.TakeHashes(Hash);

   URIDone(Res);

   return true;
}
									/*}}}*/

int main(int, const char *argv[])
{
   return RSHMethod(flNotDir(argv[0])).Run();
}
