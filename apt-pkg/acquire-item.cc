// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.cc,v 1.6 1998/10/30 07:53:34 jgg Exp $
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
/* We return to an idle state if there are still other queues that could
   fetch this object */
void pkgAcquire::Item::Failed(string Message)
{
   Status = StatIdle;
   if (QueueCounter <= 1)
   {
      ErrorText = LookupTag(Message,"Message");
      Status = StatError;
      Owner->Dequeue(this);
   }   
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
// Acquire::Item::Rename - Rename a file				/*{{{*/
// ---------------------------------------------------------------------
/* This helper function is used by alot of item methods as thier final
   step */
void pkgAcquire::Item::Rename(string From,string To)
{
   if (rename(From.c_str(),To.c_str()) != 0)
   {
      char S[300];
      sprintf(S,"rename failed, %s (%s -> %s).",strerror(errno),
	      From.c_str(),To.c_str());
      Status = StatError;
      ErrorText = S;
   }      
}
									/*}}}*/

// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The package file is added to the queue and a second class is 
   instantiated to fetch the revision file */
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,const pkgSourceList::Item *Location) :
             Item(Owner), Location(Location)
{
   Decompression = false;
   
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
// AcqIndex::Done - Finished a fetch					/*{{{*/
// ---------------------------------------------------------------------
/* This goes through a number of states.. On the initial fetch the
   method could possibly return an alternate filename which points
   to the uncompressed version of the file. If this is so the file
   is copied into the partial directory. In all other cases the file
   is decompressed with a gzip uri. */
void pkgAcqIndex::Done(string Message,unsigned long Size,string MD5)
{
   Item::Done(Message,Size,MD5);

   if (Decompression == true)
   {
      // Done, move it into position
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(Location->PackagesURI());
      Rename(DestFile,FinalFile);
      return;
   }
      
   // Handle the unzipd case
   string FileName = LookupTag(Message,"Alt-Filename");
   if (FileName.empty() == false)
   {
      // The files timestamp matches
      if (StringToBool(LookupTag(Message,"Alt-IMS-Hit"),false) == true)
	 return;
      
      Decompression = true;
      DestFile += ".decomp";
      QueueURI("copy:" + FileName,string());
      return;
   }

   FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
   }
   
   // The files timestamp matches
   if (StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      return;
   
   Decompression = true;
   DestFile += ".decomp";
   QueueURI("gzip:" + FileName,string());
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
      return;
   }

   // The files timestamp matches
   if (StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      return;
   
   // We have to copy it into place
   if (FileName != DestFile)
   {
      QueueURI("copy:" + FileName,string());
      return;
   }
   
   // Done, move it into position
   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(Location->ReleaseURI());
   Rename(DestFile,FinalFile);
}
									/*}}}*/
