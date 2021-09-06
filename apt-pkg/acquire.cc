// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Acquire - File Acquiration

   The core element for the schedule system is the concept of a named
   queue. Each queue is unique and each queue has a name derived from the
   URI. The degree of paralization can be controlled by how the queue
   name is derived from the URI.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// helper to convert time_point to a timeval
static struct timeval SteadyDurationToTimeVal(std::chrono::steady_clock::duration Time)
{
   auto const Time_sec = std::chrono::duration_cast<std::chrono::seconds>(Time);
   auto const Time_usec = std::chrono::duration_cast<std::chrono::microseconds>(Time - Time_sec);
   return {Time_sec.count(), Time_usec.count()};
}

std::string pkgAcquire::URIEncode(std::string const &part)		/*{{{*/
{
   // The "+" is encoded as a workaround for an S3 bug (LP#1003633 and LP#1086997)
   return QuoteString(part, _config->Find("Acquire::URIEncode", "+~ ").c_str());
}
									/*}}}*/
// Acquire::pkgAcquire - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* We grab some runtime state from the configuration space */
pkgAcquire::pkgAcquire() : LockFD(-1), d(NULL), Queues(0), Workers(0), Configs(0), Log(NULL), ToFetch(0),
			   Debug(_config->FindB("Debug::pkgAcquire",false)),
			   Running(false)
{
   Initialize();
}
pkgAcquire::pkgAcquire(pkgAcquireStatus *Progress) : LockFD(-1), d(NULL), Queues(0), Workers(0),
			   Configs(0), Log(NULL), ToFetch(0),
			   Debug(_config->FindB("Debug::pkgAcquire",false)),
			   Running(false)
{
   Initialize();
   SetLog(Progress);
}
void pkgAcquire::Initialize()
{
   string const Mode = _config->Find("Acquire::Queue-Mode","host");
   if (strcasecmp(Mode.c_str(),"host") == 0)
      QueueMode = QueueHost;
   if (strcasecmp(Mode.c_str(),"access") == 0)
      QueueMode = QueueAccess;
}
									/*}}}*/
// Acquire::GetLock - lock directory and prepare for action		/*{{{*/
static bool SetupAPTPartialDirectory(std::string const &grand, std::string const &parent, std::string const &postfix, mode_t const mode)
{
   if (_config->FindB("Debug::SetupAPTPartialDirectory::AssumeGood", false))
      return true;
   std::string const partial = parent + postfix;
   bool const partialExists = DirectoryExists(partial);
   if (partialExists == false)
   {
      mode_t const old_umask = umask(S_IWGRP | S_IWOTH);
      bool const creation_fail = (CreateAPTDirectoryIfNeeded(grand, partial) == false &&
	    CreateAPTDirectoryIfNeeded(parent, partial) == false);
      umask(old_umask);
      if (creation_fail == true)
	 return false;
   }

   std::string const SandboxUser = _config->Find("APT::Sandbox::User");
   if (getuid() == 0)
   {
      if (SandboxUser.empty() == false && SandboxUser != "root") // if we aren't root, we can't chown, so don't try it
      {
	 struct passwd const * const pw = getpwnam(SandboxUser.c_str());
	 struct group const * const gr = getgrnam(ROOT_GROUP);
	 if (pw != NULL && gr != NULL)
	 {
	    // chown the partial dir
	    if(chown(partial.c_str(), pw->pw_uid, gr->gr_gid) != 0)
	       _error->WarningE("SetupAPTPartialDirectory", "chown to %s:%s of directory %s failed", SandboxUser.c_str(), ROOT_GROUP, partial.c_str());
	 }
      }
      if (chmod(partial.c_str(), mode) != 0)
	 _error->WarningE("SetupAPTPartialDirectory", "chmod 0%03o of directory %s failed", mode, partial.c_str());

   }
   else if (chmod(partial.c_str(), mode) != 0)
   {
      // if we haven't created the dir and aren't root, it is kinda expected that chmod doesn't work
      if (partialExists == false)
	 _error->WarningE("SetupAPTPartialDirectory", "chmod 0%03o of directory %s failed", mode, partial.c_str());
   }

   _error->PushToStack();
   // remove 'old' FAILED files to stop us from collecting them for no reason
   for (auto const &Failed: GetListOfFilesInDir(partial, "FAILED", false, false))
      RemoveFile("SetupAPTPartialDirectory", Failed);
   _error->RevertToStack();

   return true;
}
bool pkgAcquire::GetLock(std::string const &Lock)
{
   if (Lock.empty() == true)
      return false;

   // check for existence and possibly create auxiliary directories
   string const listDir = _config->FindDir("Dir::State::lists");
   string const archivesDir = _config->FindDir("Dir::Cache::Archives");

   if (Lock == listDir)
   {
      if (SetupAPTPartialDirectory(_config->FindDir("Dir::State"), listDir, "partial", 0700) == false)
	 return _error->Errno("Acquire", _("List directory %s is missing."), (listDir + "partial").c_str());
   }
   if (Lock == archivesDir)
   {
      if (SetupAPTPartialDirectory(_config->FindDir("Dir::Cache"), archivesDir, "partial", 0700) == false)
	 return _error->Errno("Acquire", _("Archives directory %s is missing."), (archivesDir + "partial").c_str());
   }
   if (Lock == listDir || Lock == archivesDir)
   {
      if (SetupAPTPartialDirectory(_config->FindDir("Dir::State"), listDir, "auxfiles", 0755) == false)
      {
	 // not being able to create lists/auxfiles isn't critical as we will use a tmpdir then
      }
   }

   if (_config->FindB("Debug::NoLocking", false) == true)
      return true;

   // Lock the directory this acquire object will work in
   if (LockFD != -1)
      close(LockFD);
   LockFD = ::GetLock(flCombine(Lock, "lock"));
   if (LockFD == -1)
      return _error->Error(_("Unable to lock directory %s"), Lock.c_str());

   return true;
}
									/*}}}*/
// Acquire::~pkgAcquire	- Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* Free our memory, clean up the queues (destroy the workers) */
pkgAcquire::~pkgAcquire()
{
   Shutdown();

   if (LockFD != -1)
      close(LockFD);

   while (Configs != 0)
   {
      MethodConfig *Jnk = Configs;
      Configs = Configs->Next;
      delete Jnk;
   }   
}
									/*}}}*/
// Acquire::Shutdown - Clean out the acquire object			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Shutdown()
{
   while (Items.empty() == false)
   {
      if (Items[0]->Status == Item::StatFetching)
         Items[0]->Status = Item::StatError;
      delete Items[0];
   }

   while (Queues != 0)
   {
      Queue *Jnk = Queues;
      Queues = Queues->Next;
      delete Jnk;
   }   
}
									/*}}}*/
// Acquire::Add - Add a new item					/*{{{*/
// ---------------------------------------------------------------------
/* This puts an item on the acquire list. This list is mainly for tracking
   item status */
void pkgAcquire::Add(Item *Itm)
{
   Items.push_back(Itm);
}
									/*}}}*/
// Acquire::Remove - Remove a item					/*{{{*/
// ---------------------------------------------------------------------
/* Remove an item from the acquire list. This is usually not used.. */
void pkgAcquire::Remove(Item *Itm)
{
   Dequeue(Itm);
   
   for (ItemIterator I = Items.begin(); I != Items.end();)
   {
      if (*I == Itm)
      {
	 Items.erase(I);
	 I = Items.begin();
      }      
      else 
	 ++I;
   }
}
									/*}}}*/
// Acquire::Add - Add a worker						/*{{{*/
// ---------------------------------------------------------------------
/* A list of workers is kept so that the select loop can direct their FD
   usage. */
void pkgAcquire::Add(Worker *Work)
{
   Work->NextAcquire = Workers;
   Workers = Work;
}
									/*}}}*/
// Acquire::Remove - Remove a worker					/*{{{*/
// ---------------------------------------------------------------------
/* A worker has died. This can not be done while the select loop is running
   as it would require that RunFds could handling a changing list state and
   it can't.. */
void pkgAcquire::Remove(Worker *Work)
{
   if (Running == true)
      abort();
   
   Worker **I = &Workers;
   for (; *I != 0;)
   {
      if (*I == Work)
	 *I = (*I)->NextAcquire;
      else
	 I = &(*I)->NextAcquire;
   }
}
									/*}}}*/
// Acquire::Enqueue - Queue an URI for fetching				/*{{{*/
// ---------------------------------------------------------------------
/* This is the entry point for an item. An item calls this function when
   it is constructed which creates a queue (based on the current queue
   mode) and puts the item in that queue. If the system is running then
   the queue might be started. */
static bool CheckForBadItemAndFailIt(pkgAcquire::Item * const Item,
      pkgAcquire::MethodConfig const * const Config, pkgAcquireStatus * const Log)
{
   auto SavedDesc = Item->GetItemDesc();
   if (Item->IsRedirectionLoop(SavedDesc.URI))
   {
      std::string const Message = "400 URI Failure"
	 "\nURI: " + SavedDesc.URI +
	 "\nFilename: " + Item->DestFile +
	 "\nFailReason: RedirectionLoop";

      Item->Status = pkgAcquire::Item::StatError;
      Item->Failed(Message, Config);
      if (Log != nullptr)
	 Log->Fail(SavedDesc);
      return true;
   }

   HashStringList const hsl = Item->GetExpectedHashes();
   if (hsl.usable() == false && Item->HashesRequired() &&
	 _config->Exists("Acquire::ForceHash") == false)
   {
      std::string const Message = "400 URI Failure"
	 "\nURI: " + SavedDesc.URI +
	 "\nFilename: " + Item->DestFile +
	 "\nFailReason: WeakHashSums";

      Item->Status = pkgAcquire::Item::StatAuthError;
      Item->Failed(Message, Config);
      if (Log != nullptr)
	 Log->Fail(SavedDesc);
      return true;
   }
   return false;
}
void pkgAcquire::Enqueue(ItemDesc &Item)
{
   // Determine which queue to put the item in
   const MethodConfig *Config = nullptr;
   string Name = QueueName(Item.URI,Config);
   if (Name.empty() == true)
   {
      Item.Owner->Status = pkgAcquire::Item::StatError;
      return;
   }

   /* the check for running avoids that we produce errors
      in logging before we actually have started, which would
      be easier to implement but would confuse users/implementations
      so we check the items skipped here in #Startup */
   if (Running && CheckForBadItemAndFailIt(Item.Owner, Config, Log))
      return;

   // Find the queue structure
   Queue *I = Queues;
   for (; I != 0 && I->Name != Name; I = I->Next);
   if (I == 0)
   {
      I = new Queue(Name,this);
      I->Next = Queues;
      Queues = I;
      
      if (Running == true)
	 I->Startup();
   }

   // See if this is a local only URI
   if (Config->LocalOnly == true && Item.Owner->Complete == false)
      Item.Owner->Local = true;
   Item.Owner->Status = Item::StatIdle;
   
   // Queue it into the named queue
   if(I->Enqueue(Item)) 
      ToFetch++;
            
   // Some trace stuff
   if (Debug == true)
   {
      clog << "Fetching " << Item.URI << endl;
      clog << " to " << Item.Owner->DestFile << endl;
      clog << " Queue is: " << Name << endl;
   }
}
									/*}}}*/
// Acquire::Dequeue - Remove an item from all queues			/*{{{*/
// ---------------------------------------------------------------------
/* This is called when an item is finished being fetched. It removes it
   from all the queues */
void pkgAcquire::Dequeue(Item *Itm)
{
   Queue *I = Queues;
   bool Res = false;
   if (Debug == true)
      clog << "Dequeuing " << Itm->DestFile << endl;

   for (; I != 0; I = I->Next)
   {
      if (I->Dequeue(Itm))
      {
         Res = true;
	 if (Debug == true)
	    clog << "Dequeued from " << I->Name << endl;
      }
   }

   if (Res == true)
      ToFetch--;
}
									/*}}}*/
// Acquire::QueueName - Return the name of the queue for this URI	/*{{{*/
// ---------------------------------------------------------------------
/* The string returned depends on the configuration settings and the
   method parameters. Given something like http://foo.org/bar it can
   return http://foo.org or http */
string pkgAcquire::QueueName(string Uri,MethodConfig const *&Config)
{
   constexpr int DEFAULT_HOST_LIMIT = 10;
   URI U(Uri);

   // Note that this gets written through the reference to the caller.
   Config = GetConfig(U.Access);
   if (Config == nullptr)
      return {};

   // Access mode forces all methods to be Single-Instance
   if (QueueMode == QueueAccess)
      return U.Access;

   // Single-Instance methods get exactly one queue per URI
   if (Config->SingleInstance == true)
      return U.Access;

   // Host-less methods like rred, store, …
   if (U.Host.empty())
   {
      int existing = 0;
      // check how many queues exist already and reuse empty ones
      auto const AccessSchema = U.Access + ':';
      for (Queue const *I = Queues; I != 0; I = I->Next)
	 if (APT::String::Startswith(I->Name, AccessSchema))
	 {
	    if (I->Items == nullptr)
	       return I->Name;
	    ++existing;
	 }

      int const Limit = _config->FindI("Acquire::QueueHost::Limit",
#ifdef _SC_NPROCESSORS_ONLN
	    sysconf(_SC_NPROCESSORS_ONLN) * 2
#else
	    DEFAULT_HOST_LIMIT
#endif
      );

      // create a new worker if we don't have too many yet
      if (Limit <= 0 || existing < Limit)
	 return AccessSchema + std::to_string(existing);

      // find the worker with the least to do
      // we already established that there are no empty and we can't spawn new
      Queue const *selected = nullptr;
      auto selected_backlog = std::numeric_limits<decltype(HashStringList().FileSize())>::max();
      for (Queue const *Q = Queues; Q != nullptr; Q = Q->Next)
	 if (APT::String::Startswith(Q->Name, AccessSchema))
	 {
	    decltype(selected_backlog) current_backlog = 0;
	    for (auto const *I = Q->Items; I != nullptr; I = I->Next)
	    {
	       auto const hashes = I->Owner->GetExpectedHashes();
	       if (not hashes.empty())
		  current_backlog += hashes.FileSize();
	       else
		  current_backlog += I->Owner->FileSize;
	    }
	    if (current_backlog < selected_backlog)
	    {
	       selected = Q;
	       selected_backlog = current_backlog;
	    }
	 }

      if (unlikely(selected == nullptr))
	 return AccessSchema + "0";
      return selected->Name;
   }
   // most methods talking to remotes like http
   else
   {
      auto const FullQueueName = U.Access + ':' + U.Host;
      // if the queue already exists, re-use it
      for (Queue const *Q = Queues; Q != nullptr; Q = Q->Next)
	 if (Q->Name == FullQueueName)
	    return FullQueueName;

      int existing = 0;
      // check how many queues exist already and reuse empty ones
      auto const AccessSchema = U.Access + ':';
      for (Queue const *Q = Queues; Q != nullptr; Q = Q->Next)
	 if (APT::String::Startswith(Q->Name, AccessSchema))
	    ++existing;

      int const Limit = _config->FindI("Acquire::QueueHost::Limit", DEFAULT_HOST_LIMIT);
      // if we have too many hosts open use a single generic for the rest
      if (existing >= Limit)
	 return U.Access;

      // we can still create new named queues
      return FullQueueName;
   }
}
									/*}}}*/
// Acquire::GetConfig - Fetch the configuration information		/*{{{*/
// ---------------------------------------------------------------------
/* This locates the configuration structure for an access method. If 
   a config structure cannot be found a Worker will be created to
   retrieve it */
pkgAcquire::MethodConfig *pkgAcquire::GetConfig(string Access)
{
   // Search for an existing config
   MethodConfig *Conf;
   for (Conf = Configs; Conf != 0; Conf = Conf->Next)
      if (Conf->Access == Access)
	 return Conf;
   
   // Create the new config class
   Conf = new MethodConfig;
   Conf->Access = Access;

   // Create the worker to fetch the configuration
   Worker Work(Conf);
   if (Work.Start() == false)
   {
      delete Conf;
      return nullptr;
   }
   Conf->Next = Configs;
   Configs = Conf;

   /* if a method uses DownloadLimit, we switch to SingleInstance mode */
   if(_config->FindI("Acquire::"+Access+"::Dl-Limit",0) > 0)
      Conf->SingleInstance = true;
    
   return Conf;
}
									/*}}}*/
// Acquire::SetFds - Deal with readable FDs				/*{{{*/
// ---------------------------------------------------------------------
/* Collect FDs that have activity monitors into the fd sets */
void pkgAcquire::SetFds(int &Fd,fd_set *RSet,fd_set *WSet)
{
   for (Worker *I = Workers; I != 0; I = I->NextAcquire)
   {
      if (I->InReady == true && I->InFd >= 0)
      {
	 if (Fd < I->InFd)
	    Fd = I->InFd;
	 FD_SET(I->InFd,RSet);
      }
      if (I->OutReady == true && I->OutFd >= 0)
      {
	 if (Fd < I->OutFd)
	    Fd = I->OutFd;
	 FD_SET(I->OutFd,WSet);
      }
   }
}
									/*}}}*/
// Acquire::RunFds - Deal with active FDs				/*{{{*/
// ---------------------------------------------------------------------
/* Dispatch active FDs over to the proper workers. It is very important
   that a worker never be erased while this is running! The queue class
   should never erase a worker except during shutdown processing. */
bool pkgAcquire::RunFds(fd_set *RSet,fd_set *WSet)
{
   bool Res = true;

   for (Worker *I = Workers; I != 0; I = I->NextAcquire)
   {
      if (I->InFd >= 0 && FD_ISSET(I->InFd,RSet) != 0)
	 Res &= I->InFdReady();
      if (I->OutFd >= 0 && FD_ISSET(I->OutFd,WSet) != 0)
	 Res &= I->OutFdReady();
   }

   return Res;
}
									/*}}}*/
// Acquire::Run - Run the fetch sequence				/*{{{*/
// ---------------------------------------------------------------------
/* This runs the queues. It manages a select loop for all of the
   Worker tasks. The workers interact with the queues and items to
   manage the actual fetch. */
static bool IsAccessibleBySandboxUser(std::string const &filename, bool const ReadWrite)
{
   // you would think this is easily to answer with faccessat, right? Wrong!
   // It e.g. gets groups wrong, so the only thing which works reliable is trying
   // to open the file we want to open later on…
   if (unlikely(filename.empty()))
      return true;

   if (ReadWrite == false)
   {
      errno = 0;
      // can we read a file? Note that non-existing files are "fine"
      int const fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);
      if (fd == -1 && errno == EACCES)
	 return false;
      close(fd);
      return true;
   }
   else
   {
      // the file might not exist yet and even if it does we will fix permissions,
      // so important is here just that the directory it is in allows that
      std::string const dirname = flNotFile(filename);
      if (unlikely(dirname.empty()))
	 return true;

      char const * const filetag = ".apt-acquire-privs-test.XXXXXX";
      std::string const tmpfile_tpl = flCombine(dirname, filetag);
      std::unique_ptr<char, decltype(std::free) *> tmpfile { strdup(tmpfile_tpl.c_str()), std::free };
      int const fd = mkstemp(tmpfile.get());
      if (fd == -1 && errno == EACCES)
	 return false;
      RemoveFile("IsAccessibleBySandboxUser", tmpfile.get());
      close(fd);
      return true;
   }
}
static void CheckDropPrivsMustBeDisabled(pkgAcquire const &Fetcher)
{
   if(getuid() != 0)
      return;

   std::string const SandboxUser = _config->Find("APT::Sandbox::User");
   if (SandboxUser.empty() || SandboxUser == "root")
      return;

   struct passwd const * const pw = getpwnam(SandboxUser.c_str());
   if (pw == NULL)
   {
      _error->Warning(_("No sandbox user '%s' on the system, can not drop privileges"), SandboxUser.c_str());
      _config->Set("APT::Sandbox::User", "");
      return;
   }

   gid_t const old_euid = geteuid();
   gid_t const old_egid = getegid();

   long const ngroups_max = sysconf(_SC_NGROUPS_MAX);
   std::unique_ptr<gid_t[]> old_gidlist(new gid_t[ngroups_max]);
   if (unlikely(old_gidlist == NULL))
      return;
   ssize_t old_gidlist_nr;
   if ((old_gidlist_nr = getgroups(ngroups_max, old_gidlist.get())) < 0)
   {
      _error->FatalE("getgroups", "getgroups %lu failed", ngroups_max);
      old_gidlist[0] = 0;
      old_gidlist_nr = 1;
   }
   if (setgroups(1, &pw->pw_gid))
      _error->FatalE("setgroups", "setgroups %u failed", pw->pw_gid);

   if (setegid(pw->pw_gid) != 0)
      _error->FatalE("setegid", "setegid %u failed", pw->pw_gid);
   if (seteuid(pw->pw_uid) != 0)
      _error->FatalE("seteuid", "seteuid %u failed", pw->pw_uid);

   for (pkgAcquire::ItemCIterator I = Fetcher.ItemsBegin();
	I != Fetcher.ItemsEnd(); ++I)
   {
      // no need to drop privileges for a complete file
      if ((*I)->Complete == true || (*I)->Status != pkgAcquire::Item::StatIdle)
	 continue;

      // if destination file is inaccessible all hope is lost for privilege dropping
      if (IsAccessibleBySandboxUser((*I)->DestFile, true) == false)
      {
	 _error->WarningE("pkgAcquire::Run", _("Download is performed unsandboxed as root as file '%s' couldn't be accessed by user '%s'."),
	       (*I)->DestFile.c_str(), SandboxUser.c_str());
	 _config->Set("APT::Sandbox::User", "");
	 break;
      }

      // if its the source file (e.g. local sources) we might be lucky
      // by dropping the dropping only for some methods.
      URI const source((*I)->DescURI());
      if (source.Access == "file" || source.Access == "copy")
      {
	 std::string const conf = "Binary::" + source.Access + "::APT::Sandbox::User";
	 if (_config->Exists(conf) == true)
	    continue;

	 auto const filepath = DeQuoteString(source.Path);
	 if (not IsAccessibleBySandboxUser(filepath, false))
	 {
	    _error->NoticeE("pkgAcquire::Run", _("Download is performed unsandboxed as root as file '%s' couldn't be accessed by user '%s'."),
			    filepath.c_str(), SandboxUser.c_str());
	    _config->CndSet("Binary::file::APT::Sandbox::User", "root");
	    _config->CndSet("Binary::copy::APT::Sandbox::User", "root");
	 }
      }
   }

   if (seteuid(old_euid) != 0)
      _error->FatalE("seteuid", "seteuid %u failed", old_euid);
   if (setegid(old_egid) != 0)
      _error->FatalE("setegid", "setegid %u failed", old_egid);
   if (setgroups(old_gidlist_nr, old_gidlist.get()))
      _error->FatalE("setgroups", "setgroups %u failed", 0);
}
pkgAcquire::RunResult pkgAcquire::Run(int PulseInterval)
{
   _error->PushToStack();
   CheckDropPrivsMustBeDisabled(*this);

   Running = true;

   if (Log != 0)
      Log->Start();

   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Startup();
   
   bool WasCancelled = false;

   // Run till all things have been acquired
   struct timeval tv = SteadyDurationToTimeVal(std::chrono::microseconds(PulseInterval));
   while (ToFetch > 0)
   {
      fd_set RFds;
      fd_set WFds;
      int Highest = 0;
      FD_ZERO(&RFds);
      FD_ZERO(&WFds);
      SetFds(Highest,&RFds,&WFds);

      // Shorten the select() cycle in case we have items about to become ready
      auto now = clock::now();
      auto fetchAfter = time_point{};
      for (Queue *I = Queues; I != nullptr; I = I->Next)
      {
	 if (I->Items == nullptr)
	    continue;

	 auto f = I->Items->GetFetchAfter();

	 if (f == time_point() || I->Items->Owner->Status != pkgAcquire::Item::StatIdle)
	    continue;

	 if (f <= now)
	 {
	    if (not I->Cycle()) // Queue got stuck, unstuck it.
	       goto stop;
	    fetchAfter = now; // need to time out in select() below
	    if (I->Items->Owner->Status == pkgAcquire::Item::StatIdle)
	    {
	       _error->Warning("Tried to start delayed item %s, but failed", I->Items->Description.c_str());
	    }
	 }
	 else if (f < fetchAfter || fetchAfter == time_point{})
	 {
	    fetchAfter = f;
	 }
      }

      if (fetchAfter != time_point{} && (fetchAfter - now) < std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec))
      {
	 tv = SteadyDurationToTimeVal(fetchAfter - now);
      }

      int Res;
      do
      {
	 Res = select(Highest+1,&RFds,&WFds,0,&tv);
      }
      while (Res < 0 && errno == EINTR);
      
      if (Res < 0)
      {
	 _error->Errno("select","Select has failed");
	 break;
      }

      if(RunFds(&RFds,&WFds) == false)
         break;

      // Timeout, notify the log class
      if (Res == 0 || (Log != 0 && Log->Update == true))
      {
	 tv = SteadyDurationToTimeVal(std::chrono::microseconds(PulseInterval));

	 for (Worker *I = Workers; I != 0; I = I->NextAcquire)
	    I->Pulse();
	 if (Log != 0 && Log->Pulse(this) == false)
	 {
	    WasCancelled = true;
	    break;
	 }
      }      
   }
stop:
   if (Log != 0)
      Log->Stop();
   
   // Shut down the acquire bits
   Running = false;
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Shutdown(false);

   // Shut down the items
   for (ItemIterator I = Items.begin(); I != Items.end(); ++I)
      (*I)->Finished();

   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (newError)
      return Failed;
   if (WasCancelled)
      return Cancelled;
   return Continue;
}
									/*}}}*/
// Acquire::Bump - Called when an item is dequeued			/*{{{*/
// ---------------------------------------------------------------------
/* This routine bumps idle queues in hopes that they will be able to fetch
   the dequeued item */
void pkgAcquire::Bump()
{
   for (Queue *I = Queues; I != 0; I = I->Next)
      I->Bump();
}
									/*}}}*/
// Acquire::WorkerStep - Step to the next worker			/*{{{*/
// ---------------------------------------------------------------------
/* Not inlined to advoid including acquire-worker.h */
pkgAcquire::Worker *pkgAcquire::WorkerStep(Worker *I)
{
   return I->NextAcquire;
}
									/*}}}*/
// Acquire::Clean - Cleans a directory					/*{{{*/
// ---------------------------------------------------------------------
/* This is a bit simplistic, it looks at every file in the dir and sees
   if it is part of the download set. */
bool pkgAcquire::Clean(string Dir)
{
   // non-existing directories are by definition clean…
   if (DirectoryExists(Dir) == false)
      return true;

   if(Dir == "/")
      return _error->Error(_("Clean of %s is not supported"), Dir.c_str());

   int const dirfd = open(Dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
   if (dirfd == -1)
      return _error->Errno("open",_("Unable to read %s"),Dir.c_str());
   DIR * const D = fdopendir(dirfd);
   if (D == nullptr)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());

   for (struct dirent *E = readdir(D); E != nullptr; E = readdir(D))
   {
      // Skip some entries
      if (strcmp(E->d_name, "lock") == 0 ||
	  strcmp(E->d_name, "partial") == 0 ||
	  strcmp(E->d_name, "auxfiles") == 0 ||
	  strcmp(E->d_name, "lost+found") == 0 ||
	  strcmp(E->d_name, ".") == 0 ||
	  strcmp(E->d_name, "..") == 0)
	 continue;

      // Look in the get list and if not found nuke
      if (std::any_of(Items.cbegin(), Items.cend(),
	     [&E](pkgAcquire::Item const * const I) {
		return flNotDir(I->DestFile) == E->d_name;
	     }) == false)
      {
	 RemoveFileAt("pkgAcquire::Clean", dirfd, E->d_name);
      }
   }
   closedir(D);
   return true;
}
									/*}}}*/
// Acquire::TotalNeeded - Number of bytes to fetch			/*{{{*/
// ---------------------------------------------------------------------
/* This is the total number of bytes needed */
APT_PURE unsigned long long pkgAcquire::TotalNeeded()
{
   return std::accumulate(ItemsBegin(), ItemsEnd(), 0llu,
      [](unsigned long long const T, Item const * const I) {
	 return T + I->FileSize;
   });
}
									/*}}}*/
// Acquire::FetchNeeded - Number of bytes needed to get			/*{{{*/
// ---------------------------------------------------------------------
/* This is the number of bytes that is not local */
APT_PURE unsigned long long pkgAcquire::FetchNeeded()
{
   return std::accumulate(ItemsBegin(), ItemsEnd(), 0llu,
      [](unsigned long long const T, Item const * const I) {
	 if (I->Local == false)
	    return T + I->FileSize;
	 else
	    return T;
   });
}
									/*}}}*/
// Acquire::PartialPresent - Number of partial bytes we already have	/*{{{*/
// ---------------------------------------------------------------------
/* This is the number of bytes that is not local */
APT_PURE unsigned long long pkgAcquire::PartialPresent()
{
   return std::accumulate(ItemsBegin(), ItemsEnd(), 0llu,
      [](unsigned long long const T, Item const * const I) {
	 if (I->Local == false)
	    return T + I->PartialSize;
	 else
	    return T;
   });
}
									/*}}}*/
// Acquire::UriBegin - Start iterator for the uri list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::UriIterator pkgAcquire::UriBegin()
{
   return UriIterator(Queues);
}
									/*}}}*/
// Acquire::UriEnd - End iterator for the uri list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::UriIterator pkgAcquire::UriEnd()
{
   return UriIterator(0);
}
									/*}}}*/
// Acquire::MethodConfig::MethodConfig - Constructor			/*{{{*/
class pkgAcquire::MethodConfig::Private
{
   public:
   bool AuxRequests = false;
   bool SendURIEncoded = false;
};
pkgAcquire::MethodConfig::MethodConfig() : d(new Private()), Next(0), SingleInstance(false),
					   Pipeline(false), SendConfig(false), LocalOnly(false), NeedsCleanup(false),
					   Removable(false)
{
}
									/*}}}*/
bool pkgAcquire::MethodConfig::GetAuxRequests() const /*{{{*/
{
   return d->AuxRequests;
}
									/*}}}*/
void pkgAcquire::MethodConfig::SetAuxRequests(bool const value) /*{{{*/
{
   d->AuxRequests = value;
}
									/*}}}*/
bool pkgAcquire::MethodConfig::GetSendURIEncoded() const		/*{{{*/
{
   return d->SendURIEncoded;
}
									/*}}}*/
void pkgAcquire::MethodConfig::SetSendURIEncoded(bool const value)	/*{{{*/
{
   d->SendURIEncoded = value;
}
									/*}}}*/

// Queue::Queue - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::Queue(string const &name,pkgAcquire * const owner) : d(NULL), Next(0),
   Name(name), Items(0), Workers(0), Owner(owner), PipeDepth(0), MaxPipeDepth(1)
{
}
									/*}}}*/
// Queue::~Queue - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::~Queue()
{
   Shutdown(true);
   
   while (Items != 0)
   {
      QItem *Jnk = Items;
      Items = Items->Next;
      delete Jnk;
   }
}
									/*}}}*/
// Queue::Enqueue - Queue an item to the queue				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgAcquire::Queue::Enqueue(ItemDesc &Item)
{
   // MetaKeysMatch checks whether the two items have no non-matching
   // meta-keys. If the items are not transaction items, it returns
   // true, so other items can still be merged.
   auto MetaKeysMatch = [](pkgAcquire::ItemDesc const &A, pkgAcquire::Queue::QItem const *B) {
      auto OwnerA = dynamic_cast<pkgAcqTransactionItem*>(A.Owner);
      if (OwnerA == nullptr)
	 return true;

      for (auto const & OwnerBUncast : B->Owners) {
	 auto OwnerB = dynamic_cast<pkgAcqTransactionItem*>(OwnerBUncast);

	 if (OwnerB != nullptr && OwnerA->GetMetaKey() != OwnerB->GetMetaKey())
	    return false;
      }
      return true;
   };
   QItem **OptimalI = &Items;
   QItem **I = &Items;
   auto insertLocation = std::make_tuple(Item.Owner->FetchAfter(), -Item.Owner->Priority());
   // move to the end of the queue and check for duplicates here
   for (; *I != 0; ) {
      if (Item.URI == (*I)->URI && MetaKeysMatch(Item, *I))
      {
	 if (_config->FindB("Debug::pkgAcquire::Worker",false) == true)
	    std::cerr << " @ Queue: Action combined for " << Item.URI << " and " << (*I)->URI << std::endl;
	 (*I)->Owners.push_back(Item.Owner);
	 Item.Owner->Status = (*I)->Owner->Status;
	 return false;
      }
      // Determine the optimal position to insert: before anything with a
      // higher priority.
      auto queueLocation = std::make_tuple((*I)->GetFetchAfter(),
					   -(*I)->GetPriority());

      I = &(*I)->Next;
      if (queueLocation <= insertLocation)
      {
	 OptimalI = I;
      }
   }


   // Create a new item
   QItem *Itm = new QItem;
   *Itm = Item;
   Itm->Next = *OptimalI;
   *OptimalI = Itm;
   
   Item.Owner->QueueCounter++;   
   if (Items->Next == 0)
      Cycle();
   return true;
}
									/*}}}*/
// Queue::Dequeue - Remove an item from the queue			/*{{{*/
// ---------------------------------------------------------------------
/* We return true if we hit something */
bool pkgAcquire::Queue::Dequeue(Item *Owner)
{
   if (Owner->Status == pkgAcquire::Item::StatFetching)
      return _error->Error("Tried to dequeue a fetching object");

   bool Res = false;

   QItem **I = &Items;
   for (; *I != 0;)
   {
      if (Owner == (*I)->Owner)
      {
	 QItem *Jnk= *I;
	 *I = (*I)->Next;
	 Owner->QueueCounter--;
	 delete Jnk;
	 Res = true;
      }
      else
	 I = &(*I)->Next;
   }

   return Res;
}
									/*}}}*/
// Queue::Startup - Start the worker processes				/*{{{*/
// ---------------------------------------------------------------------
/* It is possible for this to be called with a pre-existing set of
   workers. */
bool pkgAcquire::Queue::Startup()
{
   if (Workers == 0)
   {
      URI U(Name);
      pkgAcquire::MethodConfig * const Cnf = Owner->GetConfig(U.Access);
      if (unlikely(Cnf == nullptr))
	 return false;

      // now-running twin of the pkgAcquire::Enqueue call
      for (QItem *I = Items; I != 0; )
      {
	 auto const INext = I->Next;
	 for (auto &&O: I->Owners)
	    CheckForBadItemAndFailIt(O, Cnf, Owner->Log);
	 // if an item failed, it will be auto-dequeued invalidation our I here
	 I = INext;
      }

      Workers = new Worker(this,Cnf,Owner->Log);
      Owner->Add(Workers);
      if (Workers->Start() == false)
	 return false;
      
      /* When pipelining we commit 10 items. This needs to change when we
         added other source retry to have cycle maintain a pipeline depth
         on its own. */
      if (Cnf->Pipeline == true)
	 MaxPipeDepth = _config->FindI("Acquire::Max-Pipeline-Depth",10);
      else
	 MaxPipeDepth = 1;
   }
   
   return Cycle();
}
									/*}}}*/
// Queue::Shutdown - Shutdown the worker processes			/*{{{*/
// ---------------------------------------------------------------------
/* If final is true then all workers are eliminated, otherwise only workers
   that do not need cleanup are removed */
bool pkgAcquire::Queue::Shutdown(bool Final)
{
   // Delete all of the workers
   pkgAcquire::Worker **Cur = &Workers;
   while (*Cur != 0)
   {
      pkgAcquire::Worker *Jnk = *Cur;
      if (Final == true || Jnk->GetConf()->NeedsCleanup == false)
      {
	 *Cur = Jnk->NextQueue;
	 Owner->Remove(Jnk);
	 delete Jnk;
      }
      else
	 Cur = &(*Cur)->NextQueue;      
   }
   
   return true;
}
									/*}}}*/
// Queue::FindItem - Find a URI in the item list			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquire::Queue::QItem *pkgAcquire::Queue::FindItem(string URI,pkgAcquire::Worker *Owner)
{
   if (Owner->Config->GetSendURIEncoded())
   {
      for (QItem *I = Items; I != nullptr; I = I->Next)
	 if (I->URI == URI && I->Worker == Owner)
	    return I;
   }
   else
   {
      for (QItem *I = Items; I != nullptr; I = I->Next)
      {
	 if (I->Worker != Owner)
	    continue;
	 ::URI tmpuri{I->URI};
	 tmpuri.Path = DeQuoteString(tmpuri.Path);
	 if (URI == std::string(tmpuri))
	    return I;
      }
   }
   return nullptr;
}
									/*}}}*/
// Queue::ItemDone - Item has been completed				/*{{{*/
// ---------------------------------------------------------------------
/* The worker signals this which causes the item to be removed from the
   queue. If this is the last queue instance then it is removed from the
   main queue too.*/
bool pkgAcquire::Queue::ItemDone(QItem *Itm)
{
   PipeDepth--;
   for (QItem::owner_iterator O = Itm->Owners.begin(); O != Itm->Owners.end(); ++O)
   {
      if ((*O)->Status == pkgAcquire::Item::StatFetching)
	 (*O)->Status = pkgAcquire::Item::StatDone;
   }

   if (Itm->Owner->QueueCounter <= 1)
      Owner->Dequeue(Itm->Owner);
   else
   {
      Dequeue(Itm->Owner);
      Owner->Bump();
   }

   return Cycle();
}
									/*}}}*/
// Queue::Cycle - Queue new items into the method			/*{{{*/
// ---------------------------------------------------------------------
/* This locates a new idle item and sends it to the worker. If pipelining
   is enabled then it keeps the pipe full. */
bool pkgAcquire::Queue::Cycle()
{
   if (Items == 0 || Workers == 0)
      return true;

   if (PipeDepth < 0)
      return _error->Error("Pipedepth failure");

   // Look for a queable item
   QItem *I = Items;
   int ActivePriority = 0;
   auto currentTime = clock::now();
   while (PipeDepth < static_cast<decltype(PipeDepth)>(MaxPipeDepth))
   {
      for (; I != 0; I = I->Next) {
	 if (I->Owner->Status == pkgAcquire::Item::StatFetching)
	    ActivePriority = std::max(ActivePriority, I->GetPriority());
	 if (I->Owner->Status == pkgAcquire::Item::StatIdle)
	    break;
      }

      // Nothing to do, queue is idle.
      if (I == 0)
	 return true;

      // This item has a lower priority than stuff in the pipeline, pretend
      // the queue is idle
      if (I->GetPriority() < ActivePriority)
	 return true;

      // Item is not ready yet, delay
      if (I->GetFetchAfter() > currentTime)
	 return true;

      I->Worker = Workers;
      for (auto const &O: I->Owners)
	 O->Status = pkgAcquire::Item::StatFetching;
      PipeDepth++;
      if (Workers->QueueItem(I) == false)
	 return false;
   }

   return true;
}
									/*}}}*/
// Queue::Bump - Fetch any pending objects if we are idle		/*{{{*/
// ---------------------------------------------------------------------
/* This is called when an item in multiple queues is dequeued */
void pkgAcquire::Queue::Bump()
{
   Cycle();
}
									/*}}}*/
HashStringList pkgAcquire::Queue::QItem::GetExpectedHashes() const	/*{{{*/
{
   /* each Item can have multiple owners and each owner might have different
      hashes, even if that is unlikely in practice and if so at least some
      owners will later fail. There is one situation through which is not a
      failure and still needs this handling: Two owners who expect the same
      file, but one owner only knows the SHA1 while the other only knows SHA256. */
   HashStringList superhsl;
   for (pkgAcquire::Queue::QItem::owner_iterator O = Owners.begin(); O != Owners.end(); ++O)
   {
      HashStringList const hsl = (*O)->GetExpectedHashes();
      // we merge both lists - if we find disagreement send no hashes
      HashStringList::const_iterator hs = hsl.begin();
      for (; hs != hsl.end(); ++hs)
	 if (superhsl.push_back(*hs) == false)
	    break;
      if (hs != hsl.end())
      {
	 superhsl.clear();
	 break;
      }
   }
   return superhsl;
}
									/*}}}*/
APT_PURE unsigned long long pkgAcquire::Queue::QItem::GetMaximumSize() const	/*{{{*/
{
   unsigned long long Maximum = std::numeric_limits<unsigned long long>::max();
   for (auto const &O: Owners)
   {
      if (O->FileSize == 0)
	 continue;
      Maximum = std::min(Maximum, O->FileSize);
   }
   if (Maximum == std::numeric_limits<unsigned long long>::max())
      return 0;
   return Maximum;
}
									/*}}}*/
APT_PURE int pkgAcquire::Queue::QItem::GetPriority() const		/*{{{*/
{
   int Priority = 0;
   for (auto const &O: Owners)
      Priority = std::max(Priority, O->Priority());

   return Priority;
}
									/*}}}*/
APT_PURE pkgAcquire::time_point pkgAcquire::Queue::QItem::GetFetchAfter() const /*{{{*/
{
   time_point FetchAfter{};
   for (auto const &O : Owners)
      FetchAfter = std::max(FetchAfter, O->FetchAfter());

   return FetchAfter;
}
									/*}}}*/
void pkgAcquire::Queue::QItem::SyncDestinationFiles() const		/*{{{*/
{
   /* ensure that the first owner has the best partial file of all and
      the rest have (potentially dangling) symlinks to it so that
      everything (like progress reporting) finds it easily */
   std::string superfile = Owner->DestFile;
   off_t supersize = 0;
   for (pkgAcquire::Queue::QItem::owner_iterator O = Owners.begin(); O != Owners.end(); ++O)
   {
      if ((*O)->DestFile == superfile)
	 continue;
      struct stat file;
      if (lstat((*O)->DestFile.c_str(),&file) == 0)
      {
	 if ((file.st_mode & S_IFREG) == 0)
	    RemoveFile("SyncDestinationFiles", (*O)->DestFile);
	 else if (supersize < file.st_size)
	 {
	    supersize = file.st_size;
	    RemoveFile("SyncDestinationFiles", superfile);
	    rename((*O)->DestFile.c_str(), superfile.c_str());
	 }
	 else
	    RemoveFile("SyncDestinationFiles", (*O)->DestFile);
	 if (symlink(superfile.c_str(), (*O)->DestFile.c_str()) != 0)
	 {
	    ; // not a problem per-se and no real alternative
	 }
      }
   }
}
									/*}}}*/
std::string pkgAcquire::Queue::QItem::Custom600Headers() const		/*{{{*/
{
   /* The others are relatively easy to merge, but this one?
      Lets not merge and see how far we can run with it…
      Likely, nobody will ever notice as all the items will
      be of the same class and hence generate the same headers. */
   return Owner->Custom600Headers();
}
									/*}}}*/

// AcquireStatus::pkgAcquireStatus - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcquireStatus::pkgAcquireStatus() : d(NULL), Percent(-1), Update(true), MorePulses(false)
{
   Start();
}
									/*}}}*/
// AcquireStatus::Pulse - Called periodically				/*{{{*/
// ---------------------------------------------------------------------
/* This computes some internal state variables for the derived classes to
   use. It generates the current downloaded bytes and total bytes to download
   as well as the current CPS estimate. */
static struct timeval GetTimevalFromSteadyClock()
{
   return SteadyDurationToTimeVal(std::chrono::steady_clock::now().time_since_epoch());
}
bool pkgAcquireStatus::Pulse(pkgAcquire *Owner)
{
   TotalBytes = 0;
   CurrentBytes = 0;
   TotalItems = 0;
   CurrentItems = 0;

   // Compute the total number of bytes to fetch
   unsigned int Unknown = 0;
   unsigned int Count = 0;
   bool ExpectAdditionalItems = false;
   for (pkgAcquire::ItemCIterator I = Owner->ItemsBegin(); 
        I != Owner->ItemsEnd();
	++I, ++Count)
   {
      TotalItems++;
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 ++CurrentItems;

      // do we expect to acquire more files than we know of yet?
      if ((*I)->ExpectedAdditionalItems > 0)
         ExpectAdditionalItems = true;

      TotalBytes += (*I)->FileSize;
      if ((*I)->Complete == true)
	 CurrentBytes += (*I)->FileSize;
      if ((*I)->FileSize == 0 && (*I)->Complete == false)
	 ++Unknown;
   }

   // Compute the current completion
   unsigned long long ResumeSize = 0;
   for (pkgAcquire::Worker *I = Owner->WorkersBegin(); I != 0;
	I = Owner->WorkerStep(I))
   {
      if (I->CurrentItem != 0 && I->CurrentItem->Owner->Complete == false)
      {
	 CurrentBytes += I->CurrentItem->CurrentSize;
	 ResumeSize += I->CurrentItem->ResumePoint;

	 // Files with unknown size always have 100% completion
	 if (I->CurrentItem->Owner->FileSize == 0 &&
	     I->CurrentItem->Owner->Complete == false)
	    TotalBytes += I->CurrentItem->CurrentSize;
      }
   }
   
   // Normalize the figures and account for unknown size downloads
   if (TotalBytes <= 0)
      TotalBytes = 1;
   if (Unknown == Count)
      TotalBytes = Unknown;

   // Wha?! Is not supposed to happen.
   if (CurrentBytes > TotalBytes)
      CurrentBytes = TotalBytes;

   // Compute the CPS
   struct timeval NewTime = GetTimevalFromSteadyClock();

   if ((NewTime.tv_sec - Time.tv_sec == 6 && NewTime.tv_usec > Time.tv_usec) ||
       NewTime.tv_sec - Time.tv_sec > 6)
   {
      std::chrono::duration<double> Delta =
	 std::chrono::seconds(NewTime.tv_sec - Time.tv_sec) +
	 std::chrono::microseconds(NewTime.tv_usec - Time.tv_usec);

      // Compute the CPS value
      if (Delta < std::chrono::milliseconds(10))
	 CurrentCPS = 0;
      else
	 CurrentCPS = ((CurrentBytes - ResumeSize) - LastBytes)/ Delta.count();
      LastBytes = CurrentBytes - ResumeSize;
      ElapsedTime = llround(Delta.count());
      Time = NewTime;
   }

   double const OldPercent = Percent;
   // calculate the percentage, if we have too little data assume 1%
   if (ExpectAdditionalItems)
      Percent = 0;
   else
      // use both files and bytes because bytes can be unreliable
      Percent = (0.8 * (CurrentBytes/double(TotalBytes)*100.0) +
                 0.2 * (CurrentItems/double(TotalItems)*100.0));

   // debug
   if (_config->FindB("Debug::acquire::progress", false) == true)
   {
      std::clog
         << "["
         << std::setw(5) << std::setprecision(4) << std::showpoint << Percent 
         << "]"
         << " Bytes: "
         << SizeToStr(CurrentBytes) << " / " << SizeToStr(TotalBytes)
         << " # Files: "
         << CurrentItems << " / " << TotalItems
         << std::endl;
   }

   double const DiffPercent = Percent - OldPercent;
   if (DiffPercent < 0.001 && _config->FindB("Acquire::Progress::Diffpercent", false) == true)
      return true;

   int fd = _config->FindI("APT::Status-Fd",-1);
   if(fd > 0)
   {
      unsigned long long ETA = 0;
      if(CurrentCPS > 0 && TotalBytes > CurrentBytes)
         ETA = (TotalBytes - CurrentBytes) / CurrentCPS;

      std::string msg;
      long i = CurrentItems < TotalItems ? CurrentItems + 1 : CurrentItems;
      // only show the ETA if it makes sense
      if (ETA > 0 && ETA < std::chrono::seconds(std::chrono::hours(24 * 2)).count())
	 strprintf(msg, _("Retrieving file %li of %li (%s remaining)"), i, TotalItems, TimeToStr(ETA).c_str());
      else
	 strprintf(msg, _("Retrieving file %li of %li"), i, TotalItems);

      // build the status str
      std::ostringstream str;
      str.imbue(std::locale::classic());
      str.precision(4);
      str << "dlstatus" << ':' << std::fixed << i << ':' << Percent << ':' << msg << '\n';
      auto const dlstatus = str.str();
      FileFd::Write(fd, dlstatus.data(), dlstatus.size());
   }

   return true;
}
									/*}}}*/
// AcquireStatus::Start - Called when the download is started		/*{{{*/
// ---------------------------------------------------------------------
/* We just reset the counters */
void pkgAcquireStatus::Start()
{
   Time = StartTime = GetTimevalFromSteadyClock();
   LastBytes = 0;
   CurrentCPS = 0;
   CurrentBytes = 0;
   TotalBytes = 0;
   FetchedBytes = 0;
   ElapsedTime = 0;
   TotalItems = 0;
   CurrentItems = 0;
}
									/*}}}*/
// AcquireStatus::Stop - Finished downloading				/*{{{*/
// ---------------------------------------------------------------------
/* This accurately computes the elapsed time and the total overall CPS. */
void pkgAcquireStatus::Stop()
{
   // Compute the CPS and elapsed time
   struct timeval NewTime = GetTimevalFromSteadyClock();

   std::chrono::duration<double> Delta =
      std::chrono::seconds(NewTime.tv_sec - StartTime.tv_sec) +
      std::chrono::microseconds(NewTime.tv_usec - StartTime.tv_usec);

   // Compute the CPS value
   if (Delta < std::chrono::milliseconds(10))
      CurrentCPS = 0;
   else
      CurrentCPS = FetchedBytes / Delta.count();
   LastBytes = CurrentBytes;
   ElapsedTime = llround(Delta.count());
}
									/*}}}*/
// AcquireStatus::Fetched - Called when a byte set has been fetched	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to get accurate final transfer rate reporting. */
void pkgAcquireStatus::Fetched(unsigned long long Size,unsigned long long Resume)
{
   FetchedBytes += Size - Resume;
}
									/*}}}*/
bool pkgAcquireStatus::ReleaseInfoChanges(metaIndex const * const LastRelease, metaIndex const * const CurrentRelease, std::vector<ReleaseInfoChange> &&Changes)/*{{{*/
{
   (void) LastRelease;
   (void) CurrentRelease;
   return ReleaseInfoChangesAsGlobalErrors(std::move(Changes));
}
									/*}}}*/
bool pkgAcquireStatus::ReleaseInfoChangesAsGlobalErrors(std::vector<ReleaseInfoChange> &&Changes)/*{{{*/
{
   bool AllOkay = true;
   for (auto const &c: Changes)
      if (c.DefaultAction)
	 _error->Notice("%s", c.Message.c_str());
      else
      {
	 _error->Error("%s", c.Message.c_str());
	 AllOkay = false;
      }
   return AllOkay;
}
									/*}}}*/


pkgAcquire::UriIterator::UriIterator(pkgAcquire::Queue *Q) : d(NULL), CurQ(Q), CurItem(0)
{
   while (CurItem == 0 && CurQ != 0)
   {
      CurItem = CurQ->Items;
      CurQ = CurQ->Next;
   }
}

pkgAcquire::UriIterator::~UriIterator() {}
pkgAcquire::MethodConfig::~MethodConfig() { delete d; }
pkgAcquireStatus::~pkgAcquireStatus() {}
