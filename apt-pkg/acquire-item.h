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
#include <apt-pkg/hashes.h>
#include <apt-pkg/weakptr.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#include <apt-pkg/vendor.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/indexrecords.h>
#endif

/** \addtogroup acquire
 *  @{
 *
 *  \file acquire-item.h
 */

class indexRecords;
class pkgRecords;
class pkgSourceList;
class IndexTarget;
class pkgAcqMetaBase;

/** \brief Represents the process by which a pkgAcquire object should	{{{
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
class pkgAcquire::Item : public WeakPointable
{  
   void *d;

   protected:
   
   /** \brief The acquire object with which this item is associated. */
   pkgAcquire *Owner;

   /** \brief Insert this item into its owner's queue.
    *
    *  \param Item Metadata about this item (its URI and
    *  description).
    */
   void QueueURI(ItemDesc &Item);

   /** \brief Remove this item from its owner's queue. */
   void Dequeue();

   /** \brief Rename a file without modifying its timestamp.
    *
    *  Many item methods call this as their final action.
    *
    *  \param From The file to be renamed.
    *
    *  \param To The new name of \a From.  If \a To exists it will be
    *  overwritten.
    */
   bool Rename(std::string From,std::string To);

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
       StatTransientNetworkError,
     } Status;

   /** \brief Contains a textual description of the error encountered
    *  if #ItemState is #StatError or #StatAuthError.
    */
   std::string ErrorText;

   /** \brief The size of the object to fetch. */
   unsigned long long FileSize;

   /** \brief How much of the object was already fetched. */
   unsigned long long PartialSize;

   /** \brief If not \b NULL, contains the name of a subprocess that
    *  is operating on this object (for instance, "gzip" or "gpgv").
    */
   APT_DEPRECATED const char *Mode;

   /** \brief contains the name of the subprocess that is operating on this object
    * (for instance, "gzip", "rred" or "gpgv"). This is obsoleting #Mode from above
    * as it can manage the lifetime of included string properly. */
   std::string ActiveSubprocess;

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
   std::string UsedMirror;

   /** \brief The number of fetch queues into which this item has been
    *  inserted.
    *
    *  There is one queue for each source from which an item could be
    *  downloaded.
    *
    *  \sa pkgAcquire
    */
   unsigned int QueueCounter;

   /** \brief TransactionManager */
   pkgAcqMetaBase *TransactionManager;

   /** \brief The number of additional fetch items that are expected
    *  once this item is done.
    *
    *  Some items like pkgAcqMeta{Index,Sig} will queue additional
    *  items. This variable can be set by the methods if it knows
    *  in advance how many items to expect to get a more accurate
    *  progress.
    */
   unsigned int ExpectedAdditionalItems;
   
   /** \brief The name of the file into which the retrieved object
    *  will be written.
    */
   std::string DestFile;

   /** \brief storge name until a transaction is finished */
   std::string PartialFile;

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
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);

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
    *  \param Hashes The HashSums of the object that was fetched.
    *  \param Cnf The method via which the object was fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Done(std::string Message, unsigned long long Size, HashStringList const &Hashes,
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
   virtual void Start(std::string Message,unsigned long long Size);

   /** \brief Custom headers to be sent to the fetch process.
    *
    *  \return a string containing RFC822-style headers that are to be
    *  inserted into the 600 URI Acquire message sent to the fetch
    *  subprocess.  The headers are inserted after a newline-less
    *  line, so they should (if nonempty) have a leading newline and
    *  no trailing newline.
    */
   virtual std::string Custom600Headers() const {return std::string();};

   /** \brief A "descriptive" URI-like string.
    *
    *  \return a URI that should be used to describe what is being fetched.
    */
   virtual std::string DescURI() = 0;
   /** \brief Short item description.
    *
    *  \return a brief description of the object being fetched.
    */
   virtual std::string ShortDesc() {return DescURI();}

   /** \brief Invoked by the worker when the download is completely done. */
   virtual void Finished() {};
   
   /** \brief HashSums
    *
    *  \return the HashSums of this object, if applicable; otherwise, an
    *  empty list.
    */
   HashStringList HashSums() const {return ExpectedHashes;};
   std::string HashSum() const {HashStringList const hashes = HashSums(); HashString const * const hs = hashes.find(NULL); return hs != NULL ? hs->toStr() : ""; };

   /** \return the acquire process with which this item is associated. */
   pkgAcquire *GetOwner() const {return Owner;};

   /** \return \b true if this object is being fetched from a trusted source. */
   virtual bool IsTrusted() const {return false;};
   
   /** \brief Report mirror problem
    * 
    *  This allows reporting mirror failures back to a centralized
    *  server. The apt-report-mirror-failure script is called for this
    * 
    *  \param FailCode A short failure string that is send
    */
   void ReportMirrorFailure(std::string FailCode);

   /** \brief Set the name of the current active subprocess
    *
    *  See also #ActiveSubprocess
    */
   void SetActiveSubprocess(const std::string &subprocess);

   /** \brief Initialize an item.
    *
    *  Adds the item to the list of items known to the acquire
    *  process, but does not place it into any fetch queues (you must
    *  manually invoke QueueURI() to do so).
    *
    *  \param Owner The new owner of this item.
    *  \param ExpectedHashes of the file represented by this item
    */
   Item(pkgAcquire *Owner,
        HashStringList const &ExpectedHashes=HashStringList(),
        pkgAcqMetaBase *TransactionManager=NULL);

   /** \brief Remove this item from its owner's queue by invoking
    *  pkgAcquire::Remove.
    */
   virtual ~Item();

   protected:

   enum RenameOnErrorState {
      HashSumMismatch,
      SizeMismatch,
      InvalidFormat,
      SignatureError,
      NotClearsigned,
   };

   /** \brief Rename failed file and set error
    *
    * \param state respresenting the error we encountered
    */
   bool RenameOnError(RenameOnErrorState const state);

   /** \brief The HashSums of the item is supposed to have than done */
   HashStringList ExpectedHashes;

   /** \brief The item that is currently being downloaded. */
   pkgAcquire::ItemDesc Desc;
};
									/*}}}*/
/** \brief Information about an index patch (aka diff). */		/*{{{*/
struct DiffInfo {
   /** The filename of the diff. */
   std::string file;

   /** The hashes of the diff */
   HashStringList result_hashes;

   /** The hashes of the file after the diff is applied */
   HashStringList patch_hashes;

   /** The size of the file after the diff is applied */
   unsigned long long result_size;

   /** The size of the diff itself */
   unsigned long long patch_size;
};
									/*}}}*/
									/*}}}*/

class pkgAcqMetaBase  : public pkgAcquire::Item
{
   void *d;

 protected:
   std::vector<Item*> Transaction;

   /** \brief A package-system-specific parser for the meta-index file. */
   indexRecords *MetaIndexParser;

   /** \brief The index files which should be looked up in the meta-index
    *  and then downloaded.
    */
   const std::vector<IndexTarget*>* IndexTargets;

   /** \brief If \b true, the index's signature is currently being verified.
    */
   bool AuthPass;

   // required to deal gracefully with problems caused by incorrect ims hits
   bool IMSHit; 

   /** \brief Starts downloading the individual index files.
    *
    *  \param verify If \b true, only indices whose expected hashsum
    *  can be determined from the meta-index will be downloaded, and
    *  the hashsums of indices will be checked (reporting
    *  #StatAuthError if there is a mismatch).  If verify is \b false,
    *  no hashsum checking will be performed.
    */
   void QueueIndexes(bool verify);

   /** \brief Called when a file is finished being retrieved.
    *
    *  If the file was not downloaded to DestFile, a copy process is
    *  set up to copy it to DestFile; otherwise, Complete is set to \b
    *  true and the file is moved to its final location.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    */
   bool CheckDownloadDone(const std::string &Message,
                          const std::string &RealURI);

   /** \brief Queue the downloaded Signature for verification */
   void QueueForSignatureVerify(const std::string &MetaIndexFile,
                                const std::string &MetaIndexFileSignature);

   /** \brief get the custom600 header for all pkgAcqMeta */
   std::string GetCustom600Headers(const std::string &RealURI) const;

   /** \brief Called when authentication succeeded.
    *
    *  Sanity-checks the authenticated file, queues up the individual
    *  index files for download, and saves the signature in the lists
    *  directory next to the authenticated list file.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    */
   bool CheckAuthDone(std::string Message, const std::string &RealURI);

   /** Check if the current item should fail at this point */
   bool CheckStopAuthentication(const std::string &RealURI,
                                const std::string &Message);

   /** \brief Check that the release file is a release file for the
    *  correct distribution.
    *
    *  \return \b true if no fatal errors were encountered.
    */
   bool VerifyVendor(std::string Message, const std::string &RealURI);
   
 public:
   // transaction code
   void Add(Item *I);
   void AbortTransaction();
   bool TransactionHasError() APT_PURE;
   void CommitTransaction();

   /** \brief Stage (queue) a copy action when the transaction is commited
    */
   void TransactionStageCopy(Item *I,
                             const std::string &From, 
                             const std::string &To);
   /** \brief Stage (queue) a removal action when the transaction is commited
    */
   void TransactionStageRemoval(Item *I, const std::string &FinalFile);

   pkgAcqMetaBase(pkgAcquire *Owner,
                  const std::vector<IndexTarget*>* IndexTargets,
                  indexRecords* MetaIndexParser,
                  HashStringList const &ExpectedHashes=HashStringList(),
                  pkgAcqMetaBase *TransactionManager=NULL)
      : Item(Owner, ExpectedHashes, TransactionManager),
        MetaIndexParser(MetaIndexParser), IndexTargets(IndexTargets),
        AuthPass(false), IMSHit(false) {};
};

/** \brief An acquire item that downloads the detached signature	{{{
 *  of a meta-index (Release) file, then queues up the release
 *  file itself.
 *
 *  \todo Why protected members?
 *
 *  \sa pkgAcqMetaIndex
 */
class pkgAcqMetaSig : public pkgAcqMetaBase
{
   void *d;

   protected:

   /** \brief The URI of the signature file.  Unlike Desc.URI, this is
    *  never modified; it is used to determine the file that is being
    *  downloaded.
    */
   std::string RealURI;

   /** \brief The file we need to verify */
   std::string MetaIndexFile;

   /** \brief The file we use to verify the MetaIndexFile with */
   std::string MetaIndexFileSignature;

   /** \brief Long URI description used in the acquire system */
   std::string URIDesc;

   /** \brief Short URI description used in the acquire system */
   std::string ShortDesc;

   public:
   
   // Specialized action members
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size,
                     HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string Custom600Headers() const;
   virtual std::string DescURI() {return RealURI; };

   /** \brief Create a new pkgAcqMetaSig. */
   pkgAcqMetaSig(pkgAcquire *Owner,
                 pkgAcqMetaBase *TransactionManager,
                 std::string URI,std::string URIDesc, std::string ShortDesc,
                 std::string MetaIndexFile,
		 const std::vector<IndexTarget*>* IndexTargets,
		 indexRecords* MetaIndexParser);
   virtual ~pkgAcqMetaSig();
};
									/*}}}*/

/** \brief An item that is responsible for downloading the meta-index	{{{
 *  file (i.e., Release) itself and verifying its signature.
 *
 *  Once the download and verification are complete, the downloads of
 *  the individual index files are queued up using pkgAcqDiffIndex.
 *  If the meta-index file had a valid signature, the expected hashsums
 *  of the index files will be the md5sums listed in the meta-index;
 *  otherwise, the expected hashsums will be "" (causing the
 *  authentication of the index files to be bypassed).
 */
class pkgAcqMetaIndex : public pkgAcqMetaBase
{
   void *d;

   protected:
   /** \brief The URI that is actually being downloaded; never
    *  modified by pkgAcqMetaIndex.
    */
   std::string RealURI;

   std::string URIDesc;
   std::string ShortDesc;

   /** \brief The URI of the meta-index file for the detached signature */
   std::string MetaIndexSigURI;

   /** \brief A "URI-style" description of the meta-index file */
   std::string MetaIndexSigURIDesc;

   /** \brief A brief description of the meta-index file */
   std::string MetaIndexSigShortDesc;

   /** \brief delayed constructor */
   void Init(std::string URIDesc, std::string ShortDesc);
   
   public:

   // Specialized action members
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string Custom600Headers() const;
   virtual std::string DescURI() {return RealURI; };
   virtual void Finished();

   /** \brief Create a new pkgAcqMetaIndex. */
   pkgAcqMetaIndex(pkgAcquire *Owner,
                   pkgAcqMetaBase *TransactionManager,
		   std::string URI,std::string URIDesc, std::string ShortDesc,
                   std::string MetaIndexSigURI, std::string MetaIndexSigURIDesc, std::string MetaIndexSigShortDesc,
		   const std::vector<IndexTarget*>* IndexTargets,
		   indexRecords* MetaIndexParser);
};
									/*}}}*/
/** \brief An item repsonsible for downloading clearsigned metaindexes	{{{*/
class pkgAcqMetaClearSig : public pkgAcqMetaIndex
{
   void *d;

   /** \brief The URI of the meta-index file for the detached signature */
   std::string MetaIndexURI;

   /** \brief A "URI-style" description of the meta-index file */
   std::string MetaIndexURIDesc;

   /** \brief A brief description of the meta-index file */
   std::string MetaIndexShortDesc;

   /** \brief The URI of the detached meta-signature file if the clearsigned one failed. */
   std::string MetaSigURI;

   /** \brief A "URI-style" description of the meta-signature file */
   std::string MetaSigURIDesc;

   /** \brief A brief description of the meta-signature file */
   std::string MetaSigShortDesc;

public:
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual std::string Custom600Headers() const;
   virtual void Done(std::string Message,unsigned long long Size,
                     HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);

   /** \brief Create a new pkgAcqMetaClearSig. */
   pkgAcqMetaClearSig(pkgAcquire *Owner,
		std::string const &URI, std::string const &URIDesc, std::string const &ShortDesc,
		std::string const &MetaIndexURI, std::string const &MetaIndexURIDesc, std::string const &MetaIndexShortDesc,
		std::string const &MetaSigURI, std::string const &MetaSigURIDesc, std::string const &MetaSigShortDesc,
		const std::vector<IndexTarget*>* IndexTargets,
		indexRecords* MetaIndexParser);
   virtual ~pkgAcqMetaClearSig();
};
									/*}}}*/


/** \brief Common base class for all classes that deal with fetching 	{{{
           indexes
 */
class pkgAcqBaseIndex : public pkgAcquire::Item
{
   void *d;

 protected:
   /** \brief Pointer to the IndexTarget data
    */
   const struct IndexTarget * Target;

   /** \brief Pointer to the indexRecords parser */
   indexRecords *MetaIndexParser;

   /** \brief The MetaIndex Key */
   std::string MetaKey;

   /** \brief The URI of the index file to recreate at our end (either
    *  by downloading it or by applying partial patches).
    */
   std::string RealURI;

   bool VerifyHashByMetaKey(HashStringList const &Hashes);

   pkgAcqBaseIndex(pkgAcquire *Owner,
                   pkgAcqMetaBase *TransactionManager,
                   struct IndexTarget const * const Target,
                   HashStringList const &ExpectedHashes,
                   indexRecords *MetaIndexParser)
      : Item(Owner, ExpectedHashes, TransactionManager), Target(Target), 
        MetaIndexParser(MetaIndexParser) {};
};
									/*}}}*/
/** \brief An item that is responsible for fetching an index file of	{{{
 *  package list diffs and starting the package list's download.
 *
 *  This item downloads the Index file and parses it, then enqueues
 *  additional downloads of either the individual patches (using
 *  pkgAcqIndexDiffs) or the entire Packages file (using pkgAcqIndex).
 *
 *  \sa pkgAcqIndexDiffs, pkgAcqIndex
 */
class pkgAcqDiffIndex : public pkgAcqBaseIndex
{
   void *d;

 protected:
   /** \brief If \b true, debugging information will be written to std::clog. */
   bool Debug;

   /** \brief The index file which will be patched to generate the new
    *  file.
    */
   std::string CurrentPackagesFile;

   /** \brief A description of the Packages file (stored in
    *  pkgAcquire::ItemDesc::Description).
    */
   std::string Description;

   /** \brief If the copy step of the packages file is done
    */
   bool PackagesFileReadyInPartial;

 public:
   // Specialized action members
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string DescURI() {return RealURI + "Index";};
   virtual std::string Custom600Headers() const;

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
   bool ParseDiffIndex(std::string IndexDiffFile);
   

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
    *  \param ExpectedHashes The list file's hashsums which are expected.
    */
   pkgAcqDiffIndex(pkgAcquire *Owner,
                   pkgAcqMetaBase *TransactionManager,
                   struct IndexTarget const * const Target,
                   HashStringList const &ExpectedHashes,
                   indexRecords *MetaIndexParser);
};
									/*}}}*/
/** \brief An item that is responsible for fetching client-merge patches {{{
 *  that need to be applied to a given package index file.
 *
 *  Instead of downloading and applying each patch one by one like its
 *  sister #pkgAcqIndexDiffs this class will download all patches at once
 *  and call rred with all the patches downloaded once. Rred will then
 *  merge and apply them in one go, which should be a lot faster – but is
 *  incompatible with server-based merges of patches like reprepro can do.
 *
 *  \sa pkgAcqDiffIndex, pkgAcqIndex
 */
class pkgAcqIndexMergeDiffs : public pkgAcqBaseIndex
{
   void *d;

   protected:

   /** \brief If \b true, debugging output will be written to
    *  std::clog.
    */
   bool Debug;

   /** \brief description of the file being downloaded. */
   std::string Description;

   /** \brief information about the current patch */
   struct DiffInfo const patch;

   /** \brief list of all download items for the patches */
   std::vector<pkgAcqIndexMergeDiffs*> const * const allPatches;

   /** The current status of this patch. */
   enum DiffState
   {
      /** \brief The diff is currently being fetched. */
      StateFetchDiff,

      /** \brief The diff is currently being applied. */
      StateApplyDiff,

      /** \brief the work with this diff is done */
      StateDoneDiff,

      /** \brief something bad happened and fallback was triggered */
      StateErrorDiff
   } State;

   public:
   /** \brief Called when the patch file failed to be downloaded.
    *
    *  This method will fall back to downloading the whole index file
    *  outright; its arguments are ignored.
    */
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &Hashes,
	 pkgAcquire::MethodConfig *Cnf);
   virtual std::string DescURI() {return RealURI + "Index";};

   /** \brief Create an index merge-diff item.
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
    *  \param ExpectedHashes The expected md5sum of the completely
    *  reconstructed package index file; the index file will be tested
    *  against this value when it is entirely reconstructed.
    *
    *  \param patch contains infos about the patch this item is supposed
    *  to download which were read from the index
    *
    *  \param allPatches contains all related items so that each item can
    *  check if it was the last one to complete the download step
    */
   pkgAcqIndexMergeDiffs(pkgAcquire *Owner,
                         pkgAcqMetaBase *TransactionManager,
                         struct IndexTarget const * const Target,
                         HashStringList const &ExpectedHash,
                         indexRecords *MetaIndexParser,
                         DiffInfo const &patch, 
                         std::vector<pkgAcqIndexMergeDiffs*> const * const allPatches);
};
									/*}}}*/
/** \brief An item that is responsible for fetching server-merge patches {{{
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
class pkgAcqIndexDiffs : public pkgAcqBaseIndex
{
   void *d;

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
   APT_HIDDEN bool QueueNextDiff();

   /** \brief Handle tasks that must be performed after the item
    *  finishes downloading.
    *
    *  Dequeues the item and checks the resulting file's hashsums
    *  against ExpectedHashes after the last patch was applied.
    *  There is no need to check the md5/sha1 after a "normal" 
    *  patch because QueueNextDiff() will check the sha1 later.
    *
    *  \param allDone If \b true, the file was entirely reconstructed,
    *  and its md5sum is verified. 
    */
   APT_HIDDEN void Finish(bool allDone=false);

   protected:

   /** \brief If \b true, debugging output will be written to
    *  std::clog.
    */
   bool Debug;

   /** A description of the file being downloaded. */
   std::string Description;

   /** The patches that remain to be downloaded, including the patch
    *  being downloaded right now.  This list should be ordered so
    *  that each diff appears before any diff that depends on it.
    *
    *  \todo These are indexed by sha1sum; why not use some sort of
    *  dictionary instead of relying on ordering and stripping them
    *  off the front?
    */
   std::vector<DiffInfo> available_patches;

   /** The current status of this patch. */
   enum DiffState
     {
	/** \brief The diff is in an unknown state. */
	 StateFetchUnkown,

	 /** \brief The diff is currently being fetched. */
	 StateFetchDiff,
	 
	 /** \brief The diff is currently being uncompressed. */
	 StateUnzipDiff, // FIXME: No longer used

	 /** \brief The diff is currently being applied. */
	 StateApplyDiff
   } State;

   public:
   
   /** \brief Called when the patch file failed to be downloaded.
    *
    *  This method will fall back to downloading the whole index file
    *  outright; its arguments are ignored.
    */
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);

   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string DescURI() {return RealURI + "IndexDiffs";};

   /** \brief Create an index diff item.
    *
    *  After filling in its basic fields, this invokes Finish(true) if
    *  \a diffs is empty, or QueueNextDiff() otherwise.
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
    *  \param ExpectedHashes The expected hashsums of the completely
    *  reconstructed package index file; the index file will be tested
    *  against this value when it is entirely reconstructed.
    *
    *  \param diffs The remaining diffs from the index of diffs.  They
    *  should be ordered so that each diff appears before any diff
    *  that depends on it.
    */
   pkgAcqIndexDiffs(pkgAcquire *Owner,
                    pkgAcqMetaBase *TransactionManager,
                    struct IndexTarget const * const Target,
                    HashStringList const &ExpectedHash,
                    indexRecords *MetaIndexParser,
		    std::vector<DiffInfo> diffs=std::vector<DiffInfo>());
};
									/*}}}*/
/** \brief An acquire item that is responsible for fetching an index	{{{
 *  file (e.g., Packages or Sources).
 *
 *  \sa pkgAcqDiffIndex, pkgAcqIndexDiffs, pkgAcqIndexTrans
 *
 *  \todo Why does pkgAcqIndex have protected members?
 */
class pkgAcqIndex : public pkgAcqBaseIndex
{
   void *d;

   protected:

   /** \brief The stages the method goes through
    *
    *  The method first downloads the indexfile, then its decompressed (or
    *  copied) and verified
    */
   enum AllStages {
      STAGE_DOWNLOAD,
      STAGE_DECOMPRESS_AND_VERIFY,
   };
   AllStages Stage;

   /** \brief Handle what needs to be done when the download is done */
   void StageDownloadDone(std::string Message,
                          HashStringList const &Hashes,
                          pkgAcquire::MethodConfig *Cfg);

   /** \brief Handle what needs to be done when the decompression/copy is
    *         done 
    */
   void StageDecompressDone(std::string Message,
                            HashStringList const &Hashes,
                            pkgAcquire::MethodConfig *Cfg);

   /** \brief If \b set, this partially downloaded file will be
    *  removed when the download completes.
    */
   std::string EraseFileName;

   /** \brief The compression-related file extensions that are being
    *  added to the downloaded file one by one if first fails (e.g., "gz bz2").
    */
   std::string CompressionExtensions;

   /** \brief The actual compression extension currently used */
   std::string CurrentCompressionExtension;

   /** \brief Do the changes needed to fetch via AptByHash (if needed) */
   void InitByHashIfNeeded(const std::string MetaKey);

   /** \brief Auto select the right compression to use */
   void AutoSelectCompression();

   /** \brief Get the full pathname of the final file for the current URI
    */
   std::string GetFinalFilename() const;

   /** \brief Schedule file for verification after a IMS hit */
   void ReverifyAfterIMS();

   /** \brief Validate the downloaded index file */
   bool ValidateFile(const std::string &FileName);

   public:
   
   // Specialized action members
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, 
                     HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string Custom600Headers() const;
   virtual std::string DescURI() {return Desc.URI;};

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
    *  \param ExpectedHashes The expected hashsum of this index file.
    *
    *  \param compressExt The compression-related extension with which
    *  this index file should be downloaded, or "" to autodetect
    *  Compression types can be set with config Acquire::CompressionTypes,
    *  default is ".lzma" or ".bz2" (if the needed binaries are present)
    *  fallback is ".gz" or none.
    */
   pkgAcqIndex(pkgAcquire *Owner,std::string URI,std::string URIDesc,
	       std::string ShortDesc, HashStringList const &ExpectedHashes);
   pkgAcqIndex(pkgAcquire *Owner, pkgAcqMetaBase *TransactionManager,
               IndexTarget const * const Target,
               HashStringList const &ExpectedHash,
               indexRecords *MetaIndexParser);
               
   void Init(std::string const &URI, std::string const &URIDesc,
             std::string const &ShortDesc);
};
									/*}}}*/
/** \brief Information about an index file. */				/*{{{*/
class IndexTarget
{
   void *d;

 public:
   /** \brief A URI from which the index file can be downloaded. */
   std::string URI;

   /** \brief A description of the index file. */
   std::string Description;

   /** \brief A shorter description of the index file. */
   std::string ShortDesc;

   /** \brief The key by which this index file should be
    *  looked up within the meta signature file.
    */
   std::string MetaKey;

   virtual bool IsOptional() const {
      return false;
   }
};
									/*}}}*/
/** \brief Information about an optional index file. */			/*{{{*/
class OptionalIndexTarget : public IndexTarget
{
   void *d;

   virtual bool IsOptional() const {
      return true;
   }
};
									/*}}}*/
/** \brief An item that is responsible for fetching a package file.	{{{
 *
 *  If the package file already exists in the cache, nothing will be
 *  done.
 */
class pkgAcqArchive : public pkgAcquire::Item
{
   void *d;

   protected:
   /** \brief The package version being fetched. */
   pkgCache::VerIterator Version;

   /** \brief The list of sources from which to pick archives to
    *  download this package from.
    */
   pkgSourceList *Sources;

   /** \brief A package records object, used to look up the file
    *  corresponding to each version of the package.
    */
   pkgRecords *Recs;

   /** \brief A location in which the actual filename of the package
    *  should be stored.
    */
   std::string &StoreFilename;

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
   
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string DescURI() {return Desc.URI;};
   virtual std::string ShortDesc() {return Desc.ShortDesc;};
   virtual void Finished();
   virtual bool IsTrusted() const;
   
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
    *  \param[out] StoreFilename A location in which the actual filename of
    *  the package should be stored.  It will be set to a guessed
    *  basename in the constructor, and filled in with a fully
    *  qualified filename once the download finishes.
    */
   pkgAcqArchive(pkgAcquire *Owner,pkgSourceList *Sources,
		 pkgRecords *Recs,pkgCache::VerIterator const &Version,
		 std::string &StoreFilename);
};
									/*}}}*/
/** \brief Retrieve an arbitrary file to the current directory.		{{{
 *
 *  The file is retrieved even if it is accessed via a URL type that
 *  normally is a NOP, such as "file".  If the download fails, the
 *  partial file is renamed to get a ".FAILED" extension.
 */
class pkgAcqFile : public pkgAcquire::Item
{
   void *d;

   /** \brief How many times to retry the download, set from
    *  Acquire::Retries.
    */
   unsigned int Retries;
   
   /** \brief Should this file be considered a index file */
   bool IsIndexFile;

   public:
   
   // Specialized action members
   virtual void Failed(std::string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(std::string Message,unsigned long long Size, HashStringList const &CalcHashes,
		     pkgAcquire::MethodConfig *Cnf);
   virtual std::string DescURI() {return Desc.URI;};
   virtual std::string Custom600Headers() const;

   /** \brief Create a new pkgAcqFile object.
    *
    *  \param Owner The pkgAcquire object with which this object is
    *  associated.
    *
    *  \param URI The URI to download.
    *
    *  \param Hashes The hashsums of the file to download, if they are known;
    *  otherwise empty list.
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
    *  \param IsIndexFile The file is considered a IndexFile and cache-control
    *                     headers like "cache-control: max-age=0" are send
    *
    * If DestFilename is empty, download to DestDir/\<basename\> if
    * DestDir is non-empty, $CWD/\<basename\> otherwise.  If
    * DestFilename is NOT empty, DestDir is ignored and DestFilename
    * is the absolute name to which the file should be downloaded.
    */

   pkgAcqFile(pkgAcquire *Owner, std::string URI, HashStringList const &Hashes, unsigned long long Size,
	      std::string Desc, std::string ShortDesc,
	      const std::string &DestDir="", const std::string &DestFilename="",
	      bool IsIndexFile=false);
};
									/*}}}*/
/** @} */

#endif
