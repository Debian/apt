// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.cc,v 1.1 1998/10/15 06:59:59 jgg Exp $
/* ######################################################################

   Acquire Item - Item to acquire

   Each item can download to exactly one file at a time. This means you
   cannot create an item that fetches two uri's to two files at the same 
   time. The pkgAcqIndex class creates a second class upon instantiation
   to fetch the other index files because of this.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/acquire-item.h"
#endif
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <strutl.h>
									/*}}}*/

// Acquire::Item::Item - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Item::Item(pkgAcquire *Owner) : Owner(Owner), QueueCounter(0)
{
   Owner->Add(this);
}
									/*}}}*/
// Acquire::Item::~Item - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Item::~Item()
{
   Owner->Remove(this);
}
									/*}}}*/

// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The package file is added to the queue and a second class is 
   instantiated to fetch the revision file */
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,const pkgSourceList::Item *Location) :
             Item(Owner), Location(Location)
{
   QueueURI(Location->PackagesURI() + ".gz");
   Description = Location->PackagesInfo();
   
   new pkgAcqIndexRel(Owner,Location);
}
									/*}}}*/
// pkgAcqIndex::ToFile - File to write the download to			/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgAcqIndex::ToFile()
{
   string PartialDir = _config->FindDir("Dir::State::lists") + "/partial/";
   
   return PartialDir + URItoFileName(Location->PackagesURI());
}
									/*}}}*/

// AcqIndexRel::pkgAcqIndexRel - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* The Release file is added to the queue */
pkgAcqIndexRel::pkgAcqIndexRel(pkgAcquire *Owner,
			       const pkgSourceList::Item *Location) :
                Item(Owner), Location(Location)
{
   QueueURI(Location->ReleaseURI());
   Description = Location->ReleaseInfo();
}
									/*}}}*/
// AcqIndexRel::ToFile - File to write the download to			/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgAcqIndexRel::ToFile()
{
   string PartialDir = _config->FindDir("Dir::State::lists") + "/partial/";
   
   return PartialDir + URItoFileName(Location->ReleaseURI());
}
									/*}}}*/
