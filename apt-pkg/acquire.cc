// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.cc,v 1.2 1998/10/20 02:39:15 jgg Exp $
/* ######################################################################

   Acquire - File Acquiration

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/acquire.h"
#endif       
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <strutl.h>
									/*}}}*/

// Acquire::pkgAcquire - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::pkgAcquire()
{
   Queues = 0;
   Configs = 0;
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
// Acquire::Enqueue - Queue an URI for fetching				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Enqueue(Item *Item,string URI)
{
   cout << "Fetching " << URI << endl;
   cout << "   to " << Item->ToFile() << endl;
   cout << " Queue is: " << QueueName(URI) << endl;
}
									/*}}}*/
// Acquire::QueueName - Return the name of the queue for this URI	/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgAcquire::QueueName(string URI)
{
   const MethodConfig *Config = GetConfig(URIAccess(URI));
   return string();
}
									/*}}}*/
// Acquire::GetConfig - Fetch the configuration information		/*{{{*/
// ---------------------------------------------------------------------
/* This locates the configuration structure for an access method. If 
   a config structure cannot be found a Worker will be created to
   retrieve it */
const pkgAcquire::MethodConfig *pkgAcquire::GetConfig(string Access)
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

// Acquire::MethodConfig::MethodConfig - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::MethodConfig::MethodConfig()
{
   SingleInstance = false;
   PreScan = false;
}
									/*}}}*/
