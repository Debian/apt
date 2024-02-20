// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Acquire - File Acquiration
   
   This module contains the Acquire system. It is responsible for bringing
   files into the local pathname space. It deals with URIs for files and
   URI handlers responsible for downloading or finding the URIs.
   
   Each file to download is represented by an Acquire::Item class subclassed
   into a specialization. The Item class can add itself to several URI
   acquire queues each prioritized by the download scheduler. When the
   system is run the proper URI handlers are spawned and the acquire
   queues are fed into the handlers by the schedular until the queues are
   empty. This allows for an Item to be downloaded from an alternate source
   if the first try turns out to fail. It also allows concurrent downloading
   of multiple items from multiple sources as well as dynamic balancing
   of load between the sources.
   
   Scheduling of downloads is done on a first ask first get basis. This
   preserves the order of the download as much as possible. And means the
   fastest source will tend to process the largest number of files.
   
   Internal methods and queues for performing gzip decompression,
   md5sum hashing and file copying are provided to allow items to apply
   a number of transformations to the data files they are working with.
   
   ##################################################################### */
									/*}}}*/

/** \defgroup acquire Acquire system					{{{
 *
 *  \brief The Acquire system is responsible for retrieving files from
 *  local or remote URIs and postprocessing them (for instance,
 *  verifying their authenticity).  The core class in this system is
 *  pkgAcquire, which is responsible for managing the download queues
 *  during the download.  There is at least one download queue for
 *  each supported protocol; protocols such as http may provide one
 *  queue per host.
 *
 *  Each file to download is represented by a subclass of
 *  pkgAcquire::Item.  The files add themselves to the download
 *  queue(s) by providing their URI information to
 *  pkgAcquire::Item::QueueURI, which calls pkgAcquire::Enqueue.
 *
 *  Once the system is set up, the Run method will spawn subprocesses
 *  to handle the enqueued URIs; the scheduler will then take items
 *  from the queues and feed them into the handlers until the queues
 *  are empty.
 *
 *  \todo Acquire supports inserting an object into several queues at
 *  once, but it is not clear what its behavior in this case is, and
 *  no subclass of pkgAcquire::Item seems to actually use this
 *  capability.
 */									/*}}}*/

/** \addtogroup acquire
 *
 *  @{
 *
 *  \file acquire.h
 */

#ifndef PKGLIB_ACQUIRE_H
#define PKGLIB_ACQUIRE_H

#include <apt-pkg/hashes.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/weakptr.h>

#include <chrono>
#include <string>
#include <vector>

#include <cstddef>
#include <sys/select.h>
#include <sys/time.h>



class pkgAcquireStatus;
class metaIndex;

/** \brief The core download scheduler.					{{{
 *
 *  This class represents an ongoing download.  It manages the lists
 *  of active and pending downloads and handles setting up and tearing
 *  down download-related structures.
 *
 *  \todo Why all the protected data items and methods?
 */
class APT_PUBLIC pkgAcquire
{   
   private:
   /** \brief The monotonic clock used by the Acquire system */
   using clock = std::chrono::steady_clock;
   /** \brief Time point on our monotonic clock */
   using time_point = std::chrono::time_point<clock>;
   /** \brief FD of the Lock file we acquire in Setup (if any) */
   int LockFD;
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   public:
   
   class Item;
   class Queue;
   class Worker;
   struct MethodConfig;
   struct ItemDesc;
   friend class Item;
   friend class pkgAcqMetaBase;
   friend class Queue;

   typedef std::vector<Item *>::iterator ItemIterator;
   typedef std::vector<Item *>::const_iterator ItemCIterator;

   protected:
   
   /** \brief A list of items to download.
    *
    *  This is built monotonically as items are created and only
    *  emptied when the download shuts down.
    */
   std::vector<Item *> Items;
   
   /** \brief The head of the list of active queues.
    *
    *  \todo why a hand-managed list of queues instead of std::list or
    *  std::set?
    */
   Queue *Queues;

   /** \brief The head of the list of active workers.
    *
    *  \todo why a hand-managed list of workers instead of std::list
    *  or std::set?
    */
   Worker *Workers;

   /** \brief The head of the list of acquire method configurations.
    *
    *  Each protocol (http, ftp, gzip, etc) via which files can be
    *  fetched can have a representation in this list.  The
    *  configuration data is filled in by parsing the 100 Capabilities
    *  string output by a method on startup (see
    *  pkgAcqMethod::pkgAcqMethod and pkgAcquire::GetConfig).
    *
    *  \todo why a hand-managed config dictionary instead of std::map?
    */
   MethodConfig *Configs;

   /** \brief The progress indicator for this download. */
   pkgAcquireStatus *Log;

   /** \brief The number of files which are to be fetched. */
   unsigned long ToFetch;

   // Configurable parameters for the scheduler

   /** \brief Represents the queuing strategy for remote URIs. */
   enum QueueStrategy {
     /** \brief Generate one queue for each protocol/host combination; downloads from 
      *  multiple hosts can proceed in parallel.
      */
     QueueHost,
     /** \brief Generate a single queue for each protocol; serialize
      *  downloads from multiple hosts.
      */
     QueueAccess} QueueMode;

   /** \brief If \b true, debugging information will be dumped to std::clog. */
   bool const Debug;
   /** \brief If \b true, a download is currently in progress. */
   bool Running;

   /** \brief Add the given item to the list of items. */
   void Add(Item *Item);

   /** \brief Remove the given item from the list of items. */
   void Remove(Item *Item);

   /** \brief Add the given worker to the list of workers. */
   void Add(Worker *Work);

   /** \brief Remove the given worker from the list of workers. */
   void Remove(Worker *Work);
   
   /** \brief Insert the given fetch request into the appropriate queue.
    *
    *  \param Item The URI to download and the item to download it
    *  for.  Copied by value into the queue; no reference to Item is
    *  retained.
    */
   void Enqueue(ItemDesc &Item);

   /** \brief Remove all fetch requests for this item from all queues. */
   void Dequeue(Item *Item);

   /** \brief Determine the fetch method and queue of a URI.
    *
    *  \param URI The URI to fetch.
    *
    *  \param[out] Config A location in which to place the method via
    *  which the URI is to be fetched.
    *
    *  \return the string-name of the queue in which a fetch request
    *  for the given URI should be placed.
    */
   std::string QueueName(std::string URI,MethodConfig const *&Config);

   /** \brief Build up the set of file descriptors upon which select() should
    *  block.
    *
    *  The default implementation inserts the file descriptors
    *  corresponding to active downloads.
    *
    *  \param[out] Fd The largest file descriptor in the generated sets.
    *
    *  \param[out] RSet The set of file descriptors that should be
    *  watched for input.
    *
    *  \param[out] WSet The set of file descriptors that should be
    *  watched for output.
    */
   virtual void SetFds(int &Fd,fd_set *RSet,fd_set *WSet);

   /** Handle input from and output to file descriptors which select()
    *  has determined are ready.  The default implementation
    *  dispatches to all active downloads.
    *
    *  \param RSet The set of file descriptors that are ready for
    *  input.
    *
    *  \param WSet The set of file descriptors that are ready for
    *  output.
    *
    * \return false if there is an error condition on one of the fds
    */
   virtual bool RunFds(fd_set *RSet,fd_set *WSet);

   /** \brief Check for idle queues with ready-to-fetch items.
    *
    *  Called by pkgAcquire::Queue::Done each time an item is dequeued
    *  but remains on some queues; i.e., another queue should start
    *  fetching it.
    */
   void Bump();
   
   public:

   /** \brief Retrieve information about a fetch method by name.
    *
    *  \param Access The name of the method to look up.
    *
    *  \return the method whose name is Access, or \b NULL if no such method exists.
    */
   MethodConfig *GetConfig(std::string Access);

   /** \brief Provides information on how a download terminated. */
   enum RunResult {
     /** \brief All files were fetched successfully. */
     Continue,

     /** \brief Some files failed to download. */
     Failed,

     /** \brief The download was cancelled by the user (i.e., #Log's
      *  pkgAcquireStatus::Pulse() method returned \b false).
      */
     Cancelled};

   /** \brief Download all the items that have been Add()ed to this
    *  download process.
    *
    *  This method will block until the download completes, invoking
    *  methods on #Log to report on the progress of the download.
    *
    *  \param PulseInterval The method pkgAcquireStatus::Pulse will be
    *  invoked on #Log at intervals of PulseInterval microseconds.
    *
    *  \return the result of the download.
    */
   RunResult Run(int PulseInterval=500000);

   /** \brief Remove all items from this download process, terminate
    *  all download workers, and empty all queues.
    */
   void Shutdown();
   
   /** \brief Get the first Worker object.
    *
    *  \return the first active worker in this download process.
    */
   inline Worker *WorkersBegin() {return Workers;};

   /** \brief Advance to the next Worker object.
    *
    *  \return the worker immediately following I, or \b NULL if none
    *  exists.
    */
   Worker *WorkerStep(Worker *I) APT_PURE;

   /** \brief Get the head of the list of items. */
   inline ItemIterator ItemsBegin() {return Items.begin();};
   inline ItemCIterator ItemsBegin() const {return Items.begin();};

   /** \brief Get the end iterator of the list of items. */
   inline ItemIterator ItemsEnd() {return Items.end();};
   inline ItemCIterator ItemsEnd() const {return Items.end();};
   
   // Iterate over queued Item URIs
   class UriIterator;
   /** \brief Get the head of the list of enqueued item URIs.
    *
    *  This iterator will step over every element of every active
    *  queue.
    */
   UriIterator UriBegin();
   /** \brief Get the end iterator of the list of enqueued item URIs. */
   UriIterator UriEnd();
   
   /** Deletes each entry in the given directory that is not being
    *  downloaded by this object.  For instance, when downloading new
    *  list files, calling Clean() will delete the old ones.
    *
    *  \param Dir The directory to be cleaned out.
    *
    *  \return \b true if the directory exists and is readable.
    */
   bool Clean(std::string Dir);
   
   /** Deletes each package list and index file in the given directory.
    *
    *  \param Dir The directory to be cleaned.
    *
    *  \return \b true if the directory exists and is readable.
    */
   bool CleanLists(std::string const &Dir);

   /** \return the total size in bytes of all the items included in
    *  this download.
    */
   unsigned long long TotalNeeded();

   /** \return the size in bytes of all non-local items included in
    *  this download.
    */
   unsigned long long FetchNeeded();

   /** \return the amount of data to be fetched that is already
    *  present on the filesystem.
    */
   unsigned long long PartialPresent();

   void SetLog(pkgAcquireStatus *Progress) { Log = Progress; }

   /** \brief acquire lock and perform directory setup
    *
    *  \param Lock defines a lock file that should be acquired to ensure
    *  only one Acquire class is in action at the time or an empty string
    *  if no lock file should be used. If set also all needed directories
    *  will be created and setup.
    */
   bool GetLock(std::string const &Lock);

   /** \brief Construct a new pkgAcquire. */
   explicit pkgAcquire(pkgAcquireStatus *Log);
   pkgAcquire();

   /** \brief Destroy this pkgAcquire object.
    *
    *  Destroys all queue, method, and item objects associated with
    *  this download.
    */
   virtual ~pkgAcquire();

   APT_HIDDEN static std::string URIEncode(std::string const &part);

   private:
   APT_HIDDEN void Initialize();
};

/** \brief Represents a single download source from which an item
 *  should be downloaded.
 *
 *  An item may have several associated ItemDescs over its lifetime.
 */
struct APT_PUBLIC pkgAcquire::ItemDesc : public WeakPointable
{
   /** \brief URI from which to download this item. */
   std::string URI;
   /** \brief description of this item. */
   std::string Description;
   /** \brief shorter description of this item. */
   std::string ShortDesc;
   /** \brief underlying item which is to be downloaded. */
   Item *Owner;
};
									/*}}}*/
/** \brief A single download queue in a pkgAcquire object.		{{{
 *
 *  \todo Why so many protected values?
 */
class APT_PUBLIC pkgAcquire::Queue
{
   friend class pkgAcquire;
   friend class pkgAcquire::UriIterator;
   friend class pkgAcquire::Worker;

   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   /** \brief The next queue in the pkgAcquire object's list of queues. */
   Queue *Next;
   
   protected:

   /** \brief A single item placed in this queue. */
   struct QItem : public ItemDesc
   {
      /** \brief The next item in the queue. */
      QItem *Next;
      /** \brief The worker associated with this item, if any. */
      pkgAcquire::Worker *Worker;

      /** \brief The underlying items interested in the download */
      std::vector<Item*> Owners;

      /** \brief How many bytes of the file have been downloaded.  Zero
       *  if the current progress of the file cannot be determined.
       */
      unsigned long long CurrentSize = 0;

      /** \brief The total number of bytes to be downloaded.  Zero if the
       *  total size of the final is unknown.
       */
      unsigned long long TotalSize = 0;

      /** \brief How much of the file was already downloaded prior to
       *  starting this worker.
       */
      unsigned long long ResumePoint = 0;

      typedef std::vector<Item*>::const_iterator owner_iterator;

      /** \brief Assign the ItemDesc portion of this QItem from
       *  another ItemDesc
       */
      void operator =(pkgAcquire::ItemDesc const &I)
      {
	 URI = I.URI;
	 Description = I.Description;
	 ShortDesc = I.ShortDesc;
	 Owners.clear();
	 Owners.push_back(I.Owner);
	 Owner = I.Owner;
      };

      /** @return the sum of all expected hashes by all owners */
      HashStringList GetExpectedHashes() const;

      /** @return smallest maximum size of all owners */
      unsigned long long GetMaximumSize() const;

      /** \brief get partial files in order */
      void SyncDestinationFiles() const;

      /** @return the custom headers to use for this item */
      std::string Custom600Headers() const;
      /** @return the maximum priority of this item */
      int APT_HIDDEN GetPriority() const;
      /** @return the maximum time to fetch this item at */
      time_point APT_HIDDEN GetFetchAfter() const;
   };

   /** \brief The name of this queue. */
   std::string Name;

   /** \brief The head of the list of items contained in this queue.
    *
    *  \todo why a by-hand list instead of an STL structure?
    */
   QItem *Items;

   /** \brief The head of the list of workers associated with this queue.
    *
    *  \todo This is plural because support exists in Queue for
    *  multiple workers.  However, it does not appear that there is
    *  any way to actually associate more than one worker with a
    *  queue.
    *
    *  \todo Why not just use a std::set?
    */
   pkgAcquire::Worker *Workers;

   /** \brief the download scheduler with which this queue is associated. */
   pkgAcquire *Owner;

   /** \brief The number of entries in this queue that are currently
    *  being downloaded.
    */
   signed long PipeDepth;

   /** \brief The maximum number of entries that this queue will
    *  attempt to download at once.
    */
   unsigned long MaxPipeDepth;
   
   public:
   
   /** \brief Insert the given fetch request into this queue. 
    *
    *  \return \b true if the queuing was successful. May return
    *  \b false if the Item is already in the queue
    */
   bool Enqueue(ItemDesc &Item);

   /** \brief Remove all fetch requests for the given item from this queue.
    *
    *  \return \b true if at least one request was removed from the queue.
    */
   bool Dequeue(Item *Owner);

   /** \brief Locate an item in this queue.
    *
    *  \param URI A URI to match against.
    *  \param Owner A pkgAcquire::Worker to match against.
    *
    *  \return the first item in the queue whose URI is #URI and that
    *  is being downloaded by #Owner.
    */
   QItem *FindItem(std::string URI,pkgAcquire::Worker *Owner) APT_PURE;

   /** Presumably this should start downloading an item?
    *
    *  \todo Unimplemented.  Implement it or remove?
    */
   bool ItemStart(QItem *Itm,unsigned long long Size);

   /** \brief Remove the given item from this queue and set its state
    *  to pkgAcquire::Item::StatDone.
    *
    *  If this is the only queue containing the item, the item is also
    *  removed from the main queue by calling pkgAcquire::Dequeue.
    *
    *  \param Itm The item to remove.
    *
    *  \return \b true if no errors are encountered.
    */
   bool ItemDone(QItem *Itm);
   
   /** \brief Start the worker process associated with this queue.
    *
    *  If a worker process is already associated with this queue,
    *  this is equivalent to calling Cycle().
    *
    *  \return \b true if the startup was successful.
    */
   bool Startup();

   /** \brief Shut down the worker process associated with this queue.
    *
    *  \param Final If \b true, then the process is stopped unconditionally.
    *               Otherwise, it is only stopped if it does not need cleanup
    *               as indicated by the pkgAcqMethod::NeedsCleanup member of
    *               its configuration.
    *
    *  \return \b true.
    */
   bool Shutdown(bool Final);

   /** \brief Send idle items to the worker process.
    *
    *  Fills up the pipeline by inserting idle items into the worker's queue.
    */
   bool Cycle();

   /** \brief Check for items that could be enqueued.
    *
    *  Call this after an item placed in multiple queues has gone from
    *  the pkgAcquire::Item::StatFetching state to the
    *  pkgAcquire::Item::StatIdle state, to possibly refill an empty queue.
    *  This is an alias for Cycle().
    *
    *  \todo Why both this and Cycle()?  Are they expected to be
    *  different someday?
    */
   void Bump();
   
   /** \brief Create a new Queue.
    *
    *  \param Name The name of the new queue.
    *  \param Owner The download process that owns the new queue.
    */
   Queue(std::string const &Name,pkgAcquire * const Owner);

   /** Shut down all the worker processes associated with this queue
    *  and empty the queue.
    */
   virtual ~Queue();
};
									/*}}}*/
/** \brief Iterates over all the URIs being fetched by a pkgAcquire object.	{{{*/
class APT_PUBLIC pkgAcquire::UriIterator
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   /** The next queue to iterate over. */
   pkgAcquire::Queue *CurQ;
   /** The item that we currently point at. */
   pkgAcquire::Queue::QItem *CurItem;
   
   public:
   
   inline void operator ++() {operator ++(0);};

   void operator ++(int)
   {
      CurItem = CurItem->Next;
      while (CurItem == 0 && CurQ != 0)
      {
	 CurItem = CurQ->Items;
	 CurQ = CurQ->Next;
      }
   };
   
   inline pkgAcquire::Queue::QItem const *operator ->() const {return CurItem;};
   inline bool operator !=(UriIterator const &rhs) const {return rhs.CurQ != CurQ || rhs.CurItem != CurItem;};
   inline bool operator ==(UriIterator const &rhs) const {return rhs.CurQ == CurQ && rhs.CurItem == CurItem;};
   
   /** \brief Create a new UriIterator.
    *
    *  \param Q The queue over which this UriIterator should iterate.
    */
   explicit UriIterator(pkgAcquire::Queue *Q);
   virtual ~UriIterator();
};
									/*}}}*/
/** \brief Information about the properties of a single acquire method.	{{{*/
struct APT_PUBLIC pkgAcquire::MethodConfig
{
   class Private;
   /** \brief dpointer placeholder (for later in case we need it) */
   Private *const d;

   /** \brief The next link on the acquire method list.
    *
    *  \todo Why not an STL container?
    */
   MethodConfig *Next;
   
   /** \brief The name of this acquire method (e.g., http). */
   std::string Access;

   /** \brief The implementation version of this acquire method. */
   std::string Version;

   /** \brief If \b true, only one download queue should be created for this
    *  method.
    */
   bool SingleInstance;

   /** \brief If \b true, this method supports pipelined downloading. */
   bool Pipeline;

   /** \brief If \b true, the worker process should send the entire
    *  APT configuration tree to the fetch subprocess when it starts
    *  up.
    */
   bool SendConfig;

   /** \brief If \b true, this fetch method does not require network access;
    *  all files are to be acquired from the local disk.
    */
   bool LocalOnly;

   /** \brief If \b true, the subprocess has to carry out some cleanup
    *  actions before shutting down.
    *
    *  For instance, the cdrom method needs to unmount the CD after it
    *  finishes.
    */
   bool NeedsCleanup;

   /** \brief If \b true, this fetch method acquires files from removable media. */
   bool Removable;
   
   /** \brief Set up the default method parameters.
    *
    *  All fields are initialized to NULL, "", or \b false as
    *  appropriate.
    */
   MethodConfig();

   APT_HIDDEN bool GetAuxRequests() const;
   APT_HIDDEN void SetAuxRequests(bool const value);
   APT_HIDDEN bool GetSendURIEncoded() const;
   APT_HIDDEN void SetSendURIEncoded(bool const value);

   virtual ~MethodConfig();
};
									/*}}}*/
/** \brief A monitor object for downloads controlled by the pkgAcquire class.	{{{
 *
 *  \todo Why protected members?
 */
class APT_PUBLIC pkgAcquireStatus
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   protected:
   
   /** \brief The last time at which this monitor object was updated. */
   struct timeval Time;

   /** \brief The time at which the download started. */
   struct timeval StartTime;

   /** \brief The number of bytes fetched as of the previous call to
    *  pkgAcquireStatus::Pulse, including local items.
    */
   unsigned long long LastBytes;

   /** \brief The current rate of download as of the most recent call
    *  to pkgAcquireStatus::Pulse, in bytes per second.
    */
   unsigned long long CurrentCPS;

   /** \brief The number of bytes fetched as of the most recent call
    *  to pkgAcquireStatus::Pulse, including local items.
    */
   unsigned long long CurrentBytes;

   /** \brief The total number of bytes that need to be fetched.
    *
    *  \warning This member is inaccurate, as new items might be
    *  enqueued while the download is in progress!
    */
   unsigned long long TotalBytes;

   /** \brief The total number of bytes accounted for by items that
    *  were successfully fetched.
    */
   unsigned long long FetchedBytes;

   /** \brief The amount of time that has elapsed since the download
    *   started.
    */
   unsigned long long ElapsedTime;

   /** \brief The total number of items that need to be fetched.
    *
    *  \warning This member is inaccurate, as new items might be
    *  enqueued while the download is in progress!
    */
   unsigned long TotalItems;

   /** \brief The number of items that have been successfully downloaded. */
   unsigned long CurrentItems;
   
   /** \brief The estimated percentage of the download (0-100)
    */
   double Percent;

   public:

   /** \brief If \b true, the download scheduler should call Pulse()
    *  at the next available opportunity.
    */
   bool Update;

   /** \brief If \b true, extra Pulse() invocations will be performed.
    *
    *  With this option set, Pulse() will be called every time that a
    *  download item starts downloading, finishes downloading, or
    *  terminates with an error.
    */
   bool MorePulses;
      
   /** \brief Invoked when a local or remote file has been completely fetched.
    *
    *  \param Size The size of the file fetched.
    *
    *  \param ResumePoint How much of the file was already fetched.
    */
   virtual void Fetched(unsigned long long Size,unsigned long long ResumePoint);
   
   /** \brief Invoked when the user should be prompted to change the
    *         inserted removable media.
    *
    *  This method should not return until the user has confirmed to
    *  the user interface that the media change is complete.
    *
    *  \param Media The name of the media type that should be changed.
    *
    *  \param Drive The identifying name of the drive whose media
    *               should be changed.
    *
    *  \return \b true if the user confirms the media change, \b
    *  false if it is cancelled.
    *
    *  \todo This is a horrible blocking monster; it should be CPSed
    *  with prejudice.
    */
   virtual bool MediaChange(std::string Media,std::string Drive) = 0;

   struct ReleaseInfoChange
   {
      std::string Type; /*!< Type of the change like "Origin", "Codename", "Version", … */
      std::string From; /*!< old value */
      std::string To; /*!< new value */
      std::string Message; /*!< translated message describing the change */
      bool DefaultAction; /*!< true if the change is informational, false if it must be explicitly confirmed */
   };
   /** \brief ask the user for confirmation of changes to infos about a repository
    *
    *  This method should present the user with a choice of accepting the change
    *  or not and indicate the user opinion via the return value. If DefaultAction is true
    *  it is acceptable to only notify the user about the change, but to accept the change
    *  automatically on behalf of the user.
    *
    *  The default implementation will fail if any Change has DefaultAction == false. Regardless of
    *  success it will print for each change the message attached to it via GlobalError either as an
    *  error (if DefaultAction == false) or as a notice otherwise.
    *
    *  @param LastRelease can be used to extract further information from the previous Release file
    *  @param CurrentRelease can be used to extract further information from the current Release file
    *  @param Changes is an array of changes alongside explanatory messages
    *                 which should be presented in some way to the user.
    *  @return \b true if all changes are accepted by user, otherwise or if user can't be asked \b false
    */
   virtual bool ReleaseInfoChanges(metaIndex const * const LastRelease, metaIndex const * const CurrentRelease, std::vector<ReleaseInfoChange> &&Changes);
   APT_HIDDEN static bool ReleaseInfoChangesAsGlobalErrors(std::vector<ReleaseInfoChange> &&Changes);

   /** \brief Invoked when an item is confirmed to be up-to-date.

    * For instance, when an HTTP download is informed that the file on
    * the server was not modified.
    */
   virtual void IMSHit(pkgAcquire::ItemDesc &/*Itm*/) {};

   /** \brief Invoked when some of an item's data is fetched. */
   virtual void Fetch(pkgAcquire::ItemDesc &/*Itm*/) {};

   /** \brief Invoked when an item is successfully and completely fetched. */
   virtual void Done(pkgAcquire::ItemDesc &/*Itm*/) {};

   /** \brief Invoked when the process of fetching an item encounters
    *  a fatal error.
    */
   virtual void Fail(pkgAcquire::ItemDesc &/*Itm*/) {};

   /** \brief Periodically invoked while the Acquire process is underway.
    *
    *  Subclasses should first call pkgAcquireStatus::Pulse(), then
    *  update their status output.  The download process is blocked
    *  while Pulse() is being called.
    *
    *  \return \b false if the user asked to cancel the whole Acquire process.
    *
    *  \see pkgAcquire::Run
    */
   virtual bool Pulse(pkgAcquire *Owner);

   /** \brief Invoked when the Acquire process starts running. */
   virtual void Start();

   /** \brief Invoked when the Acquire process stops running. */
   virtual void Stop();
   
   /** \brief Initialize all counters to 0 and the time to the current time. */
   pkgAcquireStatus();
   virtual ~pkgAcquireStatus();
};
									/*}}}*/
/** @} */

#endif
