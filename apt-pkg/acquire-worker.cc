// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-worker.cc,v 1.2 1998/10/20 02:39:13 jgg Exp $
/* ######################################################################

   Acquire Worker 

   The worker process can startup either as a Configuration prober
   or as a queue runner. As a configuration prober it only reads the
   configuration message and 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/acquire-worker.h"
#endif
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <strutl.h>

#include <unistd.h>
#include <signal.h>
									/*}}}*/

// Worker::Worker - Constructor for Queue startup			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Worker::Worker(Queue *Q,string Acc)
{
   OwnerQ = Q;
   Config = 0;
   Access = Acc;

   Construct();   
}
									/*}}}*/
// Worker::Worker - Constructor for method config startup		/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Worker::Worker(MethodConfig *Cnf)
{
   OwnerQ = 0;
   Config = Cnf;
   Access = Cnf->Access;

   Construct();   
}
									/*}}}*/
// Worker::Construct - Constructor helper				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Worker::Construct()
{
   Next = 0;
   Process = -1;
   InFd = -1;
   OutFd = -1;
   Debug = _config->FindB("Debug::pkgAcquire::Worker",false);
}
									/*}}}*/
// Worker::~Worker - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Worker::~Worker()
{
   close(InFd);
   close(OutFd);
   
   if (Process > 0)
      kill(Process,SIGINT);
}
									/*}}}*/
// Worker::Start - Start the worker process				/*{{{*/
// ---------------------------------------------------------------------
/* This forks the method and inits the communication channel */
bool pkgAcquire::Worker::Start()
{
   // Get the method path
   string Method = _config->FindDir("Dir::Bin::Methods") + Access;
   if (FileExists(Method) == false)
      return _error->Error("The method driver %s could not be found.",Method.c_str());

   if (Debug == true)
      clog << "Starting method '" << Method << '\'' << endl;

   // Create the pipes
   int Pipes[4] = {-1,-1,-1,-1};
   if (pipe(Pipes) != 0 || pipe(Pipes+2) != 0)
   {
      _error->Errno("pipe","Failed to create IPC pipe to subprocess");
      for (int I = 0; I != 4; I++)
	 close(Pipes[I]);
      return false;
   }
      
   // Fork off the process
   Process = fork();
   if (Process < 0)
   {
      cerr << "FATAL -> Failed to fork." << endl;
      exit(100);
   }

   // Spawn the subprocess
   if (Process == 0)
   {
      // Setup the FDs
      dup2(Pipes[1],STDOUT_FILENO);
      dup2(Pipes[2],STDIN_FILENO);
      dup2(((filebuf *)clog.rdbuf())->fd(),STDERR_FILENO);
      for (int I = 0; I != 4; I++)
	 close(Pipes[I]);
      SetCloseExec(STDOUT_FILENO,false);
      SetCloseExec(STDIN_FILENO,false);      
      SetCloseExec(STDERR_FILENO,false);
      
      const char *Args[2];
      Args[0] = Method.c_str();
      Args[1] = 0;
      execv(Args[0],(char **)Args);
      cerr << "Failed to exec method " << Args[0] << endl;
      exit(100);
   }

   // Fix up our FDs
   InFd = Pipes[0];
   OutFd = Pipes[3];
   SetNonBlock(Pipes[0],true);
   SetNonBlock(Pipes[3],true);
   close(Pipes[1]);
   close(Pipes[2]);
   
   // Read the configuration data
   if (WaitFd(InFd) == false ||
       ReadMessages() == false)
      return _error->Error("Method %s did not start correctly",Method.c_str());

   RunMessages();
   
   return true;
}
									/*}}}*/
// Worker::ReadMessages - Read all pending messages into the list	/*{{{*/
// ---------------------------------------------------------------------
/* This pulls full messages from the input FD into the message buffer. 
   It assumes that messages will not pause during transit so no
   fancy buffering is used. */
bool pkgAcquire::Worker::ReadMessages()
{
   char Buffer[4000];
   char *End = Buffer;
   
   while (1)
   {
      int Res = read(InFd,End,sizeof(Buffer) - (End-Buffer));
      
      // Process is dead, this is kind of bad..
      if (Res == 0)
      {
	 if (waitpid(Process,0,0) != Process)
	    _error->Warning("I waited but nothing was there!");
	 Process = -1;
	 close(InFd);
	 close(OutFd);
	 InFd = -1;
	 OutFd = -1;
	 return false;
      }
      
      // No data
      if (Res == -1)
	 return true;
      
      End += Res;
      
      // Look for the end of the message
      for (char *I = Buffer; I < End; I++)
      {
	 if (I[0] != '\n' || I[1] != '\n')
	    continue;
	 
	 // Pull the message out
	 string Message(Buffer,0,I-Buffer);

	 // Fix up the buffer
	 for (; I < End && *I == '\n'; I++);
	 End -= I-Buffer;	 
	 memmove(Buffer,I,End-Buffer);
	 I = Buffer;

	 if (Debug == true)
	    clog << "Message " << Access << ':' << QuoteString(Message,"\n") << endl;
	 
	 MessageQueue.push_back(Message);
      }
      if (End == Buffer)
	 return true;

      if (WaitFd(InFd) == false)
	 return false;
   }
   
   return true;
}
									/*}}}*/

// Worker::RunMessage - Empty the message queue				/*{{{*/
// ---------------------------------------------------------------------
/* This takes the messages from the message queue and runs them through
   the parsers in order. */
bool pkgAcquire::Worker::RunMessages()
{
   while (MessageQueue.empty() == false)
   {
      string Message = MessageQueue.front();
      MessageQueue.erase(MessageQueue.begin());
      
      // Fetch the message number
      char *End;
      int Number = strtol(Message.c_str(),&End,10);
      if (End == Message.c_str())
	 return _error->Error("Invalid message from method %s: %s",Access.c_str(),Message.c_str());

      // Determine the message number and dispatch
      switch (Number)
      {
	 case 100:
	 if (Capabilities(Message) == false)
	    return _error->Error("Unable to process Capabilities message from %s",Access.c_str());
	 break;
      }      
   }
   return true;
}
									/*}}}*/
// Worker::Capabilities - 100 Capabilities handler			/*{{{*/
// ---------------------------------------------------------------------
/* This parses the capabilities message and dumps it into the configuration
   structure. */
bool pkgAcquire::Worker::Capabilities(string Message)
{
   if (Config == 0)
      return true;
   
   Config->Version = LookupTag(Message,"Version");
   Config->SingleInstance = StringToBool(LookupTag(Message,"Single-Instance"),false);
   Config->PreScan = StringToBool(LookupTag(Message,"Pre-Scan"),false);

   // Some debug text
   if (Debug == true)
   {
      clog << "Configured access method " << Config->Access << endl;
      clog << "Version: " << Config->Version << " SingleInstance: " << 
	 Config->SingleInstance << " PreScan: " << Config->PreScan << endl;
   }
   
   return true;
}
									/*}}}*/
