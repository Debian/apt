// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.cc,v 1.1 1998/10/15 06:59:59 jgg Exp $
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
/* */
pkgAcquire::~pkgAcquire()
{
   while (Items.size() != 0)
      delete Items[0];
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
}
									/*}}}*/

