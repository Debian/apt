// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Acquire Worker - Worker process manager

   Each worker class is associated with exactly one subprocess.

   ##################################################################### */
									/*}}}*/

/** \addtogroup acquire
 *  @{
 *
 *  \file acquire-worker.h
 */

#ifndef PKGLIB_ACQUIRE_WORKER_H
#define PKGLIB_ACQUIRE_WORKER_H

#include <apt-pkg/acquire.h>
#include <apt-pkg/weakptr.h>

#include <string>
#include <vector>
#include <sys/types.h>

/** \brief A fetch subprocess.
 *
 *  A worker process is responsible for one stage of the fetch.  This
 *  class encapsulates the communications protocol between the master
 *  process and the worker, from the master end.
 *
 *  Each worker is intrinsically placed on two linked lists.  The
 *  Queue list (maintained in the #NextQueue variable) is maintained
 *  by the pkgAcquire::Queue class; it represents the set of workers
 *  assigned to a particular queue.  The Acquire list (maintained in
 *  the #NextAcquire variable) is maintained by the pkgAcquire class;
 *  it represents the set of active workers for a particular
 *  pkgAcquire object.
 *
 *  \todo Like everything else in the Acquire system, this has way too
 *  many protected items.
 *
 *  \sa pkgAcqMethod, pkgAcquire::Item, pkgAcquire
 */
class APT_PUBLIC pkgAcquire::Worker : public WeakPointable
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;
  
   friend class pkgAcquire;
   
   protected:
   friend class Queue;

   /** \brief The next link on the Queue list.
    *
    *  \todo This is always NULL; is it just for future use?
    */
   Worker *NextQueue;

   /** \brief The next link on the Acquire list. */
   Worker *NextAcquire;
   
   /** \brief The Queue with which this worker is associated. */
   Queue *OwnerQ;

   /** \brief The download progress indicator to which progress
    *  messages should be sent.
    */
   pkgAcquireStatus *Log;

   /** \brief The configuration of this method.  On startup, the
    *  target of this pointer is filled in with basic data about the
    *  method, as reported by the worker.
    */
   MethodConfig *Config;

   /** \brief The access method to be used by this worker.
    *
    *  \todo Doesn't this duplicate Config->Access?
    */
   std::string Access;

   /** \brief The PID of the subprocess. */
   pid_t Process;

   /** \brief A file descriptor connected to the standard output of
    *  the subprocess.
    *
    *  Used to read messages and data from the subprocess.
    */
   int InFd;

   /** \brief A file descriptor connected to the standard input of the
    *  subprocess.
    *
    *  Used to send commands and configuration data to the subprocess.
    */
   int OutFd;

   /** \brief The socket to send SCM_RIGHTS message through
    */
   int PrivSepSocketFd;
   int PrivSepSocketFdChild;

   /** \brief Set to \b true if the worker is in a state in which it
    *  might generate data or command responses.
    *
    *  \todo Is this right?  It's a guess.
    */
   bool InReady;

   /** \brief Set to \b true if the worker is in a state in which it
    *  is legal to send commands to it.
    *
    *  \todo Is this right?
    */
   bool OutReady;
   
   /** If \b true, debugging output will be sent to std::clog. */
   bool Debug;

   /** \brief The raw text values of messages received from the
    *  worker, in sequence.
    */
   std::vector<std::string> MessageQueue;

   /** \brief Buffers pending writes to the subprocess.
    *
    *  \todo Wouldn't a std::dequeue be more appropriate?
    */
   std::string OutQueue;
   
   /** \brief Common code for the constructor.
    *
    *  Initializes NextQueue and NextAcquire to NULL; Process, InFd,
    *  and OutFd to -1, OutReady and InReady to \b false, and Debug
    *  from _config.
    */
   void Construct();
   
   /** \brief Retrieve any available messages from the subprocess.
    *
    *  The messages are retrieved as in \link strutl.h ReadMessages()\endlink, and
    *  #MethodFailure() is invoked if an error occurs; in particular,
    *  if the pipe to the subprocess dies unexpectedly while a message
    *  is being read.
    *
    *  \return \b true if the messages were successfully read, \b
    *  false otherwise.
    */
   bool ReadMessages();

   /** \brief Parse and dispatch pending messages.
    *
    *  This dispatches the message in a manner appropriate for its
    *  type.
    *
    *  \todo Several message types lack separate handlers.
    *
    *  \sa Capabilities(), SendConfiguration(), MediaChange()
    */
   bool RunMessages();

   /** \brief Read and dispatch any pending messages from the
    *  subprocess.
    *
    *  \return \b false if the subprocess died unexpectedly while a
    *  message was being transmitted.
    */
   bool InFdReady();

   /** \brief Send any pending commands to the subprocess.
    *
    *  This method will fail if there is no pending output.
    *
    *  \return \b true if all commands were succeeded, \b false if an
    *  error occurred (in which case MethodFailure() will be invoked).
    */
   bool OutFdReady();
   
   /** \brief Handle a 100 Capabilities response from the subprocess.
    *
    *  \param Message the raw text of the message from the subprocess.
    *
    *  The message will be parsed and its contents used to fill
    *  #Config.  If #Config is NULL, this routine is a NOP.
    *
    *  \return \b true.
    */
   bool Capabilities(std::string Message);

   /** \brief Send a 601 Configuration message (containing the APT
    *  configuration) to the subprocess.
    *
    *  The APT configuration will be send to the subprocess in a
    *  message of the following form:
    *
    *  <pre>
    *  601 Configuration
    *  Config-Item: Fully-Qualified-Item=Val
    *  Config-Item: Fully-Qualified-Item=Val
    *  ...
    *  </pre>
    *
    *  \return \b true if the command was successfully sent, \b false
    *  otherwise.
    */
   bool SendConfiguration();

   /** \brief Handle a 403 Media Change message.
    *
    *  \param Message the raw text of the message; the Media field
    *  indicates what type of media should be changed, and the Drive
    *  field indicates where the media is located.
    *
    *  Invokes pkgAcquireStatus::MediaChange(Media, Drive) to ask the
    *  user to swap disks; informs the subprocess of the result (via
    *  603 Media Changed, with the Failed field set to \b true if the
    *  user cancelled the media change).
    */
   bool MediaChange(std::string Message);
   
   /** \brief Invoked when the worked process dies unexpectedly.
    *
    *  Waits for the subprocess to terminate and generates an error if
    *  it terminated abnormally, then closes and blanks out all file
    *  descriptors.  Discards all pending messages from the
    *  subprocess.
    *
    *  \return \b false.
    */
   bool MethodFailure();

   /** \brief Invoked when a fetch job is completed, either
    *  successfully or unsuccessfully.
    *
    *  Resets the status information for the worker process.
    */
   void ItemDone();
   
   public:
   
   /** \brief The queue entry that is currently being downloaded. */
   pkgAcquire::Queue::QItem *CurrentItem;

   /** \brief The most recent status string received from the
    *  subprocess.
    */
   std::string Status;

   /** \brief Tell the subprocess to download the given item.
    *
    *  \param Item the item to queue up.
    *  \return \b true if the item was successfully enqueued.
    *
    *  Queues up a 600 URI Acquire message for the given item to be
    *  sent at the next possible moment.  Does \e not flush the output
    *  queue.
    */
   bool QueueItem(pkgAcquire::Queue::QItem *Item);
   APT_HIDDEN bool ReplyAux(pkgAcquire::ItemDesc const &Item);

   /** \brief Start up the worker and fill in #Config.
    *
    *  Reads the first message from the worker, which is assumed to be
    *  a 100 Capabilities message.
    *
    *  \return \b true if all operations completed successfully.
    */
   bool Start();

   /** \brief Update the worker statistics (CurrentSize, TotalSize,
    *  etc).
    */
   void Pulse();

   /** \return The fetch method configuration. */
   inline const MethodConfig *GetConf() const {return Config;};

   /** \brief Create a new Worker to download files.
    *
    *  \param OwnerQ The queue into which this worker should be
    *  placed.
    *
    *  \param Config A location in which to store information about
    *  the fetch method.
    *
    *  \param Log The download progress indicator that should be used
    *  to report the progress of this worker.
    */
   Worker(Queue *OwnerQ,MethodConfig *Config,pkgAcquireStatus *Log);

   /** \brief Create a new Worker that should just retrieve
    *  information about the fetch method.
    *
    *  Nothing in particular forces you to refrain from actually
    *  downloading stuff, but the various status callbacks won't be
    *  invoked.
    *
    *  \param Config A location in which to store information about
    *  the fetch method.
    */
   explicit Worker(MethodConfig *Config);

   /** \brief Clean up this worker.
    *
    *  Closes the file descriptors; if MethodConfig::NeedsCleanup is
    *  \b false, also rudely interrupts the worker with a SIGINT.
    */
   virtual ~Worker();

private:
   APT_HIDDEN void PrepareFiles(char const * const caller, pkgAcquire::Queue::QItem const * const Itm);
   APT_HIDDEN void HandleFailure(std::vector<pkgAcquire::Item *> const &ItmOwners,
				 pkgAcquire::MethodConfig *const Config, pkgAcquireStatus *const Log,
				 std::string const &Message, bool const errTransient, bool const errAuthErr);
};

/** @} */

#endif
