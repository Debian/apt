// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.cc,v 1.27.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Acquire Method

   This is a skeleton class that implements most of the functionality
   of a method and some useful functions to make method implementation
   simpler. The methods all derive this and specialize it. The most
   complex implementation is the http method which needs to provide
   pipelining, it runs the message engine at the same time it is 
   downloading files..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>

#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/signal.h>
									/*}}}*/

using namespace std;

// AcqMethod::pkgAcqMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* This constructs the initialization text */
pkgAcqMethod::pkgAcqMethod(const char *Ver,unsigned long Flags)
{
   char S[300] = "";
   char *End = S;
   strcat(End,"100 Capabilities\n");
   sprintf(End+strlen(End),"Version: %s\n",Ver);

   if ((Flags & SingleInstance) == SingleInstance)
      strcat(End,"Single-Instance: true\n");
   
   if ((Flags & Pipeline) == Pipeline)
      strcat(End,"Pipeline: true\n");
   
   if ((Flags & SendConfig) == SendConfig)
      strcat(End,"Send-Config: true\n");

   if ((Flags & LocalOnly) == LocalOnly)
      strcat(End,"Local-Only: true\n");

   if ((Flags & NeedsCleanup) == NeedsCleanup)
      strcat(End,"Needs-Cleanup: true\n");

   if ((Flags & Removable) == Removable)
      strcat(End,"Removable: true\n");
   strcat(End,"\n");

   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);

   SetNonBlock(STDIN_FILENO,true);

   Queue = 0;
   QueueBack = 0;
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(bool Transient)
{
   string Err = "Undetermined Error";
   if (_error->empty() == false)
      _error->PopMessage(Err);   
   _error->Discard();
   Fail(Err,Transient);
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(string Err,bool Transient)
{
   // Strip out junk from the error messages
   for (string::iterator I = Err.begin(); I != Err.end(); I++)
   {
      if (*I == '\r') 
	 *I = ' ';
      if (*I == '\n') 
	 *I = ' ';
   }
   
   char S[1024];
   if (Queue != 0)
   {
      snprintf(S,sizeof(S)-50,"400 URI Failure\nURI: %s\n"
	       "Message: %s %s\n",Queue->Uri.c_str(),Err.c_str(),
	       FailExtra.c_str());

      // Dequeue
      FetchItem *Tmp = Queue;
      Queue = Queue->Next;
      delete Tmp;
      if (Tmp == QueueBack)
	 QueueBack = Queue;
   }
   else
      snprintf(S,sizeof(S)-50,"400 URI Failure\nURI: <UNKNOWN>\n"
	       "Message: %s %s\n",Err.c_str(),
	       FailExtra.c_str());
      
   // Set the transient flag 
   if (Transient == true)
      strcat(S,"Transient-Failure: true\n\n");
   else
      strcat(S,"\n");
   
   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);
}
									/*}}}*/
// AcqMethod::URIStart - Indicate a download is starting		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::URIStart(FetchResult &Res)
{
   if (Queue == 0)
      abort();
   
   char S[1024] = "";
   char *End = S;
   
   End += snprintf(S,sizeof(S),"200 URI Start\nURI: %s\n",Queue->Uri.c_str());
   if (Res.Size != 0)
      End += snprintf(End,sizeof(S)-4 - (End - S),"Size: %lu\n",Res.Size);
   
   if (Res.LastModified != 0)
      End += snprintf(End,sizeof(S)-4 - (End - S),"Last-Modified: %s\n",
		      TimeRFC1123(Res.LastModified).c_str());
   
   if (Res.ResumePoint != 0)
      End += snprintf(End,sizeof(S)-4 - (End - S),"Resume-Point: %lu\n",
		      Res.ResumePoint);
      
   strcat(End,"\n");
   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);
}
									/*}}}*/
// AcqMethod::URIDone - A URI is finished				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::URIDone(FetchResult &Res, FetchResult *Alt)
{
   if (Queue == 0)
      abort();
   
   char S[1024] = "";
   char *End = S;
   
   End += snprintf(S,sizeof(S),"201 URI Done\nURI: %s\n",Queue->Uri.c_str());

   if (Res.Filename.empty() == false)
      End += snprintf(End,sizeof(S)-50 - (End - S),"Filename: %s\n",Res.Filename.c_str());
   
   if (Res.Size != 0)
      End += snprintf(End,sizeof(S)-50 - (End - S),"Size: %lu\n",Res.Size);
   
   if (Res.LastModified != 0)
      End += snprintf(End,sizeof(S)-50 - (End - S),"Last-Modified: %s\n",
		      TimeRFC1123(Res.LastModified).c_str());

   if (Res.MD5Sum.empty() == false)
   {
      End += snprintf(End,sizeof(S)-50 - (End - S),"MD5-Hash: %s\n",Res.MD5Sum.c_str());
      End += snprintf(End,sizeof(S)-50 - (End - S),"MD5Sum-Hash: %s\n",Res.MD5Sum.c_str());
   }
   if (Res.SHA1Sum.empty() == false)
      End += snprintf(End,sizeof(S)-50 - (End - S),"SHA1-Hash: %s\n",Res.SHA1Sum.c_str());
   if (Res.SHA256Sum.empty() == false)
      End += snprintf(End,sizeof(S)-50 - (End - S),"SHA256-Hash: %s\n",Res.SHA256Sum.c_str());
   if (Res.GPGVOutput.size() > 0)
      End += snprintf(End,sizeof(S)-50 - (End - S),"GPGVOutput:\n");     
   for (vector<string>::iterator I = Res.GPGVOutput.begin();
      I != Res.GPGVOutput.end(); I++)
      End += snprintf(End,sizeof(S)-50 - (End - S), " %s\n", (*I).c_str());

   if (Res.ResumePoint != 0)
      End += snprintf(End,sizeof(S)-50 - (End - S),"Resume-Point: %lu\n",
		      Res.ResumePoint);

   if (Res.IMSHit == true)
      strcat(End,"IMS-Hit: true\n");
   End = S + strlen(S);
   
   if (Alt != 0)
   {
      if (Alt->Filename.empty() == false)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-Filename: %s\n",Alt->Filename.c_str());
      
      if (Alt->Size != 0)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-Size: %lu\n",Alt->Size);
      
      if (Alt->LastModified != 0)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-Last-Modified: %s\n",
			 TimeRFC1123(Alt->LastModified).c_str());
      
      if (Alt->MD5Sum.empty() == false)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-MD5-Hash: %s\n",
			 Alt->MD5Sum.c_str());
      if (Alt->SHA1Sum.empty() == false)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-SHA1-Hash: %s\n",
			 Alt->SHA1Sum.c_str());
      if (Alt->SHA256Sum.empty() == false)
	 End += snprintf(End,sizeof(S)-50 - (End - S),"Alt-SHA256-Hash: %s\n",
			 Alt->SHA256Sum.c_str());
      
      if (Alt->IMSHit == true)
	 strcat(End,"Alt-IMS-Hit: true\n");
   }
   
   strcat(End,"\n");
   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);

   // Dequeue
   FetchItem *Tmp = Queue;
   Queue = Queue->Next;
   delete Tmp;
   if (Tmp == QueueBack)
      QueueBack = Queue;
}
									/*}}}*/
// AcqMethod::MediaFail - Syncronous request for new media		/*{{{*/
// ---------------------------------------------------------------------
/* This sends a 403 Media Failure message to the APT and waits for it
   to be ackd */
bool pkgAcqMethod::MediaFail(string Required,string Drive)
{
   char S[1024];
   snprintf(S,sizeof(S),"403 Media Failure\nMedia: %s\nDrive: %s\n\n",
	    Required.c_str(),Drive.c_str());

   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);
   
   vector<string> MyMessages;
   
   /* Here we read messages until we find a 603, each non 603 message is
      appended to the main message list for later processing */
   while (1)
   {
      if (WaitFd(STDIN_FILENO) == false)
	 return false;
      
      if (ReadMessages(STDIN_FILENO,MyMessages) == false)
	 return false;

      string Message = MyMessages.front();
      MyMessages.erase(MyMessages.begin());
      
      // Fetch the message number
      char *End;
      int Number = strtol(Message.c_str(),&End,10);
      if (End == Message.c_str())
      {	 
	 cerr << "Malformed message!" << endl;
	 exit(100);
      }

      // Change ack
      if (Number == 603)
      {
	 while (MyMessages.empty() == false)
	 {
	    Messages.push_back(MyMessages.front());
	    MyMessages.erase(MyMessages.begin());
	 }

	 return !StringToBool(LookupTag(Message,"Failed"),false);
      }
      
      Messages.push_back(Message);
   }   
}
									/*}}}*/
// AcqMethod::Configuration - Handle the configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* This parses each configuration entry and puts it into the _config 
   Configuration class. */
bool pkgAcqMethod::Configuration(string Message)
{
   ::Configuration &Cnf = *_config;
   
   const char *I = Message.c_str();
   const char *MsgEnd = I + Message.length();
   
   unsigned int Length = strlen("Config-Item");
   for (; I + Length < MsgEnd; I++)
   {
      // Not a config item
      if (I[Length] != ':' || stringcasecmp(I,I+Length,"Config-Item") != 0)
	 continue;
      
      I += Length + 1;
      
      for (; I < MsgEnd && *I == ' '; I++);
      const char *Equals = I;
      for (; Equals < MsgEnd && *Equals != '='; Equals++);
      const char *End = Equals;
      for (; End < MsgEnd && *End != '\n'; End++);
      if (End == Equals)
	 return false;
      
      Cnf.Set(DeQuoteString(string(I,Equals-I)),
	      DeQuoteString(string(Equals+1,End-Equals-1)));
      I = End;
   }
   
   return true;
}
									/*}}}*/
// AcqMethod::Run - Run the message engine				/*{{{*/
// ---------------------------------------------------------------------
/* Fetch any messages and execute them. In single mode it returns 1 if
   there are no more available messages - any other result is a 
   fatal failure code! */
int pkgAcqMethod::Run(bool Single)
{
   while (1)
   {
      // Block if the message queue is empty
      if (Messages.empty() == true)
      {
	 if (Single == false)
	    if (WaitFd(STDIN_FILENO) == false)
	       break;
	 if (ReadMessages(STDIN_FILENO,Messages) == false)
	    break;
      }
            
      // Single mode exits if the message queue is empty
      if (Single == true && Messages.empty() == true)
	 return -1;
      
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

      switch (Number)
      {	 
	 case 601:
	 if (Configuration(Message) == false)
	    return 100;
	 break;
	 
	 case 600:
	 {
	    FetchItem *Tmp = new FetchItem;
	    
	    Tmp->Uri = LookupTag(Message,"URI");
	    Tmp->DestFile = LookupTag(Message,"FileName");
	    if (StrToTime(LookupTag(Message,"Last-Modified"),Tmp->LastModified) == false)
	       Tmp->LastModified = 0;
	    Tmp->IndexFile = StringToBool(LookupTag(Message,"Index-File"),false);
	    Tmp->Next = 0;
	    
	    // Append it to the list
	    FetchItem **I = &Queue;
	    for (; *I != 0; I = &(*I)->Next);
	    *I = Tmp;
	    if (QueueBack == 0)
	       QueueBack = Tmp;
	    
	    // Notify that this item is to be fetched.
	    if (Fetch(Tmp) == false)
	       Fail();
	    
	    break;					     
	 }   
      }      
   }

   Exit();
   return 0;
}
									/*}}}*/
// AcqMethod::Log - Send a log message					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Log(const char *Format,...)
{
   string CurrentURI = "<UNKNOWN>";
   if (Queue != 0)
      CurrentURI = Queue->Uri;
   
   va_list args;
   va_start(args,Format);

   // sprintf the description
   char S[1024];
   unsigned int Len = snprintf(S,sizeof(S)-4,"101 Log\nURI: %s\n"
			       "Message: ",CurrentURI.c_str());

   vsnprintf(S+Len,sizeof(S)-4-Len,Format,args);
   strcat(S,"\n\n");
   
   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);
}
									/*}}}*/
// AcqMethod::Status - Send a status message				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Status(const char *Format,...)
{
   string CurrentURI = "<UNKNOWN>";
   if (Queue != 0)
      CurrentURI = Queue->Uri;
   
   va_list args;
   va_start(args,Format);

   // sprintf the description
   char S[1024];
   unsigned int Len = snprintf(S,sizeof(S)-4,"102 Status\nURI: %s\n"
			       "Message: ",CurrentURI.c_str());

   vsnprintf(S+Len,sizeof(S)-4-Len,Format,args);
   strcat(S,"\n\n");
   
   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);
}
									/*}}}*/

// AcqMethod::FetchResult::FetchResult - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcqMethod::FetchResult::FetchResult() : LastModified(0),
                                   IMSHit(false), Size(0), ResumePoint(0)
{
}
									/*}}}*/
// AcqMethod::FetchResult::TakeHashes - Load hashes			/*{{{*/
// ---------------------------------------------------------------------
/* This hides the number of hashes we are supporting from the caller. 
   It just deals with the hash class. */
void pkgAcqMethod::FetchResult::TakeHashes(Hashes &Hash)
{
   MD5Sum = Hash.MD5.Result();
   SHA1Sum = Hash.SHA1.Result();
   SHA256Sum = Hash.SHA256.Result();
}
									/*}}}*/
