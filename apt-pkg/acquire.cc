// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.cc,v 1.4 1998/10/24 04:58:01 jgg Exp $
/* ######################################################################

   Acquire - File Acquiration

   The core element for the schedual system is the concept of a named
   queue. Each queue is unique and each queue has a name derived from the
   URI. The degree of paralization can be controled by how the queue
   name is derived from the URI.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/acquire.h"
#endif       
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <strutl.h>
									/*}}}*/

// Acquire::pkgAcquire - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::pkgAcquire()
{
   Queues = 0;
   Configs = 0;
   Workers = 0;
   ToFetch = 0;
   
   string Mode = _config->Find("Acquire::Queue-Mode","host");
   if (strcasecmp(Mode.c_str(),"host") == 0)
      QueueMode = QueueHost;
   if (strcasecmp(Mode.c_str(),"access") == 0)
      QueueMode = QueueAccess;   

   Debug = _config->FindB("Debug::pkgAcquire",false);
}
									/*}}}*/
// Acquire::~pkgAcquire	- Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* Free our memory */
pkgAcquire::~pkgAcquire()
{
   while (Items.size() != 0)
      delete Items[0];

   while (Configs != 0)
   {
      MethodConfig *Jnk = Configs;
      Configs = Configs->Next;
      delete Jnk;
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
/* */
void pkgAcquire::Add(Item *Itm)
{
   Items.push_back(Itm);
}
									/*}}}*/
// Acquire::Remove - Remove a item					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Remove(Item *Itm)
{
   for (vector<Item *>::iterator I = Items.begin(); I < Items.end(); I++)
   {
      if (*I == Itm)
	 Items.erase(I);
   }   
}
									/*}}}*/
// Acquire::Add - Add a worker						/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Add(Worker *Work)
{
   Work->NextAcquire = Workers;
   Workers = Work;
}
									/*}}}*/
// Acquire::Remove - Remove a worker					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Remove(Worker *Work)
{
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
/* */
void pkgAcquire::Enqueue(Item *Itm,string URI,string Description)
{
   // Determine which queue to put the item in
   string Name = QueueName(URI);
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
   }
   
   // Queue it into the named queue
   I->Enqueue(Itm,URI,Description);
   ToFetch++;
      
   // Some trace stuff
   if (Debug == true)
   {
      clog << "Fetching " << URI << endl;
      clog << " to " << Itm->DestFile << endl;
      clog << " Queue is: " << QueueName(URI) << endl;
   }
}
									/*}}}*/
// Acquire::Dequeue - Remove an item from all queues			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Dequeue(Item *Itm)
{
   Queue *I = Queues;
   for (; I != 0; I = I->Next)
      I->Dequeue(Itm);
   ToFetch--;
}
									/*}}}*/
// Acquire::QueueName - Return the name of the queue for this URI	/*{{{*/
// ---------------------------------------------------------------------
/* The string returned depends on the configuration settings and the
   method parameters. Given something like http://foo.org/bar it can
   return http://foo.org or http */
string pkgAcquire::QueueName(string URI)
{
   const MethodConfig *Config = GetConfig(URIAccess(URI));
   if (Config == 0)
      return string();
   
   /* Single-Instance methods get exactly one queue per URI. This is
      also used for the Access queue method  */
   if (Config->SingleInstance == true || QueueMode == QueueAccess)
      return URIAccess(URI);
      
   // Host based queue 
   string::iterator I = URI.begin();
   for (; I < URI.end() && *I != ':'; I++);
   for (; I < URI.end() && (*I == '/' || *I == ':'); I++);
   for (; I < URI.end() && *I != '/'; I++);
	
   return string(URI,0,I - URI.begin());
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
/* Dispatch active FDs over to the proper workers */
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
bool pkgAcquire::Run()
{
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Startup();
   
   // Run till all things have been acquired
   while (ToFetch > 0)
   {
      fd_set RFds;
      fd_set WFds;
      int Highest = 0;
      FD_ZERO(&RFds);
      FD_ZERO(&WFds);
      SetFds(Highest,&RFds,&WFds);
      
      if (select(Highest+1,&RFds,&WFds,0,0) <= 0)
	 return _error->Errno("select","Select has failed");
      
      RunFds(&RFds,&WFds);
   }   
   
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Shutdown();

   return true;
}
									/*}}}*/

// Acquire::MethodConfig::MethodConfig - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::MethodConfig::MethodConfig()
{
   SingleInstance = false;
   PreScan = false;
   Pipeline = false;
   SendConfig = false;
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
}
									/*}}}*/
// Queue::~Queue - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::~Queue()
{
   Shutdown();
   
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
void pkgAcquire::Queue::Enqueue(Item *Owner,string URI,string Description)
{
   // Create a new item
   QItem *I = new QItem;   
   I->Next = Items;
   Items = I;
   
   // Fill it in
   Items->Owner = Owner;
   Items->URI = URI;
   Items->Description = Description;
   Owner->QueueCounter++;
}
									/*}}}*/
// Queue::Dequeue - Remove an item from the queue			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Queue::Dequeue(Item *Owner)
{
   QItem **I = &Items;
   for (; *I != 0;)
   {
      if ((*I)->Owner == Owner)
      {
	 QItem *Jnk= *I;
	 *I = (*I)->Next;
	 Owner->QueueCounter--;
	 delete Jnk;
      }
      else
	 I = &(*I)->Next;
   }
}
									/*}}}*/
// Queue::Startup - Start the worker processes				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Queue::Startup()
{
   Shutdown();
   
   pkgAcquire::MethodConfig *Cnf = Owner->GetConfig(URIAccess(Name));
   if (Cnf == 0)
      return false;
   
   Workers = new Worker(this,Cnf);
   Owner->Add(Workers);
   if (Workers->Start() == false)
      return false;
      
   Items->Worker = Workers;
   Workers->QueueItem(Items);
   
   return true;
}
									/*}}}*/
// Queue::Shutdown - Shutdown the worker processes			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Queue::Shutdown()
{
   // Delete all of the workers
   while (Workers != 0)
   {
      pkgAcquire::Worker *Jnk = Workers;
      Workers = Workers->NextQueue;
      Owner->Remove(Jnk);
      delete Jnk;
   }
   
   return true;
}
									/*}}}*/
// Queue::Finditem - Find a URI in the item list			/*{{{*/
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
   queue. */
bool pkgAcquire::Queue::ItemDone(QItem *Itm)
{
   Dequeue(Itm->Owner);
   
   if (Items == 0)
      return true;

   Items->Worker = Workers;
   Items->Owner->Status = pkgAcquire::Item::StatFetching;
   return Workers->QueueItem(Items);
}
									/*}}}*/
