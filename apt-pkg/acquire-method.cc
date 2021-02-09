// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <config.h>

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
									/*}}}*/

using namespace std;

// poor mans unordered_map::try_emplace for C++11 as it is a C++17 feature /*{{{*/
template <typename Arg>
static void try_emplace(std::unordered_map<std::string, std::string> &fields, std::string &&name, Arg &&value)
{
   if (fields.find(name) == fields.end())
      fields.emplace(std::move(name), std::forward<Arg>(value));
}
									/*}}}*/

// AcqMethod::pkgAcqMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* This constructs the initialization text */
pkgAcqMethod::pkgAcqMethod(const char *Ver,unsigned long Flags)
{
   std::unordered_map<std::string, std::string> fields;
   try_emplace(fields, "Version", Ver);
   if ((Flags & SingleInstance) == SingleInstance)
      try_emplace(fields, "Single-Instance", "true");

   if ((Flags & Pipeline) == Pipeline)
      try_emplace(fields, "Pipeline", "true");

   if ((Flags & SendConfig) == SendConfig)
      try_emplace(fields, "Send-Config", "true");

   if ((Flags & LocalOnly) == LocalOnly)
      try_emplace(fields, "Local-Only", "true");

   if ((Flags & NeedsCleanup) == NeedsCleanup)
      try_emplace(fields, "Needs-Cleanup", "true");

   if ((Flags & Removable) == Removable)
      try_emplace(fields, "Removable", "true");

   if ((Flags & AuxRequests) == AuxRequests)
      try_emplace(fields, "AuxRequests", "true");

   if ((Flags & SendURIEncoded) == SendURIEncoded)
      try_emplace(fields, "Send-URI-Encoded", "true");

   SendMessage("100 Capabilities", std::move(fields));

   SetNonBlock(STDIN_FILENO,true);

   Queue = 0;
   QueueBack = 0;
}
									/*}}}*/
void pkgAcqMethod::SendMessage(std::string const &header, std::unordered_map<std::string, std::string> &&fields) /*{{{*/
{
   auto CheckKey = [](std::string const &str) {
      // Space, hyphen-minus, and alphanum are allowed for keys/headers.
      return str.find_first_not_of(" -0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos;
   };

   auto CheckValue = [](std::string const &str) {
      return std::all_of(str.begin(), str.end(), [](unsigned char c) -> bool {
	 return c > 127			   // unicode
		|| (c > 31 && c < 127)     // printable chars
		|| c == '\n' || c == '\t'; // special whitespace
      });
   };

   auto Error = [this]() {
      _error->Error("SECURITY: Message contains control characters, rejecting.");
      _error->DumpErrors();
      SendMessage("400 URI Failure", {{"URI", "<UNKNOWN>"}, {"Message", "SECURITY: Message contains control characters, rejecting."}});
      abort();
   };

   if (!CheckKey(header))
      return Error();

   for (auto const &f : fields)
   {
      if (!CheckKey(f.first))
	 return Error();
      if (!CheckValue(f.second))
	 return Error();
   }

   std::cout << header << '\n';
   for (auto const &f : fields)
   {
      if (f.second.empty())
	 continue;
      std::cout << f.first << ": ";
      auto const lines = VectorizeString(f.second, '\n');
      if (likely(lines.empty() == false))
      {
	 std::copy(lines.begin(), std::prev(lines.end()), std::ostream_iterator<std::string>(std::cout, "\n "));
	 std::cout << *lines.rbegin();
      }
      std::cout << '\n';
   }
   std::cout << '\n'
	     << std::flush;
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(bool Transient)
{

   Fail("", Transient);
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
void pkgAcqMethod::Fail(string Err, bool Transient)
{

   if (not _error->empty())
   {
      while (not _error->empty())
      {
	 std::string msg;
	 if (_error->PopMessage(msg))
	 {
	    if (not Err.empty())
	       Err.append("\n");
	    Err.append(msg);
	 }
      }
   }
   if (Err.empty())
      Err = "Undetermined Error";

   // Strip out junk from the error messages
   std::transform(Err.begin(), Err.end(), Err.begin(), [](char const c) {
      if (c == '\r' || c == '\n')
	 return ' ';
      return c;
   });
   if (IP.empty() == false && _config->FindB("Acquire::Failure::ShowIP", true) == true)
      Err.append(" ").append(IP);

   std::unordered_map<std::string, std::string> fields;
   if (Queue != nullptr)
      try_emplace(fields, "URI", Queue->Uri);
   else
      try_emplace(fields, "URI", "<UNKNOWN>");
   try_emplace(fields, "Message", Err);

   if(FailReason.empty() == false)
      try_emplace(fields, "FailReason", FailReason);
   if (UsedMirror.empty() == false)
      try_emplace(fields, "UsedMirror", UsedMirror);
   if (Transient == true)
      try_emplace(fields, "Transient-Failure", "true");

   SendMessage("400 URI Failure", std::move(fields));

   if (Queue != nullptr)
      Dequeue();
}
									/*}}}*/
// AcqMethod::DropPrivsOrDie - Drop privileges or die		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::DropPrivsOrDie()
{
   if (!DropPrivileges()) {
      Fail(false);
      exit(112);	/* call the european emergency number */
   }
}

									/*}}}*/
// AcqMethod::URIStart - Indicate a download is starting		/*{{{*/
void pkgAcqMethod::URIStart(FetchResult &Res)
{
   if (Queue == 0)
      abort();

   std::unordered_map<std::string, std::string> fields;
   try_emplace(fields, "URI", Queue->Uri);
   if (Res.Size != 0)
      try_emplace(fields, "Size", std::to_string(Res.Size));
   if (Res.LastModified != 0)
      try_emplace(fields, "Last-Modified", TimeRFC1123(Res.LastModified, true));
   if (Res.ResumePoint != 0)
      try_emplace(fields, "Resume-Point", std::to_string(Res.ResumePoint));
   if (UsedMirror.empty() == false)
      try_emplace(fields, "UsedMirror", UsedMirror);

   SendMessage("200 URI Start", std::move(fields));
}
									/*}}}*/
// AcqMethod::URIDone - A URI is finished				/*{{{*/
static void printHashStringList(std::unordered_map<std::string, std::string> &fields, std::string const &Prefix, HashStringList const &list)
{
   for (auto const &hash : list)
   {
      // very old compatibility name for MD5Sum
      if (hash.HashType() == "MD5Sum")
	 try_emplace(fields, Prefix + "MD5-Hash", hash.HashValue());
      try_emplace(fields, Prefix + hash.HashType() + "-Hash", hash.HashValue());
   }
}
void pkgAcqMethod::URIDone(FetchResult &Res, FetchResult *Alt)
{
   if (Queue == 0)
      abort();

   std::unordered_map<std::string, std::string> fields;
   try_emplace(fields, "URI", Queue->Uri);
   if (Res.Filename.empty() == false)
      try_emplace(fields, "Filename", Res.Filename);
   if (Res.Size != 0)
      try_emplace(fields, "Size", std::to_string(Res.Size));
   if (Res.LastModified != 0)
      try_emplace(fields, "Last-Modified", TimeRFC1123(Res.LastModified, true));
   printHashStringList(fields, "", Res.Hashes);

   if (UsedMirror.empty() == false)
      try_emplace(fields, "UsedMirror", UsedMirror);
   if (Res.GPGVOutput.empty() == false)
   {
      std::ostringstream os;
      std::copy(Res.GPGVOutput.begin(), Res.GPGVOutput.end() - 1, std::ostream_iterator<std::string>(os, "\n"));
      os << *Res.GPGVOutput.rbegin();
      try_emplace(fields, "GPGVOutput", os.str());
   }
   if (Res.ResumePoint != 0)
      try_emplace(fields, "Resume-Point", std::to_string(Res.ResumePoint));
   if (Res.IMSHit == true)
      try_emplace(fields, "IMS-Hit", "true");

   if (Alt != nullptr)
   {
      if (Alt->Filename.empty() == false)
	 try_emplace(fields, "Alt-Filename", Alt->Filename);
      if (Alt->Size != 0)
	 try_emplace(fields, "Alt-Size", std::to_string(Alt->Size));
      if (Alt->LastModified != 0)
	 try_emplace(fields, "Alt-Last-Modified", TimeRFC1123(Alt->LastModified, true));
      if (Alt->IMSHit == true)
	 try_emplace(fields, "Alt-IMS-Hit", "true");
      printHashStringList(fields, "Alt-", Alt->Hashes);
   }

   SendMessage("201 URI Done", std::move(fields));
   Dequeue();
}
									/*}}}*/
// AcqMethod::MediaFail - Synchronous request for new media		/*{{{*/
// ---------------------------------------------------------------------
/* This sends a 403 Media Failure message to the APT and waits for it
   to be ackd */
bool pkgAcqMethod::MediaFail(string Required,string Drive)
{
   fprintf(stdout, "403 Media Failure\nMedia: %s\nDrive: %s\n",
	    Required.c_str(),Drive.c_str());
   std::cout << "\n" << std::flush;

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
      const char *Equals = (const char*) memchr(I, '=', MsgEnd - I);
      if (Equals == NULL)
	 return false;
      const char *End = (const char*) memchr(Equals, '\n', MsgEnd - Equals);
      if (End == NULL)
	 End = MsgEnd;
      
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
	    Tmp->Proxy(LookupTag(Message, "Proxy"));
	    Tmp->DestFile = LookupTag(Message,"FileName");
	    if (RFC1123StrToTime(LookupTag(Message,"Last-Modified"),Tmp->LastModified) == false)
	       Tmp->LastModified = 0;
	    Tmp->IndexFile = StringToBool(LookupTag(Message,"Index-File"),false);
	    Tmp->FailIgnore = StringToBool(LookupTag(Message,"Fail-Ignore"),false);
	    Tmp->ExpectedHashes = HashStringList();
	    for (char const * const * t = HashString::SupportedHashes(); *t != NULL; ++t)
	    {
	       std::string tag = "Expected-";
	       tag.append(*t);
	       std::string const hash = LookupTag(Message, tag.c_str());
	       if (hash.empty() == false)
		  Tmp->ExpectedHashes.push_back(HashString(*t, hash));
	    }
            char *End;
	    if (Tmp->ExpectedHashes.FileSize() > 0)
	       Tmp->MaximumSize = Tmp->ExpectedHashes.FileSize();
	    else
	       Tmp->MaximumSize = strtoll(LookupTag(Message, "Maximum-Size", "0").c_str(), &End, 10);
	    Tmp->Next = 0;
	    
	    // Append it to the list
	    FetchItem **I = &Queue;
	    for (; *I != 0; I = &(*I)->Next);
	    *I = Tmp;
	    if (QueueBack == 0)
	       QueueBack = Tmp;

	    // Notify that this item is to be fetched.
	    if (URIAcquire(Message, Tmp) == false)
	       Fail();

	    break;
	 }
      }
   }

   Exit();
   return 0;
}
									/*}}}*/
// AcqMethod::PrintStatus - privately really send a log/status message	/*{{{*/
void pkgAcqMethod::PrintStatus(char const * const header, const char* Format,
			       va_list &args) const
{
   string CurrentURI = "<UNKNOWN>";
   if (Queue != 0)
      CurrentURI = Queue->Uri;
   if (UsedMirror.empty() == true)
      fprintf(stdout, "%s\nURI: %s\nMessage: ",
	      header, CurrentURI.c_str());
   else
      fprintf(stdout, "%s\nURI: %s\nUsedMirror: %s\nMessage: ",
	      header, CurrentURI.c_str(), UsedMirror.c_str());
   vfprintf(stdout,Format,args);
   std::cout << "\n\n" << std::flush;
}
									/*}}}*/
// AcqMethod::Log - Send a log message					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Log(const char *Format,...)
{
   va_list args;
   ssize_t size = 400;
   std::ostringstream outstr;
   while (true) {
      bool ret;
      va_start(args,Format);
      ret = iovprintf(outstr, Format, args, size);
      va_end(args);
      if (ret == true)
	 break;
   }
   std::unordered_map<std::string, std::string> fields;
   if (Queue != 0)
      try_emplace(fields, "URI", Queue->Uri);
   else
      try_emplace(fields, "URI", "<UNKNOWN>");
   if (not UsedMirror.empty())
      try_emplace(fields, "UsedMirror", UsedMirror);
   try_emplace(fields, "Message", outstr.str());
   SendMessage("101 Log", std::move(fields));
}
									/*}}}*/
// AcqMethod::Status - Send a status message				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Status(const char *Format,...)
{
   va_list args;
   ssize_t size = 400;
   std::ostringstream outstr;
   while (true) {
      bool ret;
      va_start(args,Format);
      ret = iovprintf(outstr, Format, args, size);
      va_end(args);
      if (ret == true)
	 break;
   }
   std::unordered_map<std::string, std::string> fields;
   if (Queue != 0)
      try_emplace(fields, "URI", Queue->Uri);
   else
      try_emplace(fields, "URI", "<UNKNOWN>");
   if (not UsedMirror.empty())
      try_emplace(fields, "UsedMirror", UsedMirror);
   try_emplace(fields, "Message", outstr.str());
   SendMessage("102 Status", std::move(fields));
}
									/*}}}*/
// AcqMethod::Redirect - Send a redirect message                       /*{{{*/
// ---------------------------------------------------------------------
/* This method sends the redirect message and dequeues the item as
 * the worker will enqueue again later on to the right queue */
void pkgAcqMethod::Redirect(const string &NewURI)
{
   if (NewURI.find_first_not_of(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~") != std::string::npos)
   {
      _error->Error("SECURITY: URL redirect target contains control characters, rejecting.");
      Fail();
      return;
   }
   std::unordered_map<std::string, std::string> fields;
   try_emplace(fields, "URI", Queue->Uri);
   try_emplace(fields, "New-URI", NewURI);
   SendMessage("103 Redirect", std::move(fields));
   Dequeue();
}
                                                                        /*}}}*/
// AcqMethod::FetchResult::FetchResult - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcqMethod::FetchResult::FetchResult() : LastModified(0),
                                   IMSHit(false), Size(0), ResumePoint(0), d(NULL)
{
}
									/*}}}*/
// AcqMethod::FetchResult::TakeHashes - Load hashes			/*{{{*/
// ---------------------------------------------------------------------
/* This hides the number of hashes we are supporting from the caller. 
   It just deals with the hash class. */
void pkgAcqMethod::FetchResult::TakeHashes(class Hashes &Hash)
{
   Hashes = Hash.GetHashStringList();
}
									/*}}}*/
void pkgAcqMethod::Dequeue() {						/*{{{*/
   FetchItem const * const Tmp = Queue;
   Queue = Queue->Next;
   if (Tmp == QueueBack)
      QueueBack = Queue;
   delete Tmp;
}
									/*}}}*/
pkgAcqMethod::~pkgAcqMethod() {}

struct pkgAcqMethod::FetchItem::Private
{
   std::string Proxy;
};

pkgAcqMethod::FetchItem::FetchItem() : Next(nullptr), DestFileFd(-1), LastModified(0), IndexFile(false),
				       FailIgnore(false), MaximumSize(0), d(new Private)
{}

std::string pkgAcqMethod::FetchItem::Proxy()
{
   return d->Proxy;
}

void pkgAcqMethod::FetchItem::Proxy(std::string const &Proxy)
{
   d->Proxy = Proxy;
}

pkgAcqMethod::FetchItem::~FetchItem() { delete d; }

pkgAcqMethod::FetchResult::~FetchResult() {}
