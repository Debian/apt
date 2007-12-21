// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.h,v 1.26.2.3 2004/01/02 18:51:00 mdz Exp $
/* ######################################################################

   Acquire Item - Item to acquire

   When an item is instantiated it will add it self to the local list in
   the Owner Acquire class. Derived classes will then call QueueURI to 
   register all the URI's they wish to fetch at the initial moment.   
   
   Three item classes are provided to provide functionality for
   downloading of Index, Translation and Packages files.
   
   A Archive class is provided for downloading .deb files. It does Hash
   checking and source location as well as a retry algorithm.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_ITEM_H
#define PKGLIB_ACQUIRE_ITEM_H

#include <apt-pkg/acquire.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/vendor.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/hashes.h>

/** \addtogroup acquire
 *  @{
 *
 *  \file acquire-item.h
 */

/** \brief Represents the process by which a pkgAcquire object should
 *  retrieve a file or a collection of files.
 *
 *  By convention, Item subclasses should insert themselves into the
 *  acquire queue when they are created by calling QueueURI(), and
 *  remove themselves by calling Dequeue() when either Done() or
 *  Failed() is invoked.  Item objects are also responsible for
 *  notifying the download progress indicator (accessible via
 *  #Owner->Log) of their status.
 *
 *  \see pkgAcquire
 */
class pkgAcquire::Item
{  
   protected:
   
   /** \brief The acquire object with which this item is associated. */
   pkgAcquire *Owner;

   /** \brief Insert this item into its owner's queue.
    *
    *  \param ItemDesc Metadata about this item (its URI and
    *  description).
    */
   inline void QueueURI(ItemDesc &Item)
                 {Owner->Enqueue(Item);};

   /** \brief Remove this item from its owner's queue. */
   inline void Dequeue() {Owner->Dequeue(this);};
   
   /** \brief Rename a file without modifying its timestamp.
    *
    *  Many item methods call this as their final action.
    *
    *  \param From The file to be renamed.
    *
    *  \param To The new name of #From.  If #To exists it will be
    *  overwritten.
    */
   void Rename(string From,string To);
   
   public:

   /** \brief The current status of this item. */
   enum ItemState
     {
       /** \brief The item is waiting to be downloaded. */
       StatIdle,

       /** \brief The item is currently being downloaded. */
       StatFetching,

       /** \brief The item has been successfully downloaded. */
       StatDone,

       /** \brief An error was encountered while downloading this
	*  item.
	*/
       StatError,

       /** \brief The item was downloaded but its authenticity could
	*  not be verified.
	*/
       StatAuthError,

       /** \brief The item was could not be downloaded because of 
	*  a transient network error (e.g. network down)
	*/
       StatTransientNetworkError
     } Status;

   /** \brief Contains a textual description of the error encountered
    *  if #Status is #StatError or #StatAuthError.
    */
   string ErrorText;

   /** \brief The size of the object to fetch. */
   unsigned long FileSize;

   /** \brief How much of the object was already fetched. */
   unsigned long PartialSize;

   /** \brief If not \b NULL, contains the name of a subprocess that
    *  is operating on this object (for instance, "gzip" or "gpgv").
    */
   const char *Mode;

   /** \brief A client-supplied unique identifier.
    * 
    *  This field is initalized to 0; it is meant to be filled in by
    *  clients that wish to use it to uniquely identify items.
    *
    *  \todo it's unused in apt itself
    */
   unsigned long ID;

   /** \brief If \b true, the entire object has been successfully fetched.
    *
    *  Subclasses should set this to \b true when appropriate.
    */
   bool Complete;

   /** \brief If \b true, the URI of this object is "local".
    *
    *  The only effect of this field is to exclude the object from the
    *  download progress indicator's overall statistics.
    */
   bool Local;

   /** \brief The number of fetch queues into which this item has been
    *  inserted.
    *
    *  There is one queue for each source from which an item could be
    *  downloaded.
    *
    *  \sa pkgAcquire
    */
   unsigned int QueueCounter;
   
   /** \brief The name of the file into which the retrieved object
    *  will be written.
    */
   string DestFile;

   /** \brief Invoked by the acquire worker when the object couldn't
    *  be fetched.
    *
    *  This is a branch of the continuation of the fetch process.
    *
    *  \param Message An RFC822-formatted message from the acquire
    *  method describing what went wrong.  Use LookupTag() to parse
    *  it.
    *
    *  \param Cnf The method via which the worker tried to fetch this object.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);

   /** \brief Invoked by the acquire worker when the object was
    *  fetched successfully.
    *
    *  Note that the object might \e not have been written to
    *  DestFile; check for the presence of an Alt-Filename entry in
    *  Message to find the file to which it was really written.
    *
    *  Done is often used to switch from one stage of the processing
    *  to the next (e.g. fetching, unpacking, copying).  It is one
    *  branch of the continuation of the fetch process.
    *
    *  \param Message Data from the acquire method.  Use LookupTag()
    *  to parse it.
    *  \param Size The size of the object that was fetched.
    *  \param Hash The HashSum of the object that was fetched.
    *  \param Cnf The method via which the object was fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Done(string Message,unsigned long Size,string Hash,
		     pkgAcquire::MethodConfig *Cnf);

   /** \brief Invoked when the worker starts to fetch this object.
    *
    *  \param Message RFC822-formatted data from the worker process.
    *  Use LookupTag() to parse it.
    *
    *  \param Size The size of the object being fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Start(string Message,unsigned long Size);

   /** \brief Custom headers to be sent to the fetch process.
    *
    *  \return a string containing RFC822-style headers that are to be
    *  inserted into the 600 URI Acquire message sent to the fetch
    *  subprocess.  The headers are inserted after a newline-less
    *  line, so they should (if nonempty) have a leading newline and
    *  no trailing newline.
    */
   virtual string Custom600Headers() {return string();};

   /** \brief A "descriptive" URI-like string.
    *
    *  \return a URI that should be used to describe what is being fetched.
    */
   virtual string DescURI() = 0;
   /** \brief Short item description.
    *
    *  \return a brief description of the object being fetched.
    */
   virtual string ShortDesc() {return DescURI();}

   /** \brief Invoked by the worker when the download is completely done. */
   virtual void Finished() {};
   
   /** \brief HashSum 
    *
    *  \return the HashSum of this object, if applicable; otherwise, an
    *  empty string.
    */
   virtual string HashSum() {return string();};

   /** \return the acquire process with which this item is associated. */
   pkgAcquire *GetOwner() {return Owner;};

   /** \return \b true if this object is being fetched from a trusted source. */
   virtual bool IsTrusted() {return false;};

   /** \brief Initialize an item.
    *
    *  Adds the item to the list of items known to the acquire
    *  process, but does not place it into any fetch queues (you must
    *  manually invoke QueueURI() to do so).
    *
    *  Initializes all fields of the item other than Owner to 0,
    *  false, or the empty string.
    *
    *  \param Owner The new owner of this item.
    */
   Item(pkgAcquire *Owner);

   /** \brief Remove this item from its owner's queue by invoking
    *  pkgAcquire::Remove.
    */
   virtual ~Item();
};

/** \brief Information about an index patch (aka diff). */
struct DiffInfo {
   /** The filename of the diff. */
   string file;

   /** The sha1 hash of the diff. */
   string sha1;

   /** The size of the diff. */
   unsigned long size;
};

/** \brief An item that is responsible for fetching an index file of
 *  package list diffs and starting the package list's download.
 *
 *  This item downloads the Index file and parses it, then enqueues
 *  additional downloads of either the individual patches (using
 *  pkgAcqIndexDiffs) or the entire Packages file (using pkgAcqIndex).
 *
 *  \sa pkgAcqIndexDiffs, pkgAcqIndex
 */
class pkgAcqDiffIndex : public pkgAcquire::Item
{
 protected:
   /** \brief If \b true, debugging information will be written to std::clog. */
   bool Debug;

   /** \brief The item that is currently being downloaded. */
   pkgAcquire::ItemDesc Desc;

   /** \brief The URI of the index file to recreate at our end (either
    *  by downloading it or by applying partial patches).
    */
   string RealURI;

   /** \brief The Hash that the real index file should have after
    *  all patches have been applied.
    */
   HashString ExpectedHash;

   /** \brief The index file which will be patched to generate the new
    *  file.
    */
   string CurrentPackagesFile;

   /** \brief A description of the Packages file (stored in
    *  pkgAcquire::ItemDesc::Description).
    */
   string Description;

 public:
   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string Md5Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string DescURI() {return RealURI + "Index";};
   virtual string Custom600Headers();

   /** \brief Parse the Index file for a set of Packages diffs.
    *
    *  Parses the Index file and creates additional download items as
    *  necessary.
    *
    *  \param IndexDiffFile The name of the Index file.
    *
    *  \return \b true if the Index file was successfully parsed, \b
    *  false otherwise.
    */
   bool ParseDiffIndex(string IndexDiffFile);
   

   /** \brief Create a new pkgAcqDiffIndex.
    *
    *  \param Owner The Acquire object that owns this item.
    *
    *  \param URI The URI of the list file to download.
    *
    *  \param URIDesc A long description of the list file to download.
    *
    *  \param ShortDesc A short description of the list file to download.
    *
    *  \param ExpectedHash The list file's MD5 signature.
    */
   pkgAcqDiffIndex(pkgAcquire *Owner,string URI,string URIDesc,
		   string ShortDesc, HashString ExpectedHash);
};

/** \brief An item that is responsible for fetching all the patches
 *  that need to be applied to a given package index file.
 *
 *  After downloading and applying a single patch, this item will
 *  enqueue a new pkgAcqIndexDiffs to download and apply the remaining
 *  patches.  If no patch can be found that applies to an intermediate
 *  file or if one of the patches cannot be downloaded, falls back to
 *  downloading the entire package index file using pkgAcqIndex.
 *
 *  \sa pkgAcqDiffIndex, pkgAcqIndex
 */
class pkgAcqIndexDiffs : public pkgAcquire::Item
{
   private:

   /** \brief Queue up the next diff download.
    *
    *  Search for the next available diff that applies to the file
    *  that currently exists on disk, and enqueue it by calling
    *  QueueURI().
    *
    *  \return \b true if an applicable diff was found, \b false
    *  otherwise.
    */
   bool QueueNextDiff();

   /** \brief Handle tasks that must be performed after the item
    *  finishes downloading.
    *
    *  Dequeues the item and checks the resulting file's md5sum
    *  against ExpectedHash after the last patch was applied.
    *  There is no need to check the md5/sha1 after a "normal" 
    *  patch because QueueNextDiff() will check the sha1 later.
    *
    *  \param allDone If \b true, the file was entirely reconstructed,
    *  and its md5sum is verified. 
    */
   void Finish(bool allDone=false);

   protected:

   /** \brief If \b true, debugging output will be written to
    *  std::clog.
    */
   bool Debug;

   /** \brief A description of the item that is currently being
    *  downloaded.
    */
   pkgAcquire::ItemDesc Desc;

   /** \brief The URI of the package index file that is being
    *  reconstructed.
    */
   string RealURI;

   /** \brief The HashSum of the package index file that is being
    *  reconstructed.
    */
   HashString ExpectedHash;

   /** A description of the file being downloaded. */
   string Description;

   /** The patches that remain to be downloaded, including the patch
    *  being downloaded right now.  This list should be ordered so
    *  that each diff appears before any diff that depends on it.
    *
    *  \todo These are indexed by sha1sum; why not use some sort of
    *  dictionary instead of relying on ordering and stripping them
    *  off the front?
    */
   vector<DiffInfo> available_patches;
   /** The current status of this patch. */
   enum DiffState
     {
	/** \brief The diff is in an unknown state. */
	 StateFetchUnkown,

	 /** \brief The diff is currently being fetched. */
	 StateFetchDiff,
	 
	 /** \brief The diff is currently being uncompressed. */
	 StateUnzipDiff,

	 /** \brief The diff is currently being applied. */
	 StateApplyDiff
   } State;

   public:
   
   /** \brief Called when the patch file failed to be downloaded.
    *
    *  This method will fall back to downloading the whole index file
    *  outright; its arguments are ignored.
    */
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);

   virtual void Done(string Message,unsigned long Size,string Md5Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string DescURI() {return RealURI + "Index";};

   /** \brief Create an index diff item.
    *
    *  After filling in its basic fields, this invokes Finish(true) if
    *  #diffs is empty, or QueueNextDiff() otherwise.
    *
    *  \param Owner The pkgAcquire object that owns this item.
    *
    *  \param URI The URI of the package index file being
    *  reconstructed.
    *
    *  \param URIDesc A long description of this item.
    *
    *  \param ShortDesc A brief description of this item.
    *
    *  \param ExpectedHash The expected md5sum of the completely
    *  reconstructed package index file; the index file will be tested
    *  against this value when it is entirely reconstructed.
    *
    *  \param diffs The remaining diffs from the index of diffs.  They
    *  should be ordered so that each diff appears before any diff
    *  that depends on it.
    */
   pkgAcqIndexDiffs(pkgAcquire *Owner,string URI,string URIDesc,
		    string ShortDesc, HashString ExpectedHash,
		    vector<DiffInfo> diffs=vector<DiffInfo>());
};

/** \brief An acquire item that is responsible for fetching an index
 *  file (e.g., Packages or Sources).
 *
 *  \sa pkgAcqDiffIndex, pkgAcqIndexDiffs, pkgAcqIndexTrans
 *
 *  \todo Why does pkgAcqIndex have protected members?
 */
class pkgAcqIndex : public pkgAcquire::Item
{
   protected:

   /** \brief If \b true, the index file has been decompressed. */
   bool Decompression;

   /** \brief If \b true, the partially downloaded file will be
    *  removed when the download completes.
    */
   bool Erase;

   /** \brief The download request that is currently being
    *   processed.
    */
   pkgAcquire::ItemDesc Desc;

   /** \brief The object that is actually being fetched (minus any
    *  compression-related extensions).
    */
   string RealURI;

   /** \brief The expected hashsum of the decompressed index file. */
   HashString ExpectedHash;

   /** \brief The compression-related file extension that is being
    *  added to the downloaded file (e.g., ".gz" or ".bz2").
    */
   string CompressionExtension;

   public:
   
   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string Md5Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string Custom600Headers();
   virtual string DescURI() {return RealURI + CompressionExtension;};
   virtual string HashSum() {return ExpectedHash.toStr(); };

   /** \brief Create a pkgAcqIndex.
    *
    *  \param Owner The pkgAcquire object with which this item is
    *  associated.
    *
    *  \param URI The URI of the index file that is to be downloaded.
    *
    *  \param URIDesc A "URI-style" description of this index file.
    *
    *  \param ShortDesc A brief description of this index file.
    *
    *  \param ExpectedHash The expected hashsum of this index file.
    *
    *  \param compressExt The compression-related extension with which
    *  this index file should be downloaded, or "" to autodetect
    *  (".bz2" is used if bzip2 is installed, ".gz" otherwise).
    */
   pkgAcqIndex(pkgAcquire *Owner,string URI,string URIDesc,
	       string ShortDesc, HashString ExpectedHash, string compressExt="");
};

/** \brief An acquire item that is responsible for fetching a
 *  translated index file.
 *
 *  The only difference from pkgAcqIndex is that transient failures
 *  are suppressed: no error occurs if the translated index file is
 *  missing.
 */
class pkgAcqIndexTrans : public pkgAcqIndex
{
   public:
  
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);

   /** \brief Create a pkgAcqIndexTrans.
    *
    *  \param Owner The pkgAcquire object with which this item is
    *  associated.
    *
    *  \param URI The URI of the index file that is to be downloaded.
    *
    *  \param URIDesc A "URI-style" description of this index file.
    *
    *  \param ShortDesc A brief description of this index file.
    *
    *  \param ExpectedHash The expected hashsum of this index file.
    *
    *  \param compressExt The compression-related extension with which
    *  this index file should be downloaded, or "" to autodetect
    *  (".bz2" is used if bzip2 is installed, ".gz" otherwise).
    */
   pkgAcqIndexTrans(pkgAcquire *Owner,string URI,string URIDesc,
		    string ShortDesc);
};

/** \brief Information about an index file. */
struct IndexTarget
{
   /** \brief A URI from which the index file can be downloaded. */
   string URI;

   /** \brief A description of the index file. */
   string Description;

   /** \brief A shorter description of the index file. */
   string ShortDesc;

   /** \brief The key by which this index file should be
    *  looked up within the meta signature file.
    */
   string MetaKey;
};

/** \brief An acquire item that downloads the detached signature
 *  of a meta-index (Release) file, then queues up the release
 *  file itself.
 *
 *  \todo Why protected members?
 *
 *  \sa pkgAcqMetaIndex
 */
class pkgAcqMetaSig : public pkgAcquire::Item
{
   protected:
   /** \brief The last good signature file */
   string LastGoodSig;


   /** \brief The fetch request that is currently being processed. */
   pkgAcquire::ItemDesc Desc;

   /** \brief The URI of the signature file.  Unlike Desc.URI, this is
    *  never modified; it is used to determine the file that is being
    *  downloaded.
    */
   string RealURI;

   /** \brief The URI of the meta-index file to be fetched after the signature. */
   string MetaIndexURI;

   /** \brief A "URI-style" description of the meta-index file to be
    *  fetched after the signature.
    */
   string MetaIndexURIDesc;

   /** \brief A brief description of the meta-index file to be fetched
    *  after the signature.
    */
   string MetaIndexShortDesc;

   /** \brief A package-system-specific parser for the meta-index file. */
   indexRecords* MetaIndexParser;

   /** \brief The index files which should be looked up in the meta-index
    *  and then downloaded.
    *
    *  \todo Why a list of pointers instead of a list of structs?
    */
   const vector<struct IndexTarget*>* IndexTargets;

   public:
   
   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string Md5Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string Custom600Headers();
   virtual string DescURI() {return RealURI; };

   /** \brief Create a new pkgAcqMetaSig. */
   pkgAcqMetaSig(pkgAcquire *Owner,string URI,string URIDesc, string ShortDesc,
		 string MetaIndexURI, string MetaIndexURIDesc, string MetaIndexShortDesc,
		 const vector<struct IndexTarget*>* IndexTargets,
		 indexRecords* MetaIndexParser);
};

/** \brief An item that is responsible for downloading the meta-index
 *  file (i.e., Release) itself and verifying its signature.
 *
 *  Once the download and verification are complete, the downloads of
 *  the individual index files are queued up using pkgAcqDiffIndex.
 *  If the meta-index file had a valid signature, the expected hashsums
 *  of the index files will be the md5sums listed in the meta-index;
 *  otherwise, the expected hashsums will be "" (causing the
 *  authentication of the index files to be bypassed).
 */
class pkgAcqMetaIndex : public pkgAcquire::Item
{
   protected:
   /** \brief The fetch command that is currently being processed. */
   pkgAcquire::ItemDesc Desc;

   /** \brief The URI that is actually being downloaded; never
    *  modified by pkgAcqMetaIndex.
    */
   string RealURI;

   /** \brief The file in which the signature for this index was stored.
    *
    *  If empty, the signature and the md5sums of the individual
    *  indices will not be checked.
    */
   string SigFile;

   /** \brief The index files to download. */
   const vector<struct IndexTarget*>* IndexTargets;

   /** \brief The parser for the meta-index file. */
   indexRecords* MetaIndexParser;

   /** \brief If \b true, the index's signature is currently being verified.
    */
   bool AuthPass;
   // required to deal gracefully with problems caused by incorrect ims hits
   bool IMSHit; 

   /** \brief Check that the release file is a release file for the
    *  correct distribution.
    *
    *  \return \b true if no fatal errors were encountered.
    */
   bool VerifyVendor(string Message);

   /** \brief Called when a file is finished being retrieved.
    *
    *  If the file was not downloaded to DestFile, a copy process is
    *  set up to copy it to DestFile; otherwise, Complete is set to \b
    *  true and the file is moved to its final location.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    */
   void RetrievalDone(string Message);

   /** \brief Called when authentication succeeded.
    *
    *  Sanity-checks the authenticated file, queues up the individual
    *  index files for download, and saves the signature in the lists
    *  directory next to the authenticated list file.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    */
   void AuthDone(string Message);

   /** \brief Starts downloading the individual index files.
    *
    *  \param verify If \b true, only indices whose expected hashsum
    *  can be determined from the meta-index will be downloaded, and
    *  the hashsums of indices will be checked (reporting
    *  #StatAuthError if there is a mismatch).  If verify is \b false,
    *  no hashsum checking will be performed.
    */
   void QueueIndexes(bool verify);
   
   public:
   
   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size, string Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string Custom600Headers();
   virtual string DescURI() {return RealURI; };

   /** \brief Create a new pkgAcqMetaIndex. */
   pkgAcqMetaIndex(pkgAcquire *Owner,
		   string URI,string URIDesc, string ShortDesc,
		   string SigFile,
		   const vector<struct IndexTarget*>* IndexTargets,
		   indexRecords* MetaIndexParser);
};

/** \brief An item that is responsible for fetching a package file.
 *
 *  If the package file already exists in the cache, nothing will be
 *  done.
 */
class pkgAcqArchive : public pkgAcquire::Item
{
   protected:
   /** \brief The package version being fetched. */
   pkgCache::VerIterator Version;

   /** \brief The fetch command that is currently being processed. */
   pkgAcquire::ItemDesc Desc;

   /** \brief The list of sources from which to pick archives to
    *  download this package from.
    */
   pkgSourceList *Sources;

   /** \brief A package records object, used to look up the file
    *  corresponding to each version of the package.
    */
   pkgRecords *Recs;

   /** \brief The hashsum of this package. */
   HashString ExpectedHash;

   /** \brief A location in which the actual filename of the package
    *  should be stored.
    */
   string &StoreFilename;

   /** \brief The next file for this version to try to download. */
   pkgCache::VerFileIterator Vf;

   /** \brief How many (more) times to try to find a new source from
    *  which to download this package version if it fails.
    *
    *  Set from Acquire::Retries.
    */
   unsigned int Retries;

   /** \brief \b true if this version file is being downloaded from a
    *  trusted source.
    */
   bool Trusted; 

   /** \brief Queue up the next available file for this version. */
   bool QueueNext();
   
   public:
   
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string Hash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string DescURI() {return Desc.URI;};
   virtual string ShortDesc() {return Desc.ShortDesc;};
   virtual void Finished();
   virtual string HashSum() {return ExpectedHash.toStr(); };
   virtual bool IsTrusted();
   
   /** \brief Create a new pkgAcqArchive.
    *
    *  \param Owner The pkgAcquire object with which this item is
    *  associated.
    *
    *  \param Sources The sources from which to download version
    *  files.
    *
    *  \param Recs A package records object, used to look up the file
    *  corresponding to each version of the package.
    *
    *  \param Version The package version to download.
    *
    *  \param StoreFilename A location in which the actual filename of
    *  the package should be stored.  It will be set to a guessed
    *  basename in the constructor, and filled in with a fully
    *  qualified filename once the download finishes.
    */
   pkgAcqArchive(pkgAcquire *Owner,pkgSourceList *Sources,
		 pkgRecords *Recs,pkgCache::VerIterator const &Version,
		 string &StoreFilename);
};

/** \brief Retrieve an arbitrary file to the current directory.
 *
 *  The file is retrieved even if it is accessed via a URL type that
 *  normally is a NOP, such as "file".  If the download fails, the
 *  partial file is renamed to get a ".FAILED" extension.
 */
class pkgAcqFile : public pkgAcquire::Item
{
   /** \brief The currently active download process. */
   pkgAcquire::ItemDesc Desc;

   /** \brief The hashsum of the file to download, if it is known. */
   HashString ExpectedHash;

   /** \brief How many times to retry the download, set from
    *  Acquire::Retries.
    */
   unsigned int Retries;
   
   public:
   
   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string CalcHash,
		     pkgAcquire::MethodConfig *Cnf);
   virtual string DescURI() {return Desc.URI;};
   virtual string HashSum() {return ExpectedHash.toStr(); };

   /** \brief Create a new pkgAcqFile object.
    *
    *  \param Owner The pkgAcquire object with which this object is
    *  associated.
    *
    *  \param URI The URI to download.
    *
    *  \param Hash The hashsum of the file to download, if it is known;
    *  otherwise "".
    *
    *  \param Size The size of the file to download, if it is known;
    *  otherwise 0.
    *
    *  \param Desc A description of the file being downloaded.
    *
    *  \param ShortDesc A brief description of the file being
    *  downloaded.
    *
    *  \param DestDir The directory the file should be downloaded into.
    *
    *  \param DestFilename The filename+path the file is downloaded to.
    *
    *
    * If DestFilename is empty, download to DestDir/<basename> if
    * DestDir is non-empty, $CWD/<basename> otherwise.  If
    * DestFilename is NOT empty, DestDir is ignored and DestFilename
    * is the absolute name to which the file should be downloaded.
    */

   pkgAcqFile(pkgAcquire *Owner, string URI, string Hash, unsigned long Size,
	      string Desc, string ShortDesc,
	      const string &DestDir="", const string &DestFilename="");
};

/** @} */

#endif
