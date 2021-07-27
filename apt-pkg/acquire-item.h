// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/weakptr.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>


/** \addtogroup acquire
 *  @{
 *
 *  \file acquire-item.h
 */

class pkgRecords;
class pkgSourceList;
class pkgAcqMetaClearSig;
class pkgAcqIndexMergeDiffs;
class metaIndex;

class APT_PUBLIC pkgAcquire::Item : public WeakPointable				/*{{{*/
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
{
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

   /** \brief contains the name of the subprocess that is operating on this object
    * (for instance, "gzip", "rred" or "gpgv"). This is obsoleting #Mode from above
    * as it can manage the lifetime of included string properly. */
   std::string ActiveSubprocess;

   /** \brief A client-supplied unique identifier.
    *
    *  This field is initialized to 0; it is meant to be filled in by
    *  clients that wish to use it to uniquely identify items.
    *
    *  APT progress reporting will store an ID there as shown in "Get:42 …"
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

   /** \brief Number of retries */
   unsigned int Retries;

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
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf);
   APT_HIDDEN void FailMessage(std::string const &Message);

   /** \brief Invoked by the acquire worker to check if the successfully
    * fetched object is also the objected we wanted to have.
    *
    *  Note that the object might \e not have been written to
    *  DestFile; check for the presence of an Alt-Filename entry in
    *  Message to find the file to which it was really written.
    *
    *  This is called before Done is called and can prevent it by returning
    *  \b false which will result in Failed being called instead.
    *
    *  You should prefer to use this method over calling Failed() from Done()
    *  as this has e.g. the wrong progress reporting.
    *
    *  \param Message Data from the acquire method.  Use LookupTag()
    *  to parse it.
    *  \param Cnf The method via which the object was fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual bool VerifyDone(std::string const &Message,
	 pkgAcquire::MethodConfig const * const Cnf);

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
    *  \param Hashes The HashSums of the object that was fetched.
    *  \param Cnf The method via which the object was fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf);

   /** \brief Invoked when the worker starts to fetch this object.
    *
    *  \param Message RFC822-formatted data from the worker process.
    *  Use LookupTag() to parse it.
    *
    *  \param Hashes The expected hashes of the object being fetched.
    *
    *  \sa pkgAcqMethod
    */
   virtual void Start(std::string const &Message, unsigned long long const Size);

   /** \brief Custom headers to be sent to the fetch process.
    *
    *  \return a string containing RFC822-style headers that are to be
    *  inserted into the 600 URI Acquire message sent to the fetch
    *  subprocess.  The headers are inserted after a newline-less
    *  line, so they should (if nonempty) have a leading newline and
    *  no trailing newline.
    */
   virtual std::string Custom600Headers() const;
   // this is more a hack than a proper external interface, hence hidden
   APT_HIDDEN std::unordered_map<std::string, std::string> &ModifyCustomFields();
   // this isn't the super nicest interface either…
   APT_HIDDEN bool PopAlternativeURI(std::string &NewURI);
   APT_HIDDEN bool IsGoodAlternativeURI(std::string const &AltUri) const;
   APT_HIDDEN void PushAlternativeURI(std::string &&NewURI, std::unordered_map<std::string, std::string> &&fields, bool const at_the_back);
   APT_HIDDEN void RemoveAlternativeSite(std::string &&OldSite);

   /** \brief A "descriptive" URI-like string.
    *
    *  \return a URI that should be used to describe what is being fetched.
    */
   virtual std::string DescURI() const = 0;
   /** \brief Short item description.
    *
    *  \return a brief description of the object being fetched.
    */
   virtual std::string ShortDesc() const;

   /** \brief Invoked by the worker when the download is completely done. */
   virtual void Finished();

   /** \return HashSums the DestFile is supposed to have in this stage */
   virtual HashStringList GetExpectedHashes() const = 0;
   /** \return the 'best' hash for display proposes like --print-uris */
   std::string HashSum() const;

   /** \return if having no hashes is a hard failure or not
    *
    * Idealy this is always \b true for every subclass, but thanks to
    * historical grow we don't have hashes for all files in all cases
    * in all steps, so it is slightly more complicated than it should be.
    */
   virtual bool HashesRequired() const { return true; }

   /** \return the acquire process with which this item is associated. */
   pkgAcquire *GetOwner() const;
   pkgAcquire::ItemDesc &GetItemDesc();

   /** \return \b true if this object is being fetched from a trusted source. */
   virtual bool IsTrusted() const;

   /** \brief Set the name of the current active subprocess
    *
    *  See also #ActiveSubprocess
    */
   void SetActiveSubprocess(std::string const &subprocess);

   /** \brief Initialize an item.
    *
    *  Adds the item to the list of items known to the acquire
    *  process, but does not place it into any fetch queues (you must
    *  manually invoke QueueURI() to do so).
    *
    *  \param Owner The new owner of this item.
    */
   explicit Item(pkgAcquire * const Owner);

   /** \brief Remove this item from its owner's queue by invoking
    *  pkgAcquire::Remove.
    */
   virtual ~Item();

   bool APT_HIDDEN IsRedirectionLoop(std::string const &NewURI);
   /** \brief The priority of the item, used for queuing */
   int APT_HIDDEN Priority();

   /** \brief internal clock definitions to avoid typing all that all over the place */
   void APT_HIDDEN FetchAfter(time_point FetchAfter);
   time_point APT_HIDDEN FetchAfter();

   protected:
   /** \brief The acquire object with which this item is associated. */
   pkgAcquire * const Owner;

   /** \brief The item that is currently being downloaded. */
   pkgAcquire::ItemDesc Desc;

   enum RenameOnErrorState {
      HashSumMismatch,
      SizeMismatch,
      InvalidFormat,
      SignatureError,
      NotClearsigned,
      MaximumSizeExceeded,
      PDiffError,
   };

   /** \brief Rename failed file and set error
    *
    * \param state respresenting the error we encountered
    */
   bool RenameOnError(RenameOnErrorState const state);

   /** \brief Insert this item into its owner's queue.
    *
    *  The method is designed to check if the request would end
    *  in an IMSHit and if it determines that it would, it isn't
    *  queueing the Item and instead sets it to completion instantly.
    *
    *  \param Item Metadata about this item (its URI and
    *  description).
    *  \return true if the item was inserted, false if IMSHit was detected
    */
   virtual bool QueueURI(ItemDesc &Item);

   /** \brief Remove this item from its owner's queue. */
   void Dequeue();

   /** \brief Rename a file without modifying its timestamp.
    *
    *  Many item methods call this as their final action.
    *
    *  \param From The file to be renamed.
    *
    *  \param To The new name of \a From.  If \a To exists it will be
    *  overwritten. If \a From and \a To are equal nothing happens.
    */
   bool Rename(std::string const &From, std::string const &To);

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const;

   private:
   class Private;
   Private * const d;

   friend class pkgAcqMetaBase;
   friend class pkgAcqMetaClearSig;
};
									/*}}}*/
class APT_HIDDEN pkgAcqTransactionItem: public pkgAcquire::Item		/*{{{*/
/** \brief baseclass for the indexes files to manage them all together */
{
   void * const d;
   protected:
   HashStringList GetExpectedHashesFor(std::string const &MetaKey) const;

   bool QueueURI(pkgAcquire::ItemDesc &Item) APT_OVERRIDE;

   public:
   IndexTarget const Target;

   /** \brief storge name until a transaction is finished */
   std::string PartialFile;

   /** \brief TransactionManager */
   pkgAcqMetaClearSig * const TransactionManager;

   enum TransactionStates {
      TransactionStarted,
      TransactionCommit,
      TransactionAbort,
   };
   virtual bool TransactionState(TransactionStates const state);

   virtual std::string DescURI() const APT_OVERRIDE { return Target.URI; }
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual std::string GetMetaKey() const;
   virtual bool HashesRequired() const APT_OVERRIDE;
   virtual bool AcquireByHash() const;

   pkgAcqTransactionItem(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager, IndexTarget const &Target) APT_NONNULL(2, 3);
   virtual ~pkgAcqTransactionItem();

   friend class pkgAcqMetaBase;
   friend class pkgAcqMetaClearSig;
};
									/*}}}*/
class APT_HIDDEN pkgAcqMetaBase : public pkgAcqTransactionItem		/*{{{*/
/** \brief the manager of a transaction */
{
   void * const d;
 protected:
   std::vector<pkgAcqTransactionItem*> Transaction;

   /** \brief If \b true, the index's signature is currently being verified.
    */
   bool AuthPass;

   /** \brief Called when a file is finished being retrieved.
    *
    *  If the file was not downloaded to DestFile, a copy process is
    *  set up to copy it to DestFile; otherwise, Complete is set to \b
    *  true and the file is moved to its final location.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    */
   bool CheckDownloadDone(pkgAcqTransactionItem * const I, const std::string &Message, HashStringList const &Hashes) const;

   /** \brief Queue the downloaded Signature for verification */
   void QueueForSignatureVerify(pkgAcqTransactionItem * const I, std::string const &File, std::string const &Signature);

   virtual std::string Custom600Headers() const APT_OVERRIDE;

   /** \brief Called when authentication succeeded.
    *
    *  Sanity-checks the authenticated file, queues up the individual
    *  index files for download, and saves the signature in the lists
    *  directory next to the authenticated list file.
    *
    *  \param Message The message block received from the fetch
    *  subprocess.
    *  \param Cnf The method and its configuration which handled the request
    */
   bool CheckAuthDone(std::string const &Message, pkgAcquire::MethodConfig const *const Cnf);

   /** Check if the current item should fail at this point */
   bool CheckStopAuthentication(pkgAcquire::Item * const I, const std::string &Message);

   /** \brief Check that the release file is a release file for the
    *  correct distribution.
    *
    *  \return \b true if no fatal errors were encountered.
    */
   bool VerifyVendor(std::string const &Message);

   virtual bool TransactionState(TransactionStates const state) APT_OVERRIDE;

 public:
   // This refers more to the Transaction-Manager than the actual file
   bool IMSHit;
   TransactionStates State;
   std::string BaseURI;

   virtual bool QueueURI(pkgAcquire::ItemDesc &Item) APT_OVERRIDE;
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual bool HashesRequired() const APT_OVERRIDE;

   // transaction code
   void Add(pkgAcqTransactionItem * const I);
   void AbortTransaction();
   bool TransactionHasError() const;
   void CommitTransaction();

   /** \brief Stage (queue) a copy action when the transaction is committed
    */
   void TransactionStageCopy(pkgAcqTransactionItem * const I,
                             const std::string &From,
                             const std::string &To);
   /** \brief Stage (queue) a removal action when the transaction is committed
    */
   void TransactionStageRemoval(pkgAcqTransactionItem * const I, const std::string &FinalFile);

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;

   pkgAcqMetaBase(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
		  IndexTarget const &DataTarget) APT_NONNULL(2, 3);
   virtual ~pkgAcqMetaBase();
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
class APT_HIDDEN pkgAcqMetaIndex : public pkgAcqMetaBase
{
   void * const d;
   protected:
   IndexTarget const DetachedSigTarget;

   /** \brief delayed constructor */
   void Init(std::string const &URIDesc, std::string const &ShortDesc);

   public:
   virtual std::string DescURI() const APT_OVERRIDE;

   // Specialized action members
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;

   /** \brief Create a new pkgAcqMetaIndex. */
   pkgAcqMetaIndex(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
		   IndexTarget const &DataTarget, IndexTarget const &DetachedSigTarget) APT_NONNULL(2, 3);
   virtual ~pkgAcqMetaIndex();

   friend class pkgAcqMetaSig;
};
									/*}}}*/
/** \brief An acquire item that downloads the detached signature	{{{
 *  of a meta-index (Release) file, then queues up the release
 *  file itself.
 *
 *  \todo Why protected members?
 *
 *  \sa pkgAcqMetaIndex
 */
class APT_HIDDEN pkgAcqMetaSig : public pkgAcqTransactionItem
{
   void * const d;

   pkgAcqMetaIndex * const MetaIndex;

   /** \brief The file we use to verify the MetaIndexFile with (not always set!) */
   std::string MetaIndexFileSignature;

   protected:

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;

   public:
   virtual bool HashesRequired() const APT_OVERRIDE { return false; }

   // Specialized action members
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;

   /** \brief Create a new pkgAcqMetaSig. */
   pkgAcqMetaSig(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
	 IndexTarget const &Target, pkgAcqMetaIndex * const MetaIndex) APT_NONNULL(2, 3, 5);
   virtual ~pkgAcqMetaSig();
};
									/*}}}*/
/** \brief An item responsible for downloading clearsigned metaindexes	{{{*/
class APT_HIDDEN pkgAcqMetaClearSig : public pkgAcqMetaIndex
{
   void * const d;
   IndexTarget const DetachedDataTarget;

 public:
   /** \brief A package-system-specific parser for the meta-index file. */
   metaIndex *MetaIndexParser;
   metaIndex *LastMetaIndexParser;

   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;
   virtual bool VerifyDone(std::string const &Message, pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Finished() APT_OVERRIDE;

   /** \brief Starts downloading the individual index files.
    *
    *  \param verify If \b true, only indices whose expected hashsum
    *  can be determined from the meta-index will be downloaded, and
    *  the hashsums of indices will be checked (reporting
    *  #StatAuthError if there is a mismatch).  If verify is \b false,
    *  no hashsum checking will be performed.
    */
   void QueueIndexes(bool const verify);

   /** \brief Create a new pkgAcqMetaClearSig. */
   pkgAcqMetaClearSig(pkgAcquire * const Owner,
		IndexTarget const &ClearsignedTarget,
		IndexTarget const &DetachedDataTarget,
		IndexTarget const &DetachedSigTarget,
		metaIndex * const MetaIndexParser);
   virtual ~pkgAcqMetaClearSig();
};
									/*}}}*/
/** \brief Common base class for all classes that deal with fetching indexes	{{{*/
class APT_HIDDEN pkgAcqBaseIndex : public pkgAcqTransactionItem
{
   void * const d;

 public:
   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;

   pkgAcqBaseIndex(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
                   IndexTarget const &Target) APT_NONNULL(2, 3);
   virtual ~pkgAcqBaseIndex();
};
									/*}}}*/
/** \brief An acquire item that is responsible for fetching an index	{{{
 *  file (e.g., Packages or Sources).
 *
 *  \sa pkgAcqDiffIndex, pkgAcqIndexDiffs, pkgAcqIndexTrans
 *
 *  \todo Why does pkgAcqIndex have protected members?
 */
class APT_HIDDEN pkgAcqIndex : public pkgAcqBaseIndex
{
   void * const d;

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
   void StageDownloadDone(std::string const &Message);

   /** \brief Handle what needs to be done when the decompression/copy is
    *         done
    */
   void StageDecompressDone();

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
   void InitByHashIfNeeded();

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;

   virtual bool TransactionState(TransactionStates const state) APT_OVERRIDE;

   public:
   // Specialized action members
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Desc.URI;};
   virtual std::string GetMetaKey() const APT_OVERRIDE;

   pkgAcqIndex(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
               IndexTarget const &Target, bool const Derived = false) APT_NONNULL(2, 3);
   virtual ~pkgAcqIndex();

   protected:
   APT_HIDDEN void Init(std::string const &URI, std::string const &URIDesc,
             std::string const &ShortDesc);
   APT_HIDDEN bool CommonFailed(std::string const &TargetURI,
				std::string const &Message, pkgAcquire::MethodConfig const *const Cnf);
};
									/*}}}*/
struct APT_HIDDEN DiffInfo {						/*{{{*/
   /** The filename of the diff. */
   std::string file;

   /** The hashes of the file after the diff is applied */
   HashStringList result_hashes;

   /** The hashes of the diff */
   HashStringList patch_hashes;

   /** The hashes of the compressed diff */
   HashStringList download_hashes;
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
class APT_HIDDEN pkgAcqDiffIndex : public pkgAcqIndex
{
   void * const d;
   std::vector<pkgAcqIndexMergeDiffs*> * diffs;
   std::vector<DiffInfo> available_patches;
   bool pdiff_merge;

 protected:
   /** \brief If \b true, debugging information will be written to std::clog. */
   bool Debug;

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;

   virtual bool QueueURI(pkgAcquire::ItemDesc &Item) APT_OVERRIDE;

   virtual bool TransactionState(TransactionStates const state) APT_OVERRIDE;
 public:
   // Specialized action members
   virtual void Failed(std::string const &Message, pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual bool VerifyDone(std::string const &Message, pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Target.URI + "Index";};
   virtual std::string GetMetaKey() const APT_OVERRIDE;

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
   bool ParseDiffIndex(std::string const &IndexDiffFile);

   /** \brief Create a new pkgAcqDiffIndex.
    *
    *  \param Owner The Acquire object that owns this item.
    *
    *  \param URI The URI of the list file to download.
    *
    *  \param URIDesc A long description of the list file to download.
    *
    *  \param ShortDesc A short description of the list file to download.
    */
   pkgAcqDiffIndex(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager,
                   IndexTarget const &Target) APT_NONNULL(2, 3);
   virtual ~pkgAcqDiffIndex();
 private:
   APT_HIDDEN void QueueOnIMSHit() const;
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
class APT_HIDDEN pkgAcqIndexMergeDiffs : public pkgAcqBaseIndex
{
   protected:

   /** \brief If \b true, debugging output will be written to
    *  std::clog.
    */
   bool Debug;

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
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
	 pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Target.URI + "Index";};
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual bool HashesRequired() const APT_OVERRIDE;
   virtual bool AcquireByHash() const APT_OVERRIDE;

   /** \brief Create an index merge-diff item.
    *
    *  \param Owner The pkgAcquire object that owns this item.
    *  \param TransactionManager responsible for this item
    *  \param Target we intend to built via pdiff patching
    *  \param baseURI is the URI used for the Index, but stripped down to Target
    *  \param DiffInfo of the patch in question
    *  \param patch contains infos about the patch this item is supposed
    *  to download which were read from the index
    *  \param allPatches contains all related items so that each item can
    *  check if it was the last one to complete the download step
    */
   pkgAcqIndexMergeDiffs(pkgAcquire *const Owner, pkgAcqMetaClearSig *const TransactionManager,
			 IndexTarget const &Target, DiffInfo const &patch,
			 std::vector<pkgAcqIndexMergeDiffs *> const *const allPatches) APT_NONNULL(2, 3, 6);
   virtual ~pkgAcqIndexMergeDiffs();
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
class APT_HIDDEN pkgAcqIndexDiffs : public pkgAcqBaseIndex
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
   APT_HIDDEN void Finish(bool const allDone=false);

   protected:

   /** \brief If \b true, debugging output will be written to
    *  std::clog.
    */
   bool Debug;

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
	 /** \brief The diff is currently being fetched. */
	 StateFetchDiff,

	 /** \brief The diff is currently being applied. */
	 StateApplyDiff
   } State;

   public:

   /** \brief Called when the patch file failed to be downloaded.
    *
    *  This method will fall back to downloading the whole index file
    *  outright; its arguments are ignored.
    */
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;

   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Target.URI + "IndexDiffs";};
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual bool HashesRequired() const APT_OVERRIDE;
   virtual bool AcquireByHash() const APT_OVERRIDE;

   /** \brief Create an index diff item.
    *
    *  After filling in its basic fields, this invokes Finish(true) if
    *  \a diffs is empty, or QueueNextDiff() otherwise.
    *
    *  \param Owner The pkgAcquire object that owns this item.
    *  \param TransactionManager responsible for this item
    *  \param Target we want to built via pdiff patching
    *  \param baseURI is the URI used for the Index, but stripped down to Target
    *  \param diffs The remaining diffs from the index of diffs.  They
    *  should be ordered so that each diff appears before any diff
    *  that depends on it.
    */
   pkgAcqIndexDiffs(pkgAcquire *const Owner, pkgAcqMetaClearSig *const TransactionManager,
		    IndexTarget const &Target,
		    std::vector<DiffInfo> const &diffs = std::vector<DiffInfo>()) APT_NONNULL(2, 3);
   virtual ~pkgAcqIndexDiffs();
};
									/*}}}*/
/** \brief An item that is responsible for fetching a package file.	{{{
 *
 *  If the package file already exists in the cache, nothing will be
 *  done.
 */
class APT_PUBLIC pkgAcqArchive : public pkgAcquire::Item
{
   void * const d;

   bool LocalSource;
   HashStringList ExpectedHashes;

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

   /** \brief \b true if this version file is being downloaded from a
    *  trusted source.
    */
   bool Trusted;

   /** \brief Queue up the next available file for this version. */
   bool QueueNext();

   /** \brief Get the full pathname of the final file for the current URI */
   virtual std::string GetFinalFilename() const APT_OVERRIDE;

   public:

   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &Hashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE;
   virtual std::string ShortDesc() const APT_OVERRIDE;
   virtual void Finished() APT_OVERRIDE;
   virtual bool IsTrusted() const APT_OVERRIDE;
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual bool HashesRequired() const APT_OVERRIDE;

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
   pkgAcqArchive(pkgAcquire * const Owner,pkgSourceList * const Sources,
		 pkgRecords * const Recs,pkgCache::VerIterator const &Version,
		 std::string &StoreFilename);
   virtual ~pkgAcqArchive();
};
									/*}}}*/
/** \brief Retrieve the changelog for the given version			{{{
 *
 *  Downloads the changelog to a temporary file it will also remove again
 *  while it is deconstructed or downloads it to a named location.
 */
class APT_PUBLIC pkgAcqChangelog : public pkgAcquire::Item
{
   class Private;
   Private * const d;
   std::string TemporaryDirectory;
   std::string const SrcName;
   std::string const SrcVersion;

   public:
   // we will never have hashes for changelogs.
   // If you need verified ones, download the deb and extract the changelog.
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE { return HashStringList(); }
   virtual bool HashesRequired() const APT_OVERRIDE { return false; }

   // Specialized action members
   virtual void Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &CalcHashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Desc.URI;};

   /** returns the URI to the changelog of this version
    *
    * @param Ver is the version to get the changelog for
    * @return the URI which will be used to acquire the changelog
    */
   static std::string URI(pkgCache::VerIterator const &Ver);

   /** returns the URI to the changelog of this version
    *
    *  \param Rls is the Release file the package comes from
    *  \param Component in which the package resides, can be empty
    *  \param SrcName is the source package name
    *  \param SrcVersion is the source package version
    * @return the URI which will be used to acquire the changelog
    */
   static std::string URI(pkgCache::RlsFileIterator const &Rls,
	 char const * const Component, char const * const SrcName,
	 char const * const SrcVersion);

   /** returns the URI to the changelog of this version
    *
    *  \param Template URI where @CHANGEPATH@ has to be filled in
    *  \param Component in which the package resides, can be empty
    *  \param SrcName is the source package name
    *  \param SrcVersion is the source package version
    * @return the URI which will be used to acquire the changelog
    */
   static std::string URI(std::string const &Template,
	 char const * const Component, char const * const SrcName,
	 char const * const SrcVersion);

   /** returns the URI template for this release file
    *
    *  \param Rls is a Release file
    * @return the URI template to use for this release file
    */
   static std::string URITemplate(pkgCache::RlsFileIterator const &Rls);

   /** \brief Create a new pkgAcqChangelog object.
    *
    *  \param Owner The pkgAcquire object with which this object is
    *  associated.
    *  \param Ver is the version to get the changelog for
    *  \param DestDir The directory the file should be downloaded into.
    *  Will be an autocreated (and cleaned up) temporary directory if not set.
    *  \param DestFilename The filename the file should have in #DestDir
    *  Defaults to sourcepackagename.changelog if not set.
    */
   pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::VerIterator const &Ver,
	 std::string const &DestDir="", std::string const &DestFilename="");

   /** \brief Create a new pkgAcqChangelog object.
    *
    *  \param Owner The pkgAcquire object with which this object is
    *  associated.
    *  \param Rls is the Release file the package comes from
    *  \param Component in which the package resides, can be empty
    *  \param SrcName is the source package name
    *  \param SrcVersion is the source package version
    *  \param DestDir The directory the file should be downloaded into.
    *  Will be an autocreated (and cleaned up) temporary directory if not set.
    *  \param DestFilename The filename the file should have in #DestDir
    *  Defaults to sourcepackagename.changelog if not set.
    */
   pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::RlsFileIterator const &Rls,
	 char const * const Component, char const * const SrcName, char const * const SrcVersion,
	 std::string const &DestDir="", std::string const &DestFilename="");

   /** \brief Create a new pkgAcqChangelog object.
    *
    *  \param Owner The pkgAcquire object with which this object is
    *  associated.
    *  \param URI is to be used to get the changelog
    *  \param SrcName is the source package name
    *  \param SrcVersion is the source package version
    *  \param DestDir The directory the file should be downloaded into.
    *  Will be an autocreated (and cleaned up) temporary directory if not set.
    *  \param DestFilename The filename the file should have in #DestDir
    *  Defaults to sourcepackagename.changelog if not set.
    */
   pkgAcqChangelog(pkgAcquire * const Owner, std::string const &URI,
	 char const * const SrcName, char const * const SrcVersion,
	 std::string const &DestDir="", std::string const &DestFilename="");

   virtual ~pkgAcqChangelog();

private:
   APT_HIDDEN void Init(std::string const &DestDir, std::string const &DestFilename);
};
									/*}}}*/
/** \brief Retrieve an arbitrary file to the current directory.		{{{
 *
 *  The file is retrieved even if it is accessed via a URL type that
 *  normally is a NOP, such as "file".  If the download fails, the
 *  partial file is renamed to get a ".FAILED" extension.
 */
class APT_PUBLIC pkgAcqFile : public pkgAcquire::Item
{
   void * const d;

   /** \brief Should this file be considered a index file */
   bool IsIndexFile;

   HashStringList const ExpectedHashes;
   public:
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE;
   virtual bool HashesRequired() const APT_OVERRIDE;

   // Specialized action members
   virtual void Done(std::string const &Message, HashStringList const &CalcHashes,
		     pkgAcquire::MethodConfig const * const Cnf) APT_OVERRIDE;
   virtual std::string DescURI() const APT_OVERRIDE {return Desc.URI;};
   virtual std::string Custom600Headers() const APT_OVERRIDE;

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

   pkgAcqFile(pkgAcquire * const Owner, std::string const &URI, HashStringList const &Hashes, unsigned long long const Size,
	      std::string const &Desc, std::string const &ShortDesc,
	      std::string const &DestDir="", std::string const &DestFilename="",
	      bool const IsIndexFile=false);
   virtual ~pkgAcqFile();
};
									/*}}}*/
class APT_HIDDEN pkgAcqAuxFile : public pkgAcqFile /*{{{*/
{
   pkgAcquire::Item *const Owner;
   pkgAcquire::Worker *const Worker;
   unsigned long long MaximumSize;

   public:
   virtual void Failed(std::string const &Message, pkgAcquire::MethodConfig const *const Cnf) APT_OVERRIDE;
   virtual void Done(std::string const &Message, HashStringList const &CalcHashes,
		     pkgAcquire::MethodConfig const *const Cnf) APT_OVERRIDE;
   virtual std::string Custom600Headers() const APT_OVERRIDE;
   virtual void Finished() APT_OVERRIDE;

   pkgAcqAuxFile(pkgAcquire::Item *const Owner, pkgAcquire::Worker *const Worker,
		 std::string const &ShortDesc, std::string const &Desc, std::string const &URI,
		 HashStringList const &Hashes, unsigned long long const MaximumSize);
   virtual ~pkgAcqAuxFile();
};
									/*}}}*/
/** @} */

#endif
