// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.cc,v 1.10 1998/11/12 04:10:52 jgg Exp $
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
pkgAcquire::Item::Item(pkgAcquire *Owner) : Owner(Owner), FileSize(0),
                       Mode(0), ID(0), Complete(false), QueueCounter(0)
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
// Acquire::Item::Start - Item has begun to download			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Item::Start(string Message,unsigned long Size)
{
   Status = StatFetching;
   if (FileSize == 0 && Complete == false)
      FileSize = Size;
}
									/*}}}*/
// Acquire::Item::Done - Item downloaded OK				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Item::Done(string Message,unsigned long Size,string)
{
   // We just downloaded something..
   string FileName = LookupTag(Message,"Filename");
   if (Complete == false && FileName == DestFile)
   {
      if (Owner->Log != 0)
	 Owner->Log->Fetched(Size,atoi(LookupTag(Message,"Resume-Point","0").c_str()));
   }
   
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
   Erase = false;
   
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Location->PackagesURI());

   // Create the item 
   Desc.URI = Location->PackagesURI() + ".gz";
   Desc.Description = Location->PackagesInfo();
   Desc.Owner = this;

   // Set the short description to the archive component
   if (Location->Dist[Location->Dist.size() - 1] == '/')
      Desc.ShortDesc = Location->Dist;
   else
      Desc.ShortDesc = Location->Dist + '/' + Location->Section;  
      
   QueueURI(Desc);
   
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
      
      /* We restore the original name to DestFile so that the clean operation
         will work OK */
      DestFile = _config->FindDir("Dir::State::lists") + "partial/";
      DestFile += URItoFileName(Location->PackagesURI());
      
      // Remove the compressed version.
      if (Erase == true)
	 unlink(DestFile.c_str());
      return;
   }

   Erase = false;
   Complete = true;
   
   // Handle the unzipd case
   string FileName = LookupTag(Message,"Alt-Filename");
   if (FileName.empty() == false)
   {
      // The files timestamp matches
      if (StringToBool(LookupTag(Message,"Alt-IMS-Hit"),false) == true)
	 return;
      
      Decompression = true;
      FileSize = 0;
      DestFile += ".decomp";
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      Mode = "copy";
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

   if (FileName == DestFile)
      Erase = true;
   else
      FileSize = 0;
   
   Decompression = true;
   DestFile += ".decomp";
   Desc.URI = "gzip:" + FileName,Location->PackagesInfo();
   QueueURI(Desc);
   Mode = "gzip";
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
   
   // Create the item
   Desc.URI = Location->ReleaseURI();
   Desc.Description = Location->ReleaseInfo();
   Desc.Owner = this;

   // Set the short description to the archive component
   if (Location->Dist[Location->Dist.size() - 1] == '/')
      Desc.ShortDesc = Location->Dist;
   else
      Desc.ShortDesc = Location->Dist + '/' + Location->Section;  
      
   QueueURI(Desc);
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

   Complete = true;
   
   // The files timestamp matches
   if (StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      return;
   
   // We have to copy it into place
   if (FileName != DestFile)
   {
      FileSize = 0;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      return;
   }
   
   // Done, move it into position
   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(Location->ReleaseURI());
   Rename(DestFile,FinalFile);
}
									/*}}}*/
