// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.1 1998/10/25 07:07:30 jgg Exp $
/* ######################################################################

   GZip method - Take a file URI in and decompress it into the target 
   file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/acquire-worker.h>
#include <strutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <wait.h>
#include <stdio.h>
									/*}}}*/

// Fail - Generate a failure message					/*{{{*/
// ---------------------------------------------------------------------
/* */
void Fail(string URI)
{
   string Err = "Undetermined Error";
   if (_error->empty() == false)
      _error->PopMessage(Err);
   
   printf("400 URI Failure\n"
	  "URI: %s\n"
	  "Message: %s\n\n",URI.c_str(),Err.c_str());
   _error->Discard();
}
									/*}}}*/

int main()
{
   setlinebuf(stdout);
   SetNonBlock(STDIN_FILENO,true);
   
   printf("100 Capabilities\n"
	  "Version: 1.0\n"
	  "Pipeline: true\n"
	  "Send-Config: true\n\n");

   vector<string> Messages;   
   while (1)
   {
      if (WaitFd(STDIN_FILENO) == false ||
	  ReadMessages(STDIN_FILENO,Messages) == false)
	 return 0;

      while (Messages.empty() == false)
      {
	 string Message = Messages.front();
	 Messages.erase(Messages.begin());
	 
	 // Fetch the message number
	 char *End;
	 int Number = strtol(Message.c_str(),&End,10);
	 if (End == Message.c_str())
	 {
	    cerr << "Malformed message!" << endl;
	    return 100;
	 }
	 
	 // 601 configuration message
	 if (Number == 601)
	 {
	    pkgInjectConfiguration(Message,*_config);
	    continue;
	 }	 

	 // 600 URI Fetch message
	 if (Number != 600)
	    continue;
	 
	 // Grab the URI bit 
	 string URI = LookupTag(Message,"URI");
	 string Target = LookupTag(Message,"Filename");
	 
	 // Grab the filename
	 string::size_type Pos = URI.find(':');
	 if (Pos == string::npos)
	 {
	    _error->Error("Invalid message");
	    Fail(URI);
	    continue;
	 }
	 string File = string(URI,Pos+1);
	 
	 // Start the reply message
	 string Result = "201 URI Done";
	 Result += "\nURI: " + URI;
	 Result += "\nFileName: " + Target;
	 
	 // See if the file exists
	 FileFd From(File,FileFd::ReadOnly);
	 FileFd To(Target,FileFd::WriteEmpty);
	 To.EraseOnFailure();
	 if (_error->PendingError() == true)
	 {
	    Fail(URI);
	    continue;
	 }	 
	 
	 // Fork gzip
	 int Process = fork();
	 if (Process < 0)
	 {
	    _error->Errno("fork","Couldn't fork gzip");
	    Fail(URI);
	    continue;
	 }
	 
	 // The child
	 if (Process == 0)
	 {
	    dup2(From.Fd(),STDIN_FILENO);
	    dup2(To.Fd(),STDOUT_FILENO);
	    
	    const char *Args[3];
	    Args[0] = _config->FindFile("Dir::bin::gzip","gzip").c_str();
	    Args[1] = "-d";
	    Args[2] = 0;
	    execvp(Args[0],(char **)Args);
	 }
	 From.Close();
	 To.Close();
	 
	 // Wait for gzip to finish
	 int Status;
	 if (waitpid(Process,&Status,0) != Process)
	 {
	    _error->Errno("wait","Waiting for gzip failed");
	    Fail(URI);
	    continue;
	 }	 

	 if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
	 {
	    _error->Error("gzip failed, perhaps the disk is full or the directory permissions are wrong.");
	    Fail(URI);
	    continue;
	 }	 
	 
	 // Transfer the modification times
	 struct stat Buf;
	 if (stat(File.c_str(),&Buf) != 0)
	 {
	    _error->Errno("stat","Failed to stat");
	    Fail(URI);
	    continue;
	 }
	 struct utimbuf TimeBuf;
	 TimeBuf.actime = Buf.st_atime;
	 TimeBuf.modtime = Buf.st_mtime;
	 if (utime(Target.c_str(),&TimeBuf) != 0)
	 {
	    _error->Errno("utime","Failed to set modification time");
	    Fail(URI);
	    continue;
	 }
	 
	 // Send the message
	 Result += "\n\n";
	 if (write(STDOUT_FILENO,Result.begin(),Result.length()) != 
	     (signed)Result.length())
	    return 100;
      }      
   }
   
   return 0;
}
