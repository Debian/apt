// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.cc,v 1.7 1998/11/11 07:30:54 jgg Exp $
/* ######################################################################

   Acquire Method

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/acquire-method.h"
#endif
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <strutl.h>
#include <apt-pkg/fileutl.h>

#include <stdio.h>
									/*}}}*/

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
   
   if ((Flags & PreScan) == PreScan)
      strcat(End,"Pre-Scan: true\n");
   
   if ((Flags & Pipeline) == Pipeline)
      strcat(End,"Pipeline: true\n");
   
   if ((Flags & SendConfig) == SendConfig)
      strcat(End,"Send-Config: true\n");
   strcat(End,"\n");

   if (write(STDOUT_FILENO,S,strlen(S)) != (signed)strlen(S))
      exit(100);

   SetNonBlock(STDIN_FILENO,true);
   
   Queue = 0;
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail()
{
   string Err = "Undetermined Error";
   if (_error->empty() == false)
      _error->PopMessage(Err);   
   _error->Discard();
   Fail(Err);
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(string Err)
{   
   char S[1024];
   if (Queue != 0)
   {
      snprintf(S,sizeof(S),"400 URI Failure\nURI: %s\n"
	       "Message: %s\n\n",Queue->Uri.c_str(),Err.c_str());
      
      // Dequeue
      FetchItem *Tmp = Queue;
      Queue = Queue->Next;
      delete Tmp;
   }
   else
      snprintf(S,sizeof(S),"400 URI Failure\nURI: <UNKNOWN>\n"
	       "Message: %s\n\n",Err.c_str());
      
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
      End += snprintf(End,sizeof(S) - (End - S),"Size: %u\n",Res.Size);
   
   if (Res.LastModified != 0)
      End += snprintf(End,sizeof(S) - (End - S),"Last-Modified: %s\n",
		      TimeRFC1123(Res.LastModified).c_str());
   
   if (Res.ResumePoint != 0)
      End += snprintf(End,sizeof(S) - (End - S),"Resume-Point: %u\n",
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
      End += snprintf(End,sizeof(S) - (End - S),"Filename: %s\n",Res.Filename.c_str());
   
   if (Res.Size != 0)
      End += snprintf(End,sizeof(S) - (End - S),"Size: %u\n",Res.Size);
   
   if (Res.LastModified != 0)
      End += snprintf(End,sizeof(S) - (End - S),"Last-Modified: %s\n",
		      TimeRFC1123(Res.LastModified).c_str());

   if (Res.MD5Sum.empty() == false)
      End += snprintf(End,sizeof(S) - (End - S),"MD5Sum: %s\n",Res.MD5Sum.c_str());

   if (Res.ResumePoint != 0)
      End += snprintf(End,sizeof(S) - (End - S),"Resume-Point: %u\n",
		      Res.ResumePoint);

   if (Res.IMSHit == true)
      strcat(End,"IMS-Hit: true\n");
   End = S + strlen(S);
   
   if (Alt != 0)
   {
      if (Alt->Filename.empty() == false)
	 End += snprintf(End,sizeof(S) - (End - S),"Alt-Filename: %s\n",Alt->Filename.c_str());
      
      if (Alt->Size != 0)
	 End += snprintf(End,sizeof(S) - (End - S),"Alt-Size: %u\n",Alt->Size);
      
      if (Alt->LastModified != 0)
	 End += snprintf(End,sizeof(S) - (End - S),"Alt-Last-Modified: %s\n",
			 TimeRFC1123(Alt->LastModified).c_str());
      
      if (Alt->MD5Sum.empty() == false)
	 End += snprintf(End,sizeof(S) - (End - S),"Alt-MD5Sum: %s\n",
			 Alt->MD5Sum.c_str());
      
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
}
									/*}}}*/
// AcqMethod::Configuration - Handle the configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* This parses each configuration entry and puts it into the _config 
   Configuration class. */
bool pkgAcqMethod::Configuration(string Message)
{
   ::Configuration &Cnf = *_config;
   
   const char *I = Message.begin();
   
   unsigned int Length = strlen("Config-Item");
   for (; I + Length < Message.end(); I++)
   {
      // Not a config item
      if (I[Length] != ':' || stringcasecmp(I,I+Length,"Config-Item") != 0)
	 continue;
      
      I += Length + 1;
      
      for (; I < Message.end() && *I == ' '; I++);
      const char *Equals = I;
      for (; Equals < Message.end() && *Equals != '='; Equals++);
      const char *End = Equals;
      for (; End < Message.end() && *End != '\n'; End++);
      if (End == Equals)
	 return false;
      
      Cnf.Set(string(I,Equals-I),string(Equals+1,End-Equals-1));
      I = End;
   }
   
   return true;
}
									/*}}}*/
// AcqMethod::Run - Run the message engine				/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgAcqMethod::Run(bool Single)
{
   while (1)
   {
      // Block if the message queue is empty
      if (Messages.empty() == true)
      {
	 if (Single == false)
	    if (WaitFd(STDIN_FILENO) == false)
	       return 0;
      
	 if (ReadMessages(STDIN_FILENO,Messages) == false)
	    return 0;
      }
            
      // Single mode exits if the message queue is empty
      if (Single == true && Messages.empty() == true)
	 return 0;
      
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
	    
	    Tmp->Next = 0;
	    
	    // Append it to the list
	    FetchItem **I = &Queue;
	    for (; *I != 0; I = &(*I)->Next);
	    *I = Tmp;

	    // Notify that this item is to be fetched.
	    if (Fetch(Tmp) == false)
	       Fail();
	    
	    break;					     
	 }   
      }      
   }

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
   unsigned int Len = snprintf(S,sizeof(S),"101 Log\nURI: %s\n"
			       "Message: ",CurrentURI.c_str());

   vsnprintf(S+Len,sizeof(S)-Len,Format,args);
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
   unsigned int Len = snprintf(S,sizeof(S),"102 Status\nURI: %s\n"
			       "Message: ",CurrentURI.c_str());

   vsnprintf(S+Len,sizeof(S)-Len,Format,args);
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

