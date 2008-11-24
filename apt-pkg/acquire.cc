// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.cc,v 1.50 2004/03/17 05:17:11 mdz Exp $
/* ######################################################################

   Acquire - File Acquiration

   The core element for the schedual system is the concept of a named
   queue. Each queue is unique and each queue has a name derived from the
   URI. The degree of paralization can be controled by how the queue
   name is derived from the URI.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>

#include <iostream>
#include <sstream>
    
#include <dirent.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
									/*}}}*/

using namespace std;

// Acquire::pkgAcquire - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* We grab some runtime state from the configuration space */
pkgAcquire::pkgAcquire(pkgAcquireStatus *Log) : Log(Log)
{
   Queues = 0;
   Configs = 0;
   Workers = 0;
   ToFetch = 0;
   Running = false;
   
   string Mode = _config->Find("Acquire::Queue-Mode","host");
   if (strcasecmp(Mode.c_str(),"host") == 0)
      QueueMode = QueueHost;
   if (strcasecmp(Mode.c_str(),"access") == 0)
      QueueMode = QueueAccess;   

   Debug = _config->FindB("Debug::pkgAcquire",false);
   
   // This is really a stupid place for this
   struct stat St;
   if (stat((_config->FindDir("Dir::State::lists") + "partial/").c_str(),&St) != 0 ||
       S_ISDIR(St.st_mode) == 0)
      _error->Error(_("Lists directory %spartial is missing."),
		    _config->FindDir("Dir::State::lists").c_str());
   if (stat((_config->FindDir("Dir::Cache::Archives") + "partial/").c_str(),&St) != 0 ||
       S_ISDIR(St.st_mode) == 0)
      _error->Error(_("Archive directory %spartial is missing."),
		    _config->FindDir("Dir::Cache::Archives").c_str());
}
									/*}}}*/
// Acquire::~pkgAcquire	- Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* Free our memory, clean up the queues (destroy the workers) */
pkgAcquire::~pkgAcquire()
{
   Shutdown();
   
   while (Configs != 0)
   {
      MethodConfig *Jnk = Configs;
      Configs = Configs->Next;
      delete Jnk;
   }   
}
									/*}}}*/
// Acquire::Shutdown - Clean out the acquire object			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Shutdown()
{
   while (Items.size() != 0)
   {
      if (Items[0]->Status == Item::StatFetching)
         Items[0]->Status = Item::StatError;
      delete Items[0];
   }

   while (Queues != 0)
   {
      Queue *Jnk = Queues;
      Queues = Queues->Next;
      delete Jnk;
   }   
}
									/*}}}*/
// Acquire::Add - Add a new item					/*{{{*/
// ---------------------------------------------------------------------
/* This puts an item on the acquire list. This list is mainly for tracking
   item status */
void pkgAcquire::Add(Item *Itm)
{
   Items.push_back(Itm);
}
									/*}}}*/
// Acquire::Remove - Remove a item					/*{{{*/
// ---------------------------------------------------------------------
/* Remove an item from the acquire list. This is usually not used.. */
void pkgAcquire::Remove(Item *Itm)
{
   Dequeue(Itm);
   
   for (ItemIterator I = Items.begin(); I != Items.end();)
   {
      if (*I == Itm)
      {
	 Items.erase(I);
	 I = Items.begin();
      }      
      else 
	 I++;
   }
}
									/*}}}*/
// Acquire::Add - Add a worker						/*{{{*/
// ---------------------------------------------------------------------
/* A list of workers is kept so that the select loop can direct their FD
   usage. */
void pkgAcquire::Add(Worker *Work)
{
   Work->NextAcquire = Workers;
   Workers = Work;
}
									/*}}}*/
// Acquire::Remove - Remove a worker					/*{{{*/
// ---------------------------------------------------------------------
/* A worker has died. This can not be done while the select loop is running
   as it would require that RunFds could handling a changing list state and
   it cant.. */
void pkgAcquire::Remove(Worker *Work)
{
   if (Running == true)
      abort();
   
   Worker **I = &Workers;
   for (; *I != 0;)
   {
      if (*I == Work)
	 *I = (*I)->NextAcquire;
      else
	 I = &(*I)->NextAcquire;
   }
}
									/*}}}*/
// Acquire::Enqueue - Queue an URI for fetching				/*{{{*/
// ---------------------------------------------------------------------
/* This is the entry point for an item. An item calls this function when
   it is constructed which creates a queue (based on the current queue
   mode) and puts the item in that queue. If the system is running then
   the queue might be started. */
void pkgAcquire::Enqueue(ItemDesc &Item)
{
   // Determine which queue to put the item in
   const MethodConfig *Config;
   string Name = QueueName(Item.URI,Config);
   if (Name.empty() == true)
      return;

   // Find the queue structure
   Queue *I = Queues;
   for (; I != 0 && I->Name != Name; I = I->Next);
   if (I == 0)
   {
      I = new Queue(Name,this);
      I->Next = Queues;
      Queues = I;
      
      if (Running == true)
	 I->Startup();
   }

   // See if this is a local only URI
   if (Config->LocalOnly == true && Item.Owner->Complete == false)
      Item.Owner->Local = true;
   Item.Owner->Status = Item::StatIdle;
   
   // Queue it into the named queue
   if(I->Enqueue(Item)) 
      ToFetch++;
            
   // Some trace stuff
   if (Debug == true)
   {
      clog << "Fetching " << Item.URI << endl;
      clog << " to " << Item.Owner->DestFile << endl;
      clog << " Queue is: " << Name << endl;
   }
}
									/*}}}*/
// Acquire::Dequeue - Remove an item from all queues			/*{{{*/
// ---------------------------------------------------------------------
/* This is called when an item is finished being fetched. It removes it
   from all the queues */
void pkgAcquire::Dequeue(Item *Itm)
{
   Queue *I = Queues;
   bool Res = false;
   for (; I != 0; I = I->Next)
      Res |= I->Dequeue(Itm);
   
   if (Debug == true)
      clog << "Dequeuing " << Itm->DestFile << endl;
   if (Res == true)
      ToFetch--;
}
									/*}}}*/
// Acquire::QueueName - Return the name of the queue for this URI	/*{{{*/
// ---------------------------------------------------------------------
/* The string returned depends on the configuration settings and the
   method parameters. Given something like http://foo.org/bar it can
   return http://foo.org or http */
string pkgAcquire::QueueName(string Uri,MethodConfig const *&Config)
{
   URI U(Uri);
   
   Config = GetConfig(U.Access);
   if (Config == 0)
      return string();
   
   /* Single-Instance methods get exactly one queue per URI. This is
      also used for the Access queue method  */
   if (Config->SingleInstance == true || QueueMode == QueueAccess)
       return U.Access;

   return U.Access + ':' + U.Host;
}
									/*}}}*/
// Acquire::GetConfig - Fetch the configuration information		/*{{{*/
// ---------------------------------------------------------------------
/* This locates the configuration structure for an access method. If 
   a config structure cannot be found a Worker will be created to
   retrieve it */
pkgAcquire::MethodConfig *pkgAcquire::GetConfig(string Access)
{
   // Search for an existing config
   MethodConfig *Conf;
   for (Conf = Configs; Conf != 0; Conf = Conf->Next)
      if (Conf->Access == Access)
	 return Conf;
   
   // Create the new config class
   Conf = new MethodConfig;
   Conf->Access = Access;
   Conf->Next = Configs;
   Configs = Conf;

   // Create the worker to fetch the configuration
   Worker Work(Conf);
   if (Work.Start() == false)
      return 0;

   /* if a method uses DownloadLimit, we switch to SingleInstance mode */
   if(_config->FindI("Acquire::"+Access+"::DlLimit",0) > 0)
      Conf->SingleInstance = true;
    
   return Conf;
}
									/*}}}*/
// Acquire::SetFds - Deal with readable FDs				/*{{{*/
// ---------------------------------------------------------------------
/* Collect FDs that have activity monitors into the fd sets */
void pkgAcquire::SetFds(int &Fd,fd_set *RSet,fd_set *WSet)
{
   for (Worker *I = Workers; I != 0; I = I->NextAcquire)
   {
      if (I->InReady == true && I->InFd >= 0)
      {
	 if (Fd < I->InFd)
	    Fd = I->InFd;
	 FD_SET(I->InFd,RSet);
      }
      if (I->OutReady == true && I->OutFd >= 0)
      {
	 if (Fd < I->OutFd)
	    Fd = I->OutFd;
	 FD_SET(I->OutFd,WSet);
      }
   }
}
									/*}}}*/
// Acquire::RunFds - Deal with active FDs				/*{{{*/
// ---------------------------------------------------------------------
/* Dispatch active FDs over to the proper workers. It is very important
   that a worker never be erased while this is running! The queue class
   should never erase a worker except during shutdown processing. */
void pkgAcquire::RunFds(fd_set *RSet,fd_set *WSet)
{
   for (Worker *I = Workers; I != 0; I = I->NextAcquire)
   {
      if (I->InFd >= 0 && FD_ISSET(I->InFd,RSet) != 0)
	 I->InFdReady();
      if (I->OutFd >= 0 && FD_ISSET(I->OutFd,WSet) != 0)
	 I->OutFdReady();
   }
}
									/*}}}*/
// Acquire::Run - Run the fetch sequence				/*{{{*/
// ---------------------------------------------------------------------
/* This runs the queues. It manages a select loop for all of the
   Worker tasks. The workers interact with the queues and items to
   manage the actual fetch. */
pkgAcquire::RunResult pkgAcquire::Run(int PulseIntervall)
{
   Running = true;
   
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Startup();
   
   if (Log != 0)
      Log->Start();
   
   bool WasCancelled = false;

   // Run till all things have been acquired
   struct timeval tv;
   tv.tv_sec = 0;
   tv.tv_usec = PulseIntervall; 
   while (ToFetch > 0)
   {
      fd_set RFds;
      fd_set WFds;
      int Highest = 0;
      FD_ZERO(&RFds);
      FD_ZERO(&WFds);
      SetFds(Highest,&RFds,&WFds);
      
      int Res;
      do
      {
	 Res = select(Highest+1,&RFds,&WFds,0,&tv);
      }
      while (Res < 0 && errno == EINTR);
      
      if (Res < 0)
      {
	 _error->Errno("select","Select has failed");
	 break;
      }
	     
      RunFds(&RFds,&WFds);
      if (_error->PendingError() == true)
	 break;
      
      // Timeout, notify the log class
      if (Res == 0 || (Log != 0 && Log->Update == true))
      {
	 tv.tv_usec = PulseIntervall;
	 for (Worker *I = Workers; I != 0; I = I->NextAcquire)
	    I->Pulse();
	 if (Log != 0 && Log->Pulse(this) == false)
	 {
	    WasCancelled = true;
	    break;
	 }
      }      
   }   

   if (Log != 0)
      Log->Stop();
   
   // Shut down the acquire bits
   Running = false;
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Shutdown(false);

   // Shut down the items
   for (ItemIterator I = Items.begin(); I != Items.end(); I++)
      (*I)->Finished(); 
   
   if (_error->PendingError())
      return Failed;
   if (WasCancelled)
      return Cancelled;
   return Continue;
}
									/*}}}*/
// Acquire::Bump - Called when an item is dequeued			/*{{{*/
// ---------------------------------------------------------------------
/* This routine bumps idle queues in hopes that they will be able to fetch
   the dequeued item */
void pkgAcquire::Bump()
{
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Bump();
}
									/*}}}*/
// Acquire::WorkerStep - Step to the next worker			/*{{{*/
// ---------------------------------------------------------------------
/* Not inlined to advoid including acquire-worker.h */
pkgAcquire::Worker *pkgAcquire::WorkerStep(Worker *I)
{
   return I->NextAcquire;
};
									/*}}}*/
// Acquire::Clean - Cleans a directory					/*{{{*/
// ---------------------------------------------------------------------
/* This is a bit simplistic, it looks at every file in the dir and sees
   if it is part of the download set. */
bool pkgAcquire::Clean(string Dir)
{
   DIR *D = opendir(Dir.c_str());   
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());
   
   string StartDir = SafeGetCWD();
   if (chdir(Dir.c_str()) != 0)
   {
      closedir(D);
      return _error->Errno("chdir",_("Unable to change to %s"),Dir.c_str());
   }
   
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,"lock") == 0 ||
	  strcmp(Dir->d_name,"partial") == 0 ||
	  strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;
      
      // Look in the get list
      ItemCIterator I = Items.begin();
      for (; I != Items.end(); I++)
	 if (flNotDir((*I)->DestFile) == Dir->d_name)
	    break;
      
      // Nothing found, nuke it
      if (I == Items.end())
	 unlink(Dir->d_name);
   };
   
   closedir(D);
   if (chdir(StartDir.c_str()) != 0)
      return _error->Errno("chdir",_("Unable to change to %s"),StartDir.c_str());
   return true;   
}
									/*}}}*/
// Acquire::TotalNeeded - Number of bytes to fetch			/*{{{*/
// ---------------------------------------------------------------------
/* This is the total number of bytes needed */
double pkgAcquire::TotalNeeded()
{
   double Total = 0;
   for (ItemCIterator I = ItemsBegin(); I != ItemsEnd(); I++)
      Total += (*I)->FileSize;
   return Total;
}
									/*}}}*/
// Acquire::FetchNeeded - Number of bytes needed to get			/*{{{*/
// ---------------------------------------------------------------------
/* This is the number of bytes that is not local */
double pkgAcquire::FetchNeeded()
{
   double Total = 0;
   for (ItemCIterator I = ItemsBegin(); I != ItemsEnd(); I++)
      if ((*I)->Local == false)
	 Total += (*I)->FileSize;
   return Total;
}
									/*}}}*/
// Acquire::PartialPresent - Number of partial bytes we already have	/*{{{*/
// ---------------------------------------------------------------------
/* This is the number of bytes that is not local */
double pkgAcquire::PartialPresent()
{
  double Total = 0;
   for (ItemCIterator I = ItemsBegin(); I != ItemsEnd(); I++)
      if ((*I)->Local == false)
	 Total += (*I)->PartialSize;
   return Total;
}

// Acquire::UriBegin - Start iterator for the uri list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::UriIterator pkgAcquire::UriBegin()
{
   return UriIterator(Queues);
}
									/*}}}*/
// Acquire::UriEnd - End iterator for the uri list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::UriIterator pkgAcquire::UriEnd()
{
   return UriIterator(0);
}
									/*}}}*/

// Acquire::MethodConfig::MethodConfig - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::MethodConfig::MethodConfig()
{
   SingleInstance = false;
   Pipeline = false;
   SendConfig = false;
   LocalOnly = false;
   Removable = false;
   Next = 0;
}
									/*}}}*/

// Queue::Queue - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::Queue(string Name,pkgAcquire *Owner) : Name(Name), 
            Owner(Owner)
{
   Items = 0;
   Next = 0;
   Workers = 0;
   MaxPipeDepth = 1;
   PipeDepth = 0;
}
									/*}}}*/
// Queue::~Queue - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::~Queue()
{
   Shutdown(true);
   
   while (Items != 0)
   {
      QItem *Jnk = Items;
      Items = Items->Next;
      delete Jnk;
   }
}
									/*}}}*/
// Queue::Enqueue - Queue an item to the queue				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Queue::Enqueue(ItemDesc &Item)
{
   QItem **I = &Items;
   // move to the end of the queue and check for duplicates here
   for (; *I != 0; I = &(*I)->Next)
      if (Item.URI == (*I)->URI) 
      {
	 Item.Owner->Status = Item::StatDone;
	 return false;
      }

   // Create a new item
   QItem *Itm = new QItem;
   *Itm = Item;
   Itm->Next = 0;
   *I = Itm;
   
   Item.Owner->QueueCounter++;   
   if (Items->Next == 0)
      Cycle();
   return true;
}
									/*}}}*/
// Queue::Dequeue - Remove an item from the queue			/*{{{*/
// ---------------------------------------------------------------------
/* We return true if we hit something */
bool pkgAcquire::Queue::Dequeue(Item *Owner)
{
   if (Owner->Status == pkgAcquire::Item::StatFetching)
      return _error->Error("Tried to dequeue a fetching object");
       
   bool Res = false;
   
   QItem **I = &Items;
   for (; *I != 0;)
   {
      if ((*I)->Owner == Owner)
      {
	 QItem *Jnk= *I;
	 *I = (*I)->Next;
	 Owner->QueueCounter--;
	 delete Jnk;
	 Res = true;
      }
      else
	 I = &(*I)->Next;
   }
   
   return Res;
}
									/*}}}*/
// Queue::Startup - Start the worker processes				/*{{{*/
// ---------------------------------------------------------------------
/* It is possible for this to be called with a pre-existing set of
   workers. */
bool pkgAcquire::Queue::Startup()
{
   if (Workers == 0)
   {
      URI U(Name);
      pkgAcquire::MethodConfig *Cnf = Owner->GetConfig(U.Access);
      if (Cnf == 0)
	 return false;
      
      Workers = new Worker(this,Cnf,Owner->Log);
      Owner->Add(Workers);
      if (Workers->Start() == false)
	 return false;
      
      /* When pipelining we commit 10 items. This needs to change when we
         added other source retry to have cycle maintain a pipeline depth
         on its own. */
      if (Cnf->Pipeline == true)
	 MaxPipeDepth = 1000;
      else
	 MaxPipeDepth = 1;
   }
   
   return Cycle();
}
									/*}}}*/
// Queue::Shutdown - Shutdown the worker processes			/*{{{*/
// ---------------------------------------------------------------------
/* If final is true then all workers are eliminated, otherwise only workers
   that do not need cleanup are removed */
bool pkgAcquire::Queue::Shutdown(bool Final)
{
   // Delete all of the workers
   pkgAcquire::Worker **Cur = &Workers;
   while (*Cur != 0)
   {
      pkgAcquire::Worker *Jnk = *Cur;
      if (Final == true || Jnk->GetConf()->NeedsCleanup == false)
      {
	 *Cur = Jnk->NextQueue;
	 Owner->Remove(Jnk);
	 delete Jnk;
      }
      else
	 Cur = &(*Cur)->NextQueue;      
   }
   
   return true;
}
									/*}}}*/
// Queue::FindItem - Find a URI in the item list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::QItem *pkgAcquire::Queue::FindItem(string URI,pkgAcquire::Worker *Owner)
{
   for (QItem *I = Items; I != 0; I = I->Next)
      if (I->URI == URI && I->Worker == Owner)
	 return I;
   return 0;
}
									/*}}}*/
// Queue::ItemDone - Item has been completed				/*{{{*/
// ---------------------------------------------------------------------
/* The worker signals this which causes the item to be removed from the
   queue. If this is the last queue instance then it is removed from the
   main queue too.*/
bool pkgAcquire::Queue::ItemDone(QItem *Itm)
{
   PipeDepth--;
   if (Itm->Owner->Status == pkgAcquire::Item::StatFetching)
      Itm->Owner->Status = pkgAcquire::Item::StatDone;
   
   if (Itm->Owner->QueueCounter <= 1)
      Owner->Dequeue(Itm->Owner);
   else
   {
      Dequeue(Itm->Owner);
      Owner->Bump();
   }
   
   return Cycle();
}
									/*}}}*/
// Queue::Cycle - Queue new items into the method			/*{{{*/
// ---------------------------------------------------------------------
/* This locates a new idle item and sends it to the worker. If pipelining
   is enabled then it keeps the pipe full. */
bool pkgAcquire::Queue::Cycle()
{
   if (Items == 0 || Workers == 0)
      return true;

   if (PipeDepth < 0)
      return _error->Error("Pipedepth failure");
			   
   // Look for a queable item
   QItem *I = Items;
   while (PipeDepth < (signed)MaxPipeDepth)
   {
      for (; I != 0; I = I->Next)
	 if (I->Owner->Status == pkgAcquire::Item::StatIdle)
	    break;
      
      // Nothing to do, queue is idle.
      if (I == 0)
	 return true;
      
      I->Worker = Workers;
      I->Owner->Status = pkgAcquire::Item::StatFetching;
      PipeDepth++;
      if (Workers->QueueItem(I) == false)
	 return false;
   }
   
   return true;
}
									/*}}}*/
// Queue::Bump - Fetch any pending objects if we are idle		/*{{{*/
// ---------------------------------------------------------------------
/* This is called when an item in multiple queues is dequeued */
void pkgAcquire::Queue::Bump()
{
   Cycle();
}
									/*}}}*/

// AcquireStatus::pkgAcquireStatus - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquireStatus::pkgAcquireStatus() : Update(true), MorePulses(false)
{
   Start();
}
									/*}}}*/
// AcquireStatus::Pulse - Called periodically				/*{{{*/
// ---------------------------------------------------------------------
/* This computes some internal state variables for the derived classes to
   use. It generates the current downloaded bytes and total bytes to download
   as well as the current CPS estimate. */
bool pkgAcquireStatus::Pulse(pkgAcquire *Owner)
{
   TotalBytes = 0;
   CurrentBytes = 0;
   TotalItems = 0;
   CurrentItems = 0;
   
   // Compute the total number of bytes to fetch
   unsigned int Unknown = 0;
   unsigned int Count = 0;
   for (pkgAcquire::ItemCIterator I = Owner->ItemsBegin(); I != Owner->ItemsEnd();
	I++, Count++)
   {
      TotalItems++;
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 CurrentItems++;
      
      // Totally ignore local items
      if ((*I)->Local == true)
	 continue;

      TotalBytes += (*I)->FileSize;
      if ((*I)->Complete == true)
	 CurrentBytes += (*I)->FileSize;
      if ((*I)->FileSize == 0 && (*I)->Complete == false)
	 Unknown++;
   }
   
   // Compute the current completion
   unsigned long ResumeSize = 0;
   for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	I = Owner->WorkerStep(I))
      if (I->CurrentItem != 0 && I->CurrentItem->Owner->Complete == false)
      {
	 CurrentBytes += I->CurrentSize;
	 ResumeSize += I->ResumePoint;
	 
	 // Files with unknown size always have 100% completion
	 if (I->CurrentItem->Owner->FileSize == 0 && 
	     I->CurrentItem->Owner->Complete == false)
	    TotalBytes += I->CurrentSize;
      }
   
   // Normalize the figures and account for unknown size downloads
   if (TotalBytes <= 0)
      TotalBytes = 1;
   if (Unknown == Count)
      TotalBytes = Unknown;

   // Wha?! Is not supposed to happen.
   if (CurrentBytes > TotalBytes)
      CurrentBytes = TotalBytes;
   
   // Compute the CPS
   struct timeval NewTime;
   gettimeofday(&NewTime,0);
   if ((NewTime.tv_sec - Time.tv_sec == 6 && NewTime.tv_usec > Time.tv_usec) ||
       NewTime.tv_sec - Time.tv_sec > 6)
   {    
      double Delta = NewTime.tv_sec - Time.tv_sec + 
	             (NewTime.tv_usec - Time.tv_usec)/1000000.0;
      
      // Compute the CPS value
      if (Delta < 0.01)
	 CurrentCPS = 0;
      else
	 CurrentCPS = ((CurrentBytes - ResumeSize) - LastBytes)/Delta;
      LastBytes = CurrentBytes - ResumeSize;
      ElapsedTime = (unsigned long)Delta;
      Time = NewTime;
   }

   int fd = _config->FindI("APT::Status-Fd",-1);
   if(fd > 0) 
   {
      ostringstream status;

      char msg[200];
      long i = CurrentItems < TotalItems ? CurrentItems + 1 : CurrentItems;
      unsigned long ETA =
	 (unsigned long)((TotalBytes - CurrentBytes) / CurrentCPS);

      // only show the ETA if it makes sense
      if (ETA > 0 && ETA < 172800 /* two days */ )
	 snprintf(msg,sizeof(msg), _("Retrieving file %li of %li (%s remaining)"), i, TotalItems, TimeToStr(ETA).c_str());
      else
	 snprintf(msg,sizeof(msg), _("Retrieving file %li of %li"), i, TotalItems);
	 


      // build the status str
      status << "dlstatus:" << i
	     << ":"  << (CurrentBytes/float(TotalBytes)*100.0) 
	     << ":" << msg 
	     << endl;
      write(fd, status.str().c_str(), status.str().size());
   }

   return true;
}
									/*}}}*/
// AcquireStatus::Start - Called when the download is started		/*{{{*/
// ---------------------------------------------------------------------
/* We just reset the counters */
void pkgAcquireStatus::Start()
{
   gettimeofday(&Time,0);
   gettimeofday(&StartTime,0);
   LastBytes = 0;
   CurrentCPS = 0;
   CurrentBytes = 0;
   TotalBytes = 0;
   FetchedBytes = 0;
   ElapsedTime = 0;
   TotalItems = 0;
   CurrentItems = 0;
}
									/*}}}*/
// AcquireStatus::Stop - Finished downloading				/*{{{*/
// ---------------------------------------------------------------------
/* This accurately computes the elapsed time and the total overall CPS. */
void pkgAcquireStatus::Stop()
{
   // Compute the CPS and elapsed time
   struct timeval NewTime;
   gettimeofday(&NewTime,0);
   
   double Delta = NewTime.tv_sec - StartTime.tv_sec + 
                  (NewTime.tv_usec - StartTime.tv_usec)/1000000.0;
   
   // Compute the CPS value
   if (Delta < 0.01)
      CurrentCPS = 0;
   else
      CurrentCPS = FetchedBytes/Delta;
   LastBytes = CurrentBytes;
   ElapsedTime = (unsigned int)Delta;
}
									/*}}}*/
// AcquireStatus::Fetched - Called when a byte set has been fetched	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to get accurate final transfer rate reporting. */
void pkgAcquireStatus::Fetched(unsigned long Size,unsigned long Resume)
{   
   FetchedBytes += Size - Resume;
}
									/*}}}*/
