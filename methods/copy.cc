// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: copy.cc,v 1.3 1998/10/26 07:11:52 jgg Exp $
/* ######################################################################

   Copy URI - This method takes a uri like a file: uri and copies it
   to the destination file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <strutl.h>
#include <apt-pkg/error.h>

#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
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
	  "Pipeline: true\n\n");
      
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
	 
	 // We only understand 600 URI Fetch messages
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
	 
	 // Copy the file
	 if (CopyFile(From,To) == false)
	 {
	    Fail(URI);
	    continue;
	 }	 
	
	 From.Close();
	 To.Close();
	 
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
	    To.OpFail();
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
