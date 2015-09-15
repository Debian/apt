// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   HTTP and HTTPS share a lot of common code and these classes are
   exactly the dumping ground for this common code

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "server.h"

#include <apti18n.h>
									/*}}}*/
using namespace std;

string ServerMethod::FailFile;
int ServerMethod::FailFd = -1;
time_t ServerMethod::FailTime = 0;

// ServerState::RunHeaders - Get the headers before the data		/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 if things are OK, 1 if an IO error occurred and 2 if a header
   parse error occurred */
ServerState::RunHeadersResult ServerState::RunHeaders(FileFd * const File,
                                                      const std::string &Uri)
{
   State = Header;
   
   Owner->Status(_("Waiting for headers"));

   Major = 0; 
   Minor = 0; 
   Result = 0; 
   TotalFileSize = 0;
   JunkSize = 0;
   StartPos = 0;
   Encoding = Closes;
   HaveContent = false;
   time(&Date);

   do
   {
      string Data;
      if (ReadHeaderLines(Data) == false)
	 continue;

      if (Owner->Debug == true)
	 clog << "Answer for: " << Uri << endl << Data;
      
      for (string::const_iterator I = Data.begin(); I < Data.end(); ++I)
      {
	 string::const_iterator J = I;
	 for (; J != Data.end() && *J != '\n' && *J != '\r'; ++J);
	 if (HeaderLine(string(I,J)) == false)
	    return RUN_HEADERS_PARSE_ERROR;
	 I = J;
      }

      // 100 Continue is a Nop...
      if (Result == 100)
	 continue;
      
      // Tidy up the connection persistence state.
      if (Encoding == Closes && HaveContent == true)
	 Persistent = false;
      
      return RUN_HEADERS_OK;
   }
   while (LoadNextResponse(false, File) == true);
   
   return RUN_HEADERS_IO_ERROR;
}
									/*}}}*/
// ServerState::HeaderLine - Process a header line			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ServerState::HeaderLine(string Line)
{
   if (Line.empty() == true)
      return true;

   string::size_type Pos = Line.find(' ');
   if (Pos == string::npos || Pos+1 > Line.length())
   {
      // Blah, some servers use "connection:closes", evil.
      Pos = Line.find(':');
      if (Pos == string::npos || Pos + 2 > Line.length())
	 return _error->Error(_("Bad header line"));
      Pos++;
   }

   // Parse off any trailing spaces between the : and the next word.
   string::size_type Pos2 = Pos;
   while (Pos2 < Line.length() && isspace(Line[Pos2]) != 0)
      Pos2++;

   string Tag = string(Line,0,Pos);
   string Val = string(Line,Pos2);

   if (stringcasecmp(Tag.c_str(),Tag.c_str()+4,"HTTP") == 0)
   {
      // Evil servers return no version
      if (Line[4] == '/')
      {
	 int const elements = sscanf(Line.c_str(),"HTTP/%3u.%3u %3u%359[^\n]",&Major,&Minor,&Result,Code);
	 if (elements == 3)
	 {
	    Code[0] = '\0';
	    if (Owner != NULL && Owner->Debug == true)
	       clog << "HTTP server doesn't give Reason-Phrase for " << Result << std::endl;
	 }
	 else if (elements != 4)
	    return _error->Error(_("The HTTP server sent an invalid reply header"));
      }
      else
      {
	 Major = 0;
	 Minor = 9;
	 if (sscanf(Line.c_str(),"HTTP %3u%359[^\n]",&Result,Code) != 2)
	    return _error->Error(_("The HTTP server sent an invalid reply header"));
      }

      /* Check the HTTP response header to get the default persistence
         state. */
      if (Major < 1)
	 Persistent = false;
      else
      {
	 if (Major == 1 && Minor == 0)
	    Persistent = false;
	 else
	    Persistent = true;
      }

      return true;
   }

   if (stringcasecmp(Tag,"Content-Length:") == 0)
   {
      if (Encoding == Closes)
	 Encoding = Stream;
      HaveContent = true;

      unsigned long long * DownloadSizePtr = &DownloadSize;
      if (Result == 416)
	 DownloadSizePtr = &JunkSize;

      *DownloadSizePtr = strtoull(Val.c_str(), NULL, 10);
      if (*DownloadSizePtr >= std::numeric_limits<unsigned long long>::max())
	 return _error->Errno("HeaderLine", _("The HTTP server sent an invalid Content-Length header"));
      else if (*DownloadSizePtr == 0)
	 HaveContent = false;

      // On partial content (206) the Content-Length less than the real
      // size, so do not set it here but leave that to the Content-Range
      // header instead
      if(Result != 206 && TotalFileSize == 0)
         TotalFileSize = DownloadSize;

      return true;
   }

   if (stringcasecmp(Tag,"Content-Type:") == 0)
   {
      HaveContent = true;
      return true;
   }

   if (stringcasecmp(Tag,"Content-Range:") == 0)
   {
      HaveContent = true;

      // §14.16 says 'byte-range-resp-spec' should be a '*' in case of 416
      if (Result == 416 && sscanf(Val.c_str(), "bytes */%llu",&TotalFileSize) == 1)
	 ; // we got the expected filesize which is all we wanted
      else if (sscanf(Val.c_str(),"bytes %llu-%*u/%llu",&StartPos,&TotalFileSize) != 2)
	 return _error->Error(_("The HTTP server sent an invalid Content-Range header"));
      if ((unsigned long long)StartPos > TotalFileSize)
	 return _error->Error(_("This HTTP server has broken range support"));

      // figure out what we will download
      DownloadSize = TotalFileSize - StartPos;
      return true;
   }

   if (stringcasecmp(Tag,"Transfer-Encoding:") == 0)
   {
      HaveContent = true;
      if (stringcasecmp(Val,"chunked") == 0)
	 Encoding = Chunked;
      return true;
   }

   if (stringcasecmp(Tag,"Connection:") == 0)
   {
      if (stringcasecmp(Val,"close") == 0)
	 Persistent = false;
      if (stringcasecmp(Val,"keep-alive") == 0)
	 Persistent = true;
      return true;
   }

   if (stringcasecmp(Tag,"Last-Modified:") == 0)
   {
      if (RFC1123StrToTime(Val.c_str(), Date) == false)
	 return _error->Error(_("Unknown date format"));
      return true;
   }

   if (stringcasecmp(Tag,"Location:") == 0)
   {
      Location = Val;
      return true;
   }

   return true;
}
									/*}}}*/
// ServerState::ServerState - Constructor				/*{{{*/
ServerState::ServerState(URI Srv, ServerMethod *Owner) : ServerName(Srv), TimeOut(120), Owner(Owner)
{
   Reset();
}
									/*}}}*/
bool ServerState::AddPartialFileToHashes(FileFd &File)			/*{{{*/
{
   File.Truncate(StartPos);
   return GetHashes()->AddFD(File, StartPos);
}
									/*}}}*/

bool ServerMethod::Configuration(string Message)			/*{{{*/
{
   if (pkgAcqMethod::Configuration(Message) == false)
      return false;

   DropPrivsOrDie();

   return true;
}
									/*}}}*/

// ServerMethod::DealWithHeaders - Handle the retrieved header data	/*{{{*/
// ---------------------------------------------------------------------
/* We look at the header data we got back from the server and decide what
   to do. Returns DealWithHeadersResult (see http.h for details).
 */
ServerMethod::DealWithHeadersResult
ServerMethod::DealWithHeaders(FetchResult &Res)
{
   // Not Modified
   if (Server->Result == 304)
   {
      unlink(Queue->DestFile.c_str());
      Res.IMSHit = true;
      Res.LastModified = Queue->LastModified;
      return IMS_HIT;
   }

   /* Redirect
    *
    * Note that it is only OK for us to treat all redirection the same
    * because we *always* use GET, not other HTTP methods.  There are
    * three redirection codes for which it is not appropriate that we
    * redirect.  Pass on those codes so the error handling kicks in.
    */
   if (AllowRedirect
       && (Server->Result > 300 && Server->Result < 400)
       && (Server->Result != 300       // Multiple Choices
           && Server->Result != 304    // Not Modified
           && Server->Result != 306))  // (Not part of HTTP/1.1, reserved)
   {
      if (Server->Location.empty() == true);
      else if (Server->Location[0] == '/' && Queue->Uri.empty() == false)
      {
	 URI Uri = Queue->Uri;
	 if (Uri.Host.empty() == false)
            NextURI = URI::SiteOnly(Uri);
	 else
	    NextURI.clear();
	 NextURI.append(DeQuoteString(Server->Location));
	 return TRY_AGAIN_OR_REDIRECT;
      }
      else
      {
	 NextURI = DeQuoteString(Server->Location);
	 URI tmpURI = NextURI;
	 URI Uri = Queue->Uri;
	 // same protocol redirects are okay
	 if (tmpURI.Access == Uri.Access)
	    return TRY_AGAIN_OR_REDIRECT;
	 // as well as http to https
	 else if (Uri.Access == "http" && tmpURI.Access == "https")
	    return TRY_AGAIN_OR_REDIRECT;
      }
      /* else pass through for error message */
   }
   // retry after an invalid range response without partial data
   else if (Server->Result == 416)
   {
      struct stat SBuf;
      if (stat(Queue->DestFile.c_str(),&SBuf) >= 0 && SBuf.st_size > 0)
      {
	 bool partialHit = false;
	 if (Queue->ExpectedHashes.usable() == true)
	 {
	    Hashes resultHashes(Queue->ExpectedHashes);
	    FileFd file(Queue->DestFile, FileFd::ReadOnly);
	    Server->TotalFileSize = file.FileSize();
	    Server->Date = file.ModificationTime();
	    resultHashes.AddFD(file);
	    HashStringList const hashList = resultHashes.GetHashStringList();
	    partialHit = (Queue->ExpectedHashes == hashList);
	 }
	 else if ((unsigned long long)SBuf.st_size == Server->TotalFileSize)
	    partialHit = true;
	 if (partialHit == true)
	 {
	    // the file is completely downloaded, but was not moved
	    if (Server->HaveContent == true)
	    {
	       // Send to error page to dev/null
	       FileFd DevNull("/dev/null",FileFd::WriteExists);
	       Server->RunData(&DevNull);
	    }
	    Server->HaveContent = false;
	    Server->StartPos = Server->TotalFileSize;
	    Server->Result = 200;
	 }
	 else if (unlink(Queue->DestFile.c_str()) == 0)
	 {
	    NextURI = Queue->Uri;
	    return TRY_AGAIN_OR_REDIRECT;
	 }
      }
   }

   /* We have a reply we don't handle. This should indicate a perm server
      failure */
   if (Server->Result < 200 || Server->Result >= 300)
   {
      std::string err;
      strprintf(err, "HttpError%u", Server->Result);
      SetFailReason(err);
      _error->Error("%u %s", Server->Result, Server->Code);
      if (Server->HaveContent == true)
	 return ERROR_WITH_CONTENT_PAGE;
      return ERROR_UNRECOVERABLE;
   }

   // This is some sort of 2xx 'data follows' reply
   Res.LastModified = Server->Date;
   Res.Size = Server->TotalFileSize;
   
   // Open the file
   delete File;
   File = new FileFd(Queue->DestFile,FileFd::WriteAny);
   if (_error->PendingError() == true)
      return ERROR_NOT_FROM_SERVER;

   FailFile = Queue->DestFile;
   FailFile.c_str();   // Make sure we don't do a malloc in the signal handler
   FailFd = File->Fd();
   FailTime = Server->Date;

   if (Server->InitHashes(Queue->ExpectedHashes) == false || Server->AddPartialFileToHashes(*File) == false)
   {
      _error->Errno("read",_("Problem hashing file"));
      return ERROR_NOT_FROM_SERVER;
   }
   if (Server->StartPos > 0)
      Res.ResumePoint = Server->StartPos;

   SetNonBlock(File->Fd(),true);
   return FILE_IS_OPEN;
}
									/*}}}*/
// ServerMethod::SigTerm - Handle a fatal signal			/*{{{*/
// ---------------------------------------------------------------------
/* This closes and timestamps the open file. This is necessary to get
   resume behavoir on user abort */
void ServerMethod::SigTerm(int)
{
   if (FailFd == -1)
      _exit(100);

   struct timeval times[2];
   times[0].tv_sec = FailTime;
   times[1].tv_sec = FailTime;
   times[0].tv_usec = times[1].tv_usec = 0;
   utimes(FailFile.c_str(), times);
   close(FailFd);

   _exit(100);
}
									/*}}}*/
// ServerMethod::Fetch - Fetch an item					/*{{{*/
// ---------------------------------------------------------------------
/* This adds an item to the pipeline. We keep the pipeline at a fixed
   depth. */
bool ServerMethod::Fetch(FetchItem *)
{
   if (Server == 0)
      return true;

   // Queue the requests
   int Depth = -1;
   for (FetchItem *I = Queue; I != 0 && Depth < (signed)PipelineDepth; 
	I = I->Next, Depth++)
   {
      if (Depth >= 0)
      {
	 // If pipelining is disabled, we only queue 1 request
	 if (Server->Pipeline == false)
	    break;
	 // if we have no hashes, do at most one such request
	 // as we can't fixup pipeling misbehaviors otherwise
	 else if (I->ExpectedHashes.usable() == false)
	    break;
      }
      
      // Make sure we stick with the same server
      if (Server->Comp(I->Uri) == false)
	 break;
      if (QueueBack == I)
      {
	 QueueBack = I->Next;
	 SendReq(I);
	 continue;
      }
   }
   
   return true;
}
									/*}}}*/
// ServerMethod::Loop - Main loop					/*{{{*/
int ServerMethod::Loop()
{
   typedef vector<string> StringVector;
   typedef vector<string>::iterator StringVectorIterator;
   map<string, StringVector> Redirected;

   signal(SIGTERM,SigTerm);
   signal(SIGINT,SigTerm);
   
   Server = 0;
   
   int FailCounter = 0;
   while (1)
   {      
      // We have no commands, wait for some to arrive
      if (Queue == 0)
      {
	 if (WaitFd(STDIN_FILENO) == false)
	    return 0;
      }
      
      /* Run messages, we can accept 0 (no message) if we didn't
         do a WaitFd above.. Otherwise the FD is closed. */
      int Result = Run(true);
      if (Result != -1 && (Result != 0 || Queue == 0))
      {
	 if(FailReason.empty() == false ||
	    _config->FindB("Acquire::http::DependOnSTDIN", true) == true)
	    return 100;
	 else
	    return 0;
      }

      if (Queue == 0)
	 continue;
      
      // Connect to the server
      if (Server == 0 || Server->Comp(Queue->Uri) == false)
	 Server = CreateServerState(Queue->Uri);

      /* If the server has explicitly said this is the last connection
         then we pre-emptively shut down the pipeline and tear down 
	 the connection. This will speed up HTTP/1.0 servers a tad
	 since we don't have to wait for the close sequence to
         complete */
      if (Server->Persistent == false)
	 Server->Close();

      // Reset the pipeline
      if (Server->IsOpen() == false)
	 QueueBack = Queue;

      // Connnect to the host
      if (Server->Open() == false)
      {
	 Fail(true);
	 Server = nullptr;
	 continue;
      }

      // Fill the pipeline.
      Fetch(0);
      
      // Fetch the next URL header data from the server.
      switch (Server->RunHeaders(File, Queue->Uri))
      {
	 case ServerState::RUN_HEADERS_OK:
	 break;
	 
	 // The header data is bad
	 case ServerState::RUN_HEADERS_PARSE_ERROR:
	 {
	    _error->Error(_("Bad header data"));
	    Fail(true);
	    RotateDNS();
	    continue;
	 }
	 
	 // The server closed a connection during the header get..
	 default:
	 case ServerState::RUN_HEADERS_IO_ERROR:
	 {
	    FailCounter++;
	    _error->Discard();
	    Server->Close();
	    Server->Pipeline = false;
	    
	    if (FailCounter >= 2)
	    {
	       Fail(_("Connection failed"),true);
	       FailCounter = 0;
	    }
	    
	    RotateDNS();
	    continue;
	 }
      };

      // Decide what to do.
      FetchResult Res;
      Res.Filename = Queue->DestFile;
      switch (DealWithHeaders(Res))
      {
	 // Ok, the file is Open
	 case FILE_IS_OPEN:
	 {
	    URIStart(Res);

	    // Run the data
	    bool Result = true;

            // ensure we don't fetch too much
            // we could do "Server->MaximumSize = Queue->MaximumSize" here
            // but that would break the clever pipeline messup detection
            // so instead we use the size of the biggest item in the queue
            Server->MaximumSize = FindMaximumObjectSizeInQueue();

            if (Server->HaveContent)
	       Result = Server->RunData(File);

	    /* If the server is sending back sizeless responses then fill in
	       the size now */
	    if (Res.Size == 0)
	       Res.Size = File->Size();
	    
	    // Close the file, destroy the FD object and timestamp it
	    FailFd = -1;
	    delete File;
	    File = 0;
	    
	    // Timestamp
	    struct timeval times[2];
	    times[0].tv_sec = times[1].tv_sec = Server->Date;
	    times[0].tv_usec = times[1].tv_usec = 0;
	    utimes(Queue->DestFile.c_str(), times);

	    // Send status to APT
	    if (Result == true)
	    {
	       Hashes * const resultHashes = Server->GetHashes();
	       HashStringList const hashList = resultHashes->GetHashStringList();
	       if (PipelineDepth != 0 && Queue->ExpectedHashes.usable() == true && Queue->ExpectedHashes != hashList)
	       {
		  // we did not get the expected hash… mhhh:
		  // could it be that server/proxy messed up pipelining?
		  FetchItem * BeforeI = Queue;
		  for (FetchItem *I = Queue->Next; I != 0 && I != QueueBack; I = I->Next)
		  {
		     if (I->ExpectedHashes.usable() == true && I->ExpectedHashes == hashList)
		     {
			// yes, he did! Disable pipelining and rewrite queue
			if (Server->Pipeline == true)
			{
			   // FIXME: fake a warning message as we have no proper way of communicating here
			   std::string out;
			   strprintf(out, _("Automatically disabled %s due to incorrect response from server/proxy. (man 5 apt.conf)"), "Acquire::http::PipelineDepth");
			   std::cerr << "W: " << out << std::endl;
			   Server->Pipeline = false;
			   // we keep the PipelineDepth value so that the rest of the queue can be fixed up as well
			}
			Rename(Res.Filename, I->DestFile);
			Res.Filename = I->DestFile;
			BeforeI->Next = I->Next;
			I->Next = Queue;
			Queue = I;
			break;
		     }
		     BeforeI = I;
		  }
	       }
	       Res.TakeHashes(*resultHashes);
	       URIDone(Res);
	    }
	    else
	    {
	       if (Server->IsOpen() == false)
	       {
		  FailCounter++;
		  _error->Discard();
		  Server->Close();
		  
		  if (FailCounter >= 2)
		  {
		     Fail(_("Connection failed"),true);
		     FailCounter = 0;
		  }
		  
		  QueueBack = Queue;
	       }
	       else
               {
                  Server->Close();
		  Fail(true);
               }
	    }
	    break;
	 }
	 
	 // IMS hit
	 case IMS_HIT:
	 {
	    URIDone(Res);
	    break;
	 }
	 
	 // Hard server error, not found or something
	 case ERROR_UNRECOVERABLE:
	 {
	    Fail();
	    break;
	 }
	  
	 // Hard internal error, kill the connection and fail
	 case ERROR_NOT_FROM_SERVER:
	 {
	    delete File;
	    File = 0;

	    Fail();
	    RotateDNS();
	    Server->Close();
	    break;
	 }

	 // We need to flush the data, the header is like a 404 w/ error text
	 case ERROR_WITH_CONTENT_PAGE:
	 {
	    Fail();
	    
	    // Send to content to dev/null
	    File = new FileFd("/dev/null",FileFd::WriteExists);
	    Server->RunData(File);
	    delete File;
	    File = 0;
	    break;
	 }
	 
         // Try again with a new URL
         case TRY_AGAIN_OR_REDIRECT:
         {
            // Clear rest of response if there is content
            if (Server->HaveContent)
            {
               File = new FileFd("/dev/null",FileFd::WriteExists);
               Server->RunData(File);
               delete File;
               File = 0;
            }

            /* Detect redirect loops.  No more redirects are allowed
               after the same URI is seen twice in a queue item. */
            StringVector &R = Redirected[Queue->DestFile];
            bool StopRedirects = false;
            if (R.empty() == true)
               R.push_back(Queue->Uri);
            else if (R[0] == "STOP" || R.size() > 10)
               StopRedirects = true;
            else
            {
               for (StringVectorIterator I = R.begin(); I != R.end(); ++I)
                  if (Queue->Uri == *I)
                  {
                     R[0] = "STOP";
                     break;
                  }
 
               R.push_back(Queue->Uri);
            }
 
            if (StopRedirects == false)
               Redirect(NextURI);
            else
               Fail();
 
            break;
         }

	 default:
	 Fail(_("Internal error"));
	 break;
      }
      
      FailCounter = 0;
   }
   
   return 0;
}
									/*}}}*/
unsigned long long ServerMethod::FindMaximumObjectSizeInQueue() const	/*{{{*/
{
   unsigned long long MaxSizeInQueue = 0;
   for (FetchItem *I = Queue; I != 0 && I != QueueBack; I = I->Next)
      MaxSizeInQueue = std::max(MaxSizeInQueue, I->MaximumSize);
   return MaxSizeInQueue;
}
									/*}}}*/
ServerMethod::ServerMethod(const char *Ver,unsigned long Flags) :	/*{{{*/
   pkgAcqMethod(Ver, Flags), Server(nullptr), File(NULL), PipelineDepth(10),
   AllowRedirect(false), Debug(false)
{
}
									/*}}}*/
