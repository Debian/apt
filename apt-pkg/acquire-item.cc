// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.cc,v 1.4 1998/10/24 04:57:56 jgg Exp $
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

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
									/*}}}*/

// Acquire::Item::Item - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Item::Item(pkgAcquire *Owner) : Owner(Owner), QueueCounter(0)
{
   Owner->Add(this);
   Status = StatIdle;
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
// Acquire::Item::Failed - Item failed to download			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Item::Failed(string Message)
{
   Status = StatError;
   ErrorText = LookupTag(Message,"Message");
   if (QueueCounter <= 1)
      Owner->Dequeue(this);
}
									/*}}}*/
// Acquire::Item::Done - Item downloaded OK				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Item::Done(string,unsigned long,string)
{
   Status = StatDone;
   ErrorText = string();
   Owner->Dequeue(this);
}
									/*}}}*/

// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The package file is added to the queue and a second class is 
   instantiated to fetch the revision file */
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,const pkgSourceList::Item *Location) :
             Item(Owner), Location(Location)
{
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Location->PackagesURI());
   
   QueueURI(Location->PackagesURI() + ".gz",Location->PackagesInfo());
   
   // Create the Release fetch class
   new pkgAcqIndexRel(Owner,Location);
}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqIndex::Custom600Headers()
{
   string Final = _config->FindDir("Dir::State::lists");
   Final += URItoFileName(Location->PackagesURI());
   
   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return string();
   
   return "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
// AcqIndexRel::pkgAcqIndexRel - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* The Release file is added to the queue */
pkgAcqIndexRel::pkgAcqIndexRel(pkgAcquire *Owner,
			       const pkgSourceList::Item *Location) :
                Item(Owner), Location(Location)
{
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Location->ReleaseURI());
   
   QueueURI(Location->ReleaseURI(),Location->ReleaseInfo());
}
									/*}}}*/
// AcqIndexRel::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqIndexRel::Custom600Headers()
{
   string Final = _config->FindDir("Dir::State::lists");
   Final += URItoFileName(Location->ReleaseURI());
   
   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return string();
   
   return "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
// AcqIndexRel::Done - Item downloaded OK				/*{{{*/
// ---------------------------------------------------------------------
/* The release file was not placed into the download directory then
   a copy URI is generated and it is copied there otherwise the file
   in the partial directory is moved into .. and the URI is finished. */
void pkgAcqIndexRel::Done(string Message,unsigned long Size,string MD5)
{
   Item::Done(Message,Size,MD5);

   string FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
   }
   
   // We have to copy it into place
   if (FileName != DestFile)
   {
      QueueURI("copy:" + FileName,string());
      return;
   }
   
   // Done, move it into position
   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(Location->ReleaseURI());
   
   if (rename(DestFile.c_str(),FinalFile.c_str()) != 0)
   {
      char S[300];
      sprintf(S,"rename failed, %s (%s -> %s).",strerror(errno),
	      DestFile.c_str(),FinalFile.c_str());
      Status = StatError;
      ErrorText = S;
   }
}
									/*}}}*/
