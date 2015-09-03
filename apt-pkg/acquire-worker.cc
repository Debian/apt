// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-worker.cc,v 1.34 2001/05/22 04:42:54 jgg Exp $
/* ######################################################################

   Acquire Worker 

   The worker process can startup either as a Configuration prober
   or as a queue runner. As a configuration prober it only reads the
   configuration message and 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>

#include <string>
#include <vector>
#include <iostream>

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sstream>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// Worker::Worker - Constructor for Queue startup			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Worker::Worker(Queue *Q,MethodConfig *Cnf,
			   pkgAcquireStatus *log) : d(NULL), Log(log)
{
   OwnerQ = Q;
   Config = Cnf;
   Access = Cnf->Access;
   CurrentItem = 0;
   TotalSize = 0;
   CurrentSize = 0;

   Construct();
}
									/*}}}*/
// Worker::Worker - Constructor for method config startup		/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Worker::Worker(MethodConfig *Cnf) : d(NULL), OwnerQ(NULL), Config(Cnf),
						Access(Cnf->Access), CurrentItem(NULL),
						CurrentSize(0), TotalSize(0)
{
   Construct();
}
									/*}}}*/
// Worker::Construct - Constructor helper				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Worker::Construct()
{
   NextQueue = 0;
   NextAcquire = 0;
   Process = -1;
   InFd = -1;
   OutFd = -1;
   OutReady = false;
   InReady = false;
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
   {
      /* Closing of stdin is the signal to exit and die when the process
         indicates it needs cleanup */
      if (Config->NeedsCleanup == false)
	 kill(Process,SIGINT);
      ExecWait(Process,Access.c_str(),true);
   }   
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
   {
      _error->Error(_("The method driver %s could not be found."),Method.c_str());
      if (Access == "https")
	 _error->Notice(_("Is the package %s installed?"), "apt-transport-https");
      return false;
   }

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
   for (int I = 0; I != 4; I++)
      SetCloseExec(Pipes[I],true);

   // Fork off the process
   Process = ExecFork();
   if (Process == 0)
   {
      // Setup the FDs
      dup2(Pipes[1],STDOUT_FILENO);
      dup2(Pipes[2],STDIN_FILENO);
      SetCloseExec(STDOUT_FILENO,false);
      SetCloseExec(STDIN_FILENO,false);
      SetCloseExec(STDERR_FILENO,false);

      const char *Args[2];
      Args[0] = Method.c_str();
      Args[1] = 0;
      execv(Args[0],(char **)Args);
      cerr << "Failed to exec method " << Args[0] << endl;
      _exit(100);
   }

   // Fix up our FDs
   InFd = Pipes[0];
   OutFd = Pipes[3];
   SetNonBlock(Pipes[0],true);
   SetNonBlock(Pipes[3],true);
   close(Pipes[1]);
   close(Pipes[2]);
   OutReady = false;
   InReady = true;

   // Read the configuration data
   if (WaitFd(InFd) == false ||
       ReadMessages() == false)
      return _error->Error(_("Method %s did not start correctly"),Method.c_str());

   RunMessages();
   if (OwnerQ != 0)
      SendConfiguration();

   return true;
}
									/*}}}*/
// Worker::ReadMessages - Read all pending messages into the list	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Worker::ReadMessages()
{
   if (::ReadMessages(InFd,MessageQueue) == false)
      return MethodFailure();
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

      if (Debug == true)
	 clog << " <- " << Access << ':' << QuoteString(Message,"\n") << endl;

      // Fetch the message number
      char *End;
      int Number = strtol(Message.c_str(),&End,10);
      if (End == Message.c_str())
	 return _error->Error("Invalid message from method %s: %s",Access.c_str(),Message.c_str());

      string URI = LookupTag(Message,"URI");
      pkgAcquire::Queue::QItem *Itm = NULL;
      if (URI.empty() == false)
	 Itm = OwnerQ->FindItem(URI,this);

      if (Itm != NULL)
      {
	 // update used mirror
	 string UsedMirror = LookupTag(Message,"UsedMirror", "");
	 if (UsedMirror.empty() == false)
	 {
	    for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
	       (*O)->UsedMirror = UsedMirror;

	    if (Itm->Description.find(" ") != string::npos)
	       Itm->Description.replace(0, Itm->Description.find(" "), UsedMirror);
	 }
      }

      // Determine the message number and dispatch
      switch (Number)
      {
	 // 100 Capabilities
	 case 100:
	 if (Capabilities(Message) == false)
	    return _error->Error("Unable to process Capabilities message from %s",Access.c_str());
	 break;

	 // 101 Log
	 case 101:
	 if (Debug == true)
	    clog << " <- (log) " << LookupTag(Message,"Message") << endl;
	 break;

	 // 102 Status
	 case 102:
	 Status = LookupTag(Message,"Message");
	 break;

         // 103 Redirect
         case 103:
         {
            if (Itm == 0)
            {
               _error->Error("Method gave invalid 103 Redirect message");
               break;
            }

	    std::string const NewURI = LookupTag(Message,"New-URI",URI.c_str());
            Itm->URI = NewURI;

	    ItemDone();

	    // Change the status so that it can be dequeued
	    for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
	       (*O)->Status = pkgAcquire::Item::StatIdle;
	    // Mark the item as done (taking care of all queues)
	    // and then put it in the main queue again
	    std::vector<Item*> const ItmOwners = Itm->Owners;
	    OwnerQ->ItemDone(Itm);
	    Itm = NULL;
	    for (pkgAcquire::Queue::QItem::owner_iterator O = ItmOwners.begin(); O != ItmOwners.end(); ++O)
	    {
	       pkgAcquire::Item *Owner = *O;
	       pkgAcquire::ItemDesc &desc = Owner->GetItemDesc();
	       // if we change site, treat it as a mirror change
	       if (URI::SiteOnly(NewURI) != URI::SiteOnly(desc.URI))
	       {
		  std::string const OldSite = desc.Description.substr(0, desc.Description.find(" "));
		  if (likely(APT::String::Startswith(desc.URI, OldSite)))
		  {
		     std::string const OldExtra = desc.URI.substr(OldSite.length() + 1);
		     if (likely(APT::String::Endswith(NewURI, OldExtra)))
		     {
			std::string const NewSite = NewURI.substr(0, NewURI.length() - OldExtra.length());
			Owner->UsedMirror = URI::ArchiveOnly(NewSite);
			if (desc.Description.find(" ") != string::npos)
			   desc.Description.replace(0, desc.Description.find(" "), Owner->UsedMirror);
		     }
		  }
	       }
	       desc.URI = NewURI;
	       OwnerQ->Owner->Enqueue(desc);

	       if (Log != 0)
		  Log->Done(desc);
	    }
            break;
         }

	 // 200 URI Start
	 case 200:
	 {
	    if (Itm == 0)
	    {
	       _error->Error("Method gave invalid 200 URI Start message");
	       break;
	    }

	    CurrentItem = Itm;
	    CurrentSize = 0;
	    TotalSize = strtoull(LookupTag(Message,"Size","0").c_str(), NULL, 10);
	    ResumePoint = strtoull(LookupTag(Message,"Resume-Point","0").c_str(), NULL, 10);
	    for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
	    {
	       (*O)->Start(Message, TotalSize);

	       // Display update before completion
	       if (Log != 0)
	       {
		  if (Log->MorePulses == true)
		     Log->Pulse((*O)->GetOwner());
		  Log->Fetch((*O)->GetItemDesc());
	       }
	    }

	    break;
	 }

	 // 201 URI Done
	 case 201:
	 {
	    if (Itm == 0)
	    {
	       _error->Error("Method gave invalid 201 URI Done message");
	       break;
	    }

	    PrepareFiles("201::URIDone", Itm);

	    // Display update before completion
	    if (Log != 0 && Log->MorePulses == true)
	       for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
		  Log->Pulse((*O)->GetOwner());

	    HashStringList ReceivedHashes;
	    {
	       std::string const givenfilename = LookupTag(Message, "Filename");
	       std::string const filename = givenfilename.empty() ? Itm->Owner->DestFile : givenfilename;
	       // see if we got hashes to verify
	       for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
	       {
		  std::string const tagname = std::string(*type) + "-Hash";
		  std::string const hashsum = LookupTag(Message, tagname.c_str());
		  if (hashsum.empty() == false)
		     ReceivedHashes.push_back(HashString(*type, hashsum));
	       }
	       // not all methods always sent Hashes our way
	       if (ReceivedHashes.usable() == false)
	       {
		  HashStringList const ExpectedHashes = Itm->GetExpectedHashes();
		  if (ExpectedHashes.usable() == true && RealFileExists(filename))
		  {
		     Hashes calc(ExpectedHashes);
		     FileFd file(filename, FileFd::ReadOnly, FileFd::None);
		     calc.AddFD(file);
		     ReceivedHashes = calc.GetHashStringList();
		  }
	       }

	       // only local files can refer other filenames and counting them as fetched would be unfair
	       if (Log != NULL && Itm->Owner->Complete == false && Itm->Owner->Local == false && givenfilename == filename)
		  Log->Fetched(ReceivedHashes.FileSize(),atoi(LookupTag(Message,"Resume-Point","0").c_str()));
	    }

	    std::vector<Item*> const ItmOwners = Itm->Owners;
	    OwnerQ->ItemDone(Itm);
	    Itm = NULL;

	    bool const isIMSHit = StringToBool(LookupTag(Message,"IMS-Hit"),false) ||
	       StringToBool(LookupTag(Message,"Alt-IMS-Hit"),false);
	    for (pkgAcquire::Queue::QItem::owner_iterator O = ItmOwners.begin(); O != ItmOwners.end(); ++O)
	    {
	       pkgAcquire::Item * const Owner = *O;
	       HashStringList const ExpectedHashes = Owner->GetExpectedHashes();
	       if(_config->FindB("Debug::pkgAcquire::Auth", false) == true)
	       {
		  std::clog << "201 URI Done: " << Owner->DescURI() << endl
		     << "ReceivedHash:" << endl;
		  for (HashStringList::const_iterator hs = ReceivedHashes.begin(); hs != ReceivedHashes.end(); ++hs)
		     std::clog <<  "\t- " << hs->toStr() << std::endl;
		  std::clog << "ExpectedHash:" << endl;
		  for (HashStringList::const_iterator hs = ExpectedHashes.begin(); hs != ExpectedHashes.end(); ++hs)
		     std::clog <<  "\t- " << hs->toStr() << std::endl;
		  std::clog << endl;
	       }

	       // decide if what we got is what we expected
	       bool consideredOkay = false;
	       if (ExpectedHashes.usable())
	       {
		  if (ReceivedHashes.usable() == false)
		  {
		     /* IMS-Hits can't be checked here as we will have uncompressed file,
			but the hashes for the compressed file. What we have was good through
			so all we have to ensure later is that we are not stalled. */
		     consideredOkay = isIMSHit;
		  }
		  else if (ReceivedHashes == ExpectedHashes)
		     consideredOkay = true;
		  else
		     consideredOkay = false;

	       }
	       else if (Owner->HashesRequired() == true)
		  consideredOkay = false;
	       else
	       {
		  consideredOkay = true;
		  // even if the hashes aren't usable to declare something secure
		  // we can at least use them to declare it an integrity failure
		  if (ExpectedHashes.empty() == false && ReceivedHashes != ExpectedHashes && _config->Find("Acquire::ForceHash").empty())
		     consideredOkay = false;
	       }

	       if (consideredOkay == true)
		  consideredOkay = Owner->VerifyDone(Message, Config);
	       else // hashsum mismatch
		  Owner->Status = pkgAcquire::Item::StatAuthError;

	       if (consideredOkay == true)
	       {
		  Owner->Done(Message, ReceivedHashes, Config);
		  if (Log != 0)
		  {
		     if (isIMSHit)
			Log->IMSHit(Owner->GetItemDesc());
		     else
			Log->Done(Owner->GetItemDesc());
		  }
	       }
	       else
	       {
		  Owner->Failed(Message,Config);
		  if (Log != 0)
		     Log->Fail(Owner->GetItemDesc());
	       }
	    }
	    ItemDone();
	    break;
	 }

	 // 400 URI Failure
	 case 400:
	 {
	    if (Itm == 0)
	    {
	       std::string const msg = LookupTag(Message,"Message");
	       _error->Error("Method gave invalid 400 URI Failure message: %s", msg.c_str());
	       break;
	    }

	    PrepareFiles("400::URIFailure", Itm);

	    // Display update before completion
	    if (Log != 0 && Log->MorePulses == true)
	       for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
		  Log->Pulse((*O)->GetOwner());

	    std::vector<Item*> const ItmOwners = Itm->Owners;
	    OwnerQ->ItemDone(Itm);
	    Itm = NULL;

	    for (pkgAcquire::Queue::QItem::owner_iterator O = ItmOwners.begin(); O != ItmOwners.end(); ++O)
	    {
	       // set some status
	       if(LookupTag(Message,"FailReason") == "Timeout" ||
		     LookupTag(Message,"FailReason") == "TmpResolveFailure" ||
		     LookupTag(Message,"FailReason") == "ResolveFailure" ||
		     LookupTag(Message,"FailReason") == "ConnectionRefused")
		  (*O)->Status = pkgAcquire::Item::StatTransientNetworkError;

	       (*O)->Failed(Message,Config);

	       if (Log != 0)
		  Log->Fail((*O)->GetItemDesc());
	    }
	    ItemDone();

	    break;
	 }

	 // 401 General Failure
	 case 401:
	 _error->Error("Method %s General failure: %s",Access.c_str(),LookupTag(Message,"Message").c_str());
	 break;

	 // 403 Media Change
	 case 403:
	 MediaChange(Message);
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
   Config->Pipeline = StringToBool(LookupTag(Message,"Pipeline"),false);
   Config->SendConfig = StringToBool(LookupTag(Message,"Send-Config"),false);
   Config->LocalOnly = StringToBool(LookupTag(Message,"Local-Only"),false);
   Config->NeedsCleanup = StringToBool(LookupTag(Message,"Needs-Cleanup"),false);
   Config->Removable = StringToBool(LookupTag(Message,"Removable"),false);

   // Some debug text
   if (Debug == true)
   {
      clog << "Configured access method " << Config->Access << endl;
      clog << "Version:" << Config->Version <<
	      " SingleInstance:" << Config->SingleInstance <<
	      " Pipeline:" << Config->Pipeline <<
	      " SendConfig:" << Config->SendConfig <<
	      " LocalOnly: " << Config->LocalOnly <<
	      " NeedsCleanup: " << Config->NeedsCleanup <<
	      " Removable: " << Config->Removable << endl;
   }

   return true;
}
									/*}}}*/
// Worker::MediaChange - Request a media change				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Worker::MediaChange(string Message)
{
   int status_fd = _config->FindI("APT::Status-Fd",-1);
   if(status_fd > 0)
   {
      string Media = LookupTag(Message,"Media");
      string Drive = LookupTag(Message,"Drive");
      ostringstream msg,status;
      ioprintf(msg,_("Please insert the disc labeled: "
		     "'%s' "
		     "in the drive '%s' and press [Enter]."),
	       Media.c_str(),Drive.c_str());
      status << "media-change: "  // message
	     << Media  << ":"     // media
	     << Drive  << ":"     // drive
	     << msg.str()         // l10n message
	     << endl;

      std::string const dlstatus = status.str();
      FileFd::Write(status_fd, dlstatus.c_str(), dlstatus.size());
   }

   if (Log == 0 || Log->MediaChange(LookupTag(Message,"Media"),
				    LookupTag(Message,"Drive")) == false)
   {
      char S[300];
      snprintf(S,sizeof(S),"603 Media Changed\nFailed: true\n\n");
      if (Debug == true)
	 clog << " -> " << Access << ':' << QuoteString(S,"\n") << endl;
      OutQueue += S;
      OutReady = true;
      return true;
   }

   char S[300];
   snprintf(S,sizeof(S),"603 Media Changed\n\n");
   if (Debug == true)
      clog << " -> " << Access << ':' << QuoteString(S,"\n") << endl;
   OutQueue += S;
   OutReady = true;
   return true;
}
									/*}}}*/
// Worker::SendConfiguration - Send the config to the method		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Worker::SendConfiguration()
{
   if (Config->SendConfig == false)
      return true;

   if (OutFd == -1)
      return false;

   /* Write out all of the configuration directives by walking the
      configuration tree */
   std::ostringstream Message;
   Message << "601 Configuration\n";
   _config->Dump(Message, NULL, "Config-Item: %F=%V\n", false);
   Message << '\n';

   if (Debug == true)
      clog << " -> " << Access << ':' << QuoteString(Message.str(),"\n") << endl;
   OutQueue += Message.str();
   OutReady = true;

   return true;
}
									/*}}}*/
// Worker::QueueItem - Add an item to the outbound queue		/*{{{*/
// ---------------------------------------------------------------------
/* Send a URI Acquire message to the method */
bool pkgAcquire::Worker::QueueItem(pkgAcquire::Queue::QItem *Item)
{
   if (OutFd == -1)
      return false;

   string Message = "600 URI Acquire\n";
   Message.reserve(300);
   Message += "URI: " + Item->URI;
   Message += "\nFilename: " + Item->Owner->DestFile;

   HashStringList const hsl = Item->GetExpectedHashes();
   for (HashStringList::const_iterator hs = hsl.begin(); hs != hsl.end(); ++hs)
      Message += "\nExpected-" + hs->HashType() + ": " + hs->HashValue();

   if (hsl.FileSize() == 0)
   {
      unsigned long long FileSize = Item->GetMaximumSize();
      if(FileSize > 0)
      {
	 string MaximumSize;
	 strprintf(MaximumSize, "%llu", FileSize);
	 Message += "\nMaximum-Size: " + MaximumSize;
      }
   }

   Item->SyncDestinationFiles();
   Message += Item->Custom600Headers();
   Message += "\n\n";

   if (RealFileExists(Item->Owner->DestFile))
   {
      std::string SandboxUser = _config->Find("APT::Sandbox::User");
      ChangeOwnerAndPermissionOfFile("Item::QueueURI", Item->Owner->DestFile.c_str(),
                                     SandboxUser.c_str(), "root", 0600);
   }

   if (Debug == true)
      clog << " -> " << Access << ':' << QuoteString(Message,"\n") << endl;
   OutQueue += Message;
   OutReady = true;

   return true;
}
									/*}}}*/
// Worker::OutFdRead - Out bound FD is ready				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Worker::OutFdReady()
{
   int Res;
   do
   {
      Res = write(OutFd,OutQueue.c_str(),OutQueue.length());
   }
   while (Res < 0 && errno == EINTR);

   if (Res <= 0)
      return MethodFailure();

   OutQueue.erase(0,Res);
   if (OutQueue.empty() == true)
      OutReady = false;

   return true;
}
									/*}}}*/
// Worker::InFdRead - In bound FD is ready				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Worker::InFdReady()
{
   if (ReadMessages() == false)
      return false;
   RunMessages();
   return true;
}
									/*}}}*/
// Worker::MethodFailure - Called when the method fails			/*{{{*/
// ---------------------------------------------------------------------
/* This is called when the method is believed to have failed, probably because
   read returned -1. */
bool pkgAcquire::Worker::MethodFailure()
{
   _error->Error("Method %s has died unexpectedly!",Access.c_str());

   // do not reap the child here to show meaningfull error to the user
   ExecWait(Process,Access.c_str(),false);
   Process = -1;
   close(InFd);
   close(OutFd);
   InFd = -1;
   OutFd = -1;
   OutReady = false;
   InReady = false;
   OutQueue = string();
   MessageQueue.erase(MessageQueue.begin(),MessageQueue.end());

   return false;
}
									/*}}}*/
// Worker::Pulse - Called periodically					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Worker::Pulse()
{
   if (CurrentItem == 0)
      return;

   struct stat Buf;
   if (stat(CurrentItem->Owner->DestFile.c_str(),&Buf) != 0)
      return;
   CurrentSize = Buf.st_size;
}
									/*}}}*/
// Worker::ItemDone - Called when the current item is finished		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Worker::ItemDone()
{
   CurrentItem = 0;
   CurrentSize = 0;
   TotalSize = 0;
   Status = string();
}
									/*}}}*/
void pkgAcquire::Worker::PrepareFiles(char const * const caller, pkgAcquire::Queue::QItem const * const Itm)/*{{{*/
{
   if (RealFileExists(Itm->Owner->DestFile))
   {
      ChangeOwnerAndPermissionOfFile(caller, Itm->Owner->DestFile.c_str(), "root", "root", 0644);
      std::string const filename = Itm->Owner->DestFile;
      for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
      {
	 pkgAcquire::Item const * const Owner = *O;
	 if (Owner->DestFile == filename)
	    continue;
	 unlink(Owner->DestFile.c_str());
	 if (link(filename.c_str(), Owner->DestFile.c_str()) != 0)
	 {
	    // different mounts can't happen for us as we download to lists/ by default,
	    // but if the system is reused by others the locations can potentially be on
	    // different disks, so use symlink as poor-men replacement.
	    // FIXME: Real copying as last fallback, but that is costly, so offload to a method preferable
	    if (symlink(filename.c_str(), Owner->DestFile.c_str()) != 0)
	       _error->Error("Can't create (sym)link of file %s to %s", filename.c_str(), Owner->DestFile.c_str());
	 }
      }
   }
   else
   {
      for (pkgAcquire::Queue::QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
	 unlink((*O)->DestFile.c_str());
   }
}
									/*}}}*/
