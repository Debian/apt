// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: file.cc,v 1.3 1998/10/23 00:50:02 jgg Exp $
/* ######################################################################

   File URI method for APT
   
   This simply checks that the file specified exists, if so the relevent
   information is returned. If a .gz filename is specified then the file
   name with .gz removed will also be checked and information about it
   will be returned in Alt-*
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <strutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
									/*}}}*/

// Fail - Generate a failure message					/*{{{*/
// ---------------------------------------------------------------------
/* */
void Fail(string URI)
{
   printf("400 URI Failure\n"
	  "URI: %s\n"
	  "Message: File does not exist\n\n",URI.c_str());
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
	 
	 // Grab the filename
	 string::size_type Pos = URI.find(':');
	 if (Pos == string::npos)
	 {
	    Fail(URI);
	    continue;
	 }
	 string File = string(URI,Pos+1);
	 
	 // Grab the modification time
	 time_t LastMod;
	 string LTime = LookupTag(Message,"Last-Modified");
	 if (LTime.empty() == false && StrToTime(LTime,LastMod) == false)
	    LTime = string();
	 
	 // Start the reply message
	 string Result = "201 URI Done";
	 Result += "\nURI: " + URI;

	 // See if the file exists
	 struct stat Buf;
	 bool Ok = false;
	 if (stat(File.c_str(),&Buf) == 0)
	 {
	    char S[300];
	    sprintf(S,"\nSize: %ld",Buf.st_size);
	    
	    Result += "\nFilename: " + File;
	    Result += S;
	    Result += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
	    if (LTime.empty() == false && LastMod == Buf.st_mtime)
	       Result += "\nIMS-Hit: true";
		
	    Ok = true;
	 }
	 
	 // See if we can compute a file without a .gz exentsion
	 Pos = File.rfind(".gz");
	 if (Pos + 3 == File.length())
	 {
	    File = string(File,0,Pos);
	    if (stat(File.c_str(),&Buf) == 0)
	    {
	       char S[300];
	       sprintf(S,"\nAlt-Size: %ld",Buf.st_size);
	       
	       Result += "\nAlt-Filename: " + File;
	       Result += S;
	       Result += "\nAlt-Last-Modified: " + TimeRFC1123(Buf.st_mtime);
	       if (LTime.empty() == false && LastMod == Buf.st_mtime)
		  Result += "\nAlt-IMS-Hit: true";
	       
	       Ok = true;
	    }
	 }
	 
	 // Did we find something?
	 if (Ok == false)
	 {
	    Fail(URI);
	    continue;
	 }
	 Result += "\n\n";
	 
	 // Send the message
	 if (write(STDOUT_FILENO,Result.begin(),Result.length()) != 
	     (signed)Result.length())
	    return 100;
      }      
   }
   
   return 0;
}
