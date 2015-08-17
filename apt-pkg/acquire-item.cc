// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.cc,v 1.46.2.9 2004/01/16 18:51:11 mdz Exp $
/* ######################################################################

   Acquire Item - Item to acquire

   Each item can download to exactly one file at a time. This means you
   cannot create an item that fetches two uri's to two files at the same 
   time. The pkgAcqIndex class creates a second class upon instantiation
   to fetch the other index files because of this.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/gpgv.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <stdio.h>
#include <ctime>

#include <apti18n.h>
									/*}}}*/

using namespace std;

static void printHashSumComparision(std::string const &URI, HashStringList const &Expected, HashStringList const &Actual) /*{{{*/
{
   if (_config->FindB("Debug::Acquire::HashSumMismatch", false) == false)
      return;
   std::cerr << std::endl << URI << ":" << std::endl << " Expected Hash: " << std::endl;
   for (HashStringList::const_iterator hs = Expected.begin(); hs != Expected.end(); ++hs)
      std::cerr <<  "\t- " << hs->toStr() << std::endl;
   std::cerr << " Actual Hash: " << std::endl;
   for (HashStringList::const_iterator hs = Actual.begin(); hs != Actual.end(); ++hs)
      std::cerr <<  "\t- " << hs->toStr() << std::endl;
}
									/*}}}*/
static std::string GetPartialFileName(std::string const &file)		/*{{{*/
{
   std::string DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += file;
   return DestFile;
}
									/*}}}*/
static std::string GetPartialFileNameFromURI(std::string const &uri)	/*{{{*/
{
   return GetPartialFileName(URItoFileName(uri));
}
									/*}}}*/
static std::string GetFinalFileNameFromURI(std::string const &uri)	/*{{{*/
{
   return _config->FindDir("Dir::State::lists") + URItoFileName(uri);
}
									/*}}}*/
static std::string GetCompressedFileName(IndexTarget const &Target, std::string const &Name, std::string const &Ext) /*{{{*/
{
   if (Ext.empty() || Ext == "uncompressed")
      return Name;

   // do not reverify cdrom sources as apt-cdrom may rewrite the Packages
   // file when its doing the indexcopy
   if (Target.URI.substr(0,6) == "cdrom:")
      return Name;

   // adjust DestFile if its compressed on disk
   if (Target.KeepCompressed == true)
      return Name + '.' + Ext;
   return Name;
}
									/*}}}*/
static std::string GetMergeDiffsPatchFileName(std::string const &Final, std::string const &Patch)/*{{{*/
{
   // rred expects the patch as $FinalFile.ed.$patchname.gz
   return Final + ".ed." + Patch + ".gz";
}
									/*}}}*/
static std::string GetDiffsPatchFileName(std::string const &Final)	/*{{{*/
{
   // rred expects the patch as $FinalFile.ed
   return Final + ".ed";
}
									/*}}}*/

static bool AllowInsecureRepositories(metaIndex const * const MetaIndexParser, pkgAcqMetaClearSig * const TransactionManager, pkgAcquire::Item * const I) /*{{{*/
{
   if(MetaIndexParser->GetTrusted() == metaIndex::TRI_YES || _config->FindB("Acquire::AllowInsecureRepositories") == true)
      return true;

   _error->Error(_("Use --allow-insecure-repositories to force the update"));
   TransactionManager->AbortTransaction();
   I->Status = pkgAcquire::Item::StatError;
   return false;
}
									/*}}}*/
static HashStringList GetExpectedHashesFromFor(metaIndex * const Parser, std::string const &MetaKey)/*{{{*/
{
   if (Parser == NULL)
      return HashStringList();
   metaIndex::checkSum * const R = Parser->Lookup(MetaKey);
   if (R == NULL)
      return HashStringList();
   return R->Hashes;
}
									/*}}}*/

// all ::HashesRequired and ::GetExpectedHashes implementations		/*{{{*/
/* ::GetExpectedHashes is abstract and has to be implemented by all subclasses.
   It is best to implement it as broadly as possible, while ::HashesRequired defaults
   to true and should be as restrictive as possible for false cases. Note that if
   a hash is returned by ::GetExpectedHashes it must match. Only if it doesn't
   ::HashesRequired is called to evaluate if its okay to have no hashes. */
APT_CONST bool pkgAcqTransactionItem::HashesRequired() const
{
   /* signed repositories obviously have a parser and good hashes.
      unsigned repositories, too, as even if we can't trust them for security,
      we can at least trust them for integrity of the download itself.
      Only repositories without a Release file can (obviously) not have
      hashes – and they are very uncommon and strongly discouraged */
   return TransactionManager->MetaIndexParser != NULL &&
      TransactionManager->MetaIndexParser->GetLoadedSuccessfully() != metaIndex::TRI_UNSET;
}
HashStringList pkgAcqTransactionItem::GetExpectedHashes() const
{
   return GetExpectedHashesFor(GetMetaKey());
}

APT_CONST bool pkgAcqMetaBase::HashesRequired() const
{
   // Release and co have no hashes 'by design'.
   return false;
}
HashStringList pkgAcqMetaBase::GetExpectedHashes() const
{
   return HashStringList();
}

APT_CONST bool pkgAcqIndexDiffs::HashesRequired() const
{
   /* We don't always have the diff of the downloaded pdiff file.
      What we have for sure is hashes for the uncompressed file,
      but rred uncompresses them on the fly while parsing, so not handled here.
      Hashes are (also) checked while searching for (next) patch to apply. */
   if (State == StateFetchDiff)
      return available_patches[0].download_hashes.empty() == false;
   return false;
}
HashStringList pkgAcqIndexDiffs::GetExpectedHashes() const
{
   if (State == StateFetchDiff)
      return available_patches[0].download_hashes;
   return HashStringList();
}

APT_CONST bool pkgAcqIndexMergeDiffs::HashesRequired() const
{
   /* @see #pkgAcqIndexDiffs::HashesRequired, with the difference that
      we can check the rred result after all patches are applied as
      we know the expected result rather than potentially apply more patches */
   if (State == StateFetchDiff)
      return patch.download_hashes.empty() == false;
   return State == StateApplyDiff;
}
HashStringList pkgAcqIndexMergeDiffs::GetExpectedHashes() const
{
   if (State == StateFetchDiff)
      return patch.download_hashes;
   else if (State == StateApplyDiff)
      return GetExpectedHashesFor(Target.MetaKey);
   return HashStringList();
}

APT_CONST bool pkgAcqArchive::HashesRequired() const
{
   return LocalSource == false;
}
HashStringList pkgAcqArchive::GetExpectedHashes() const
{
   // figured out while parsing the records
   return ExpectedHashes;
}

APT_CONST bool pkgAcqFile::HashesRequired() const
{
   // supplied as parameter at creation time, so the caller decides
   return ExpectedHashes.usable();
}
HashStringList pkgAcqFile::GetExpectedHashes() const
{
   return ExpectedHashes;
}
									/*}}}*/
// Acquire::Item::QueueURI and specialisations from child classes	/*{{{*/
bool pkgAcquire::Item::QueueURI(pkgAcquire::ItemDesc &Item)
{
   Owner->Enqueue(Item);
   return true;
}
/* The idea here is that an item isn't queued if it exists on disk and the
   transition manager was a hit as this means that the files it contains
   the checksums for can't be updated either (or they are and we are asking
   for a hashsum mismatch to happen which helps nobody) */
bool pkgAcqTransactionItem::QueueURI(pkgAcquire::ItemDesc &Item)
{
   std::string const FinalFile = GetFinalFilename();
   if (TransactionManager != NULL && TransactionManager->IMSHit == true &&
	 FileExists(FinalFile) == true)
   {
      PartialFile = DestFile = FinalFile;
      Status = StatDone;
      return false;
   }
   return pkgAcquire::Item::QueueURI(Item);
}
/* The transition manager InRelease itself (or its older sisters-in-law
   Release & Release.gpg) is always queued as this allows us to rerun gpgv
   on it to verify that we aren't stalled with old files */
bool pkgAcqMetaBase::QueueURI(pkgAcquire::ItemDesc &Item)
{
   return pkgAcquire::Item::QueueURI(Item);
}
/* the Diff/Index needs to queue also the up-to-date complete index file
   to ensure that the list cleaner isn't eating it */
bool pkgAcqDiffIndex::QueueURI(pkgAcquire::ItemDesc &Item)
{
   if (pkgAcqTransactionItem::QueueURI(Item) == true)
      return true;
   QueueOnIMSHit();
   return false;
}
									/*}}}*/
// Acquire::Item::GetFinalFilename and specialisations for child classes	/*{{{*/
std::string pkgAcquire::Item::GetFinalFilename() const
{
   return GetFinalFileNameFromURI(Desc.URI);
}
std::string pkgAcqDiffIndex::GetFinalFilename() const
{
   // the logic we inherent from pkgAcqBaseIndex isn't what we need here
   return pkgAcquire::Item::GetFinalFilename();
}
std::string pkgAcqIndex::GetFinalFilename() const
{
   std::string const FinalFile = GetFinalFileNameFromURI(Target.URI);
   return GetCompressedFileName(Target, FinalFile, CurrentCompressionExtension);
}
std::string pkgAcqMetaSig::GetFinalFilename() const
{
   return GetFinalFileNameFromURI(Target.URI);
}
std::string pkgAcqBaseIndex::GetFinalFilename() const
{
   return GetFinalFileNameFromURI(Target.URI);
}
std::string pkgAcqMetaBase::GetFinalFilename() const
{
   return GetFinalFileNameFromURI(Target.URI);
}
std::string pkgAcqArchive::GetFinalFilename() const
{
   return _config->FindDir("Dir::Cache::Archives") + flNotDir(StoreFilename);
}
									/*}}}*/
// pkgAcqTransactionItem::GetMetaKey and specialisations for child classes	/*{{{*/
std::string pkgAcqTransactionItem::GetMetaKey() const
{
   return Target.MetaKey;
}
std::string pkgAcqIndex::GetMetaKey() const
{
   if (Stage == STAGE_DECOMPRESS_AND_VERIFY || CurrentCompressionExtension == "uncompressed")
      return Target.MetaKey;
   return Target.MetaKey + "." + CurrentCompressionExtension;
}
std::string pkgAcqDiffIndex::GetMetaKey() const
{
   return Target.MetaKey + ".diff/Index";
}
									/*}}}*/
//pkgAcqTransactionItem::TransactionState and specialisations for child classes	/*{{{*/
bool pkgAcqTransactionItem::TransactionState(TransactionStates const state)
{
   bool const Debug = _config->FindB("Debug::Acquire::Transaction", false);
   switch(state)
   {
      case TransactionAbort:
	 if(Debug == true)
	    std::clog << "  Cancel: " << DestFile << std::endl;
	 if (Status == pkgAcquire::Item::StatIdle)
	 {
	    Status = pkgAcquire::Item::StatDone;
	    Dequeue();
	 }
	 break;
      case TransactionCommit:
	 if(PartialFile != "")
	 {
	    if(Debug == true)
	       std::clog << "mv " << PartialFile << " -> "<< DestFile << " # " << DescURI() << std::endl;

	    Rename(PartialFile, DestFile);
	 } else {
	    if(Debug == true)
	       std::clog << "rm " << DestFile << " # " << DescURI() << std::endl;
	    unlink(DestFile.c_str());
	 }
	 break;
   }
   return true;
}
bool pkgAcqMetaBase::TransactionState(TransactionStates const state)
{
   // Do not remove InRelease on IMSHit of Release.gpg [yes, this is very edgecasey]
   if (TransactionManager->IMSHit == false)
      return pkgAcqTransactionItem::TransactionState(state);
   return true;
}
bool pkgAcqIndex::TransactionState(TransactionStates const state)
{
   if (pkgAcqTransactionItem::TransactionState(state) == false)
      return false;

   switch (state)
   {
      case TransactionAbort:
	 if (Stage == STAGE_DECOMPRESS_AND_VERIFY)
	 {
	    // keep the compressed file, but drop the decompressed
	    EraseFileName.clear();
	    if (PartialFile.empty() == false && flExtension(PartialFile) == "decomp")
	       unlink(PartialFile.c_str());
	 }
	 break;
      case TransactionCommit:
	 if (EraseFileName.empty() == false)
	    unlink(EraseFileName.c_str());
	 break;
   }
   return true;
}
bool pkgAcqDiffIndex::TransactionState(TransactionStates const state)
{
   if (pkgAcqTransactionItem::TransactionState(state) == false)
      return false;

   switch (state)
   {
      case TransactionCommit:
	 break;
      case TransactionAbort:
	 std::string const Partial = GetPartialFileNameFromURI(Target.URI);
	 unlink(Partial.c_str());
	 break;
   }

   return true;
}
									/*}}}*/

class APT_HIDDEN NoActionItem : public pkgAcquire::Item			/*{{{*/
/* The sole purpose of this class is having an item which does nothing to
   reach its done state to prevent cleanup deleting the mentioned file.
   Handy in cases in which we know we have the file already, like IMS-Hits. */
{
   IndexTarget const Target;
   public:
   virtual std::string DescURI() const APT_OVERRIDE {return Target.URI;};
   virtual HashStringList GetExpectedHashes()  const APT_OVERRIDE {return HashStringList();};

   NoActionItem(pkgAcquire * const Owner, IndexTarget const &Target) :
      pkgAcquire::Item(Owner), Target(Target)
   {
      Status = StatDone;
      DestFile = GetFinalFileNameFromURI(Target.URI);
   }
};
									/*}}}*/

// Acquire::Item::Item - Constructor					/*{{{*/
APT_IGNORE_DEPRECATED_PUSH
pkgAcquire::Item::Item(pkgAcquire * const owner) :
   FileSize(0), PartialSize(0), Mode(0), ID(0), Complete(false), Local(false),
    QueueCounter(0), ExpectedAdditionalItems(0), Owner(owner), d(NULL)
{
   Owner->Add(this);
   Status = StatIdle;
}
APT_IGNORE_DEPRECATED_POP
									/*}}}*/
// Acquire::Item::~Item - Destructor					/*{{{*/
pkgAcquire::Item::~Item()
{
   Owner->Remove(this);
}
									/*}}}*/
std::string pkgAcquire::Item::Custom600Headers() const			/*{{{*/
{
   return std::string();
}
									/*}}}*/
std::string pkgAcquire::Item::ShortDesc() const				/*{{{*/
{
   return DescURI();
}
									/*}}}*/
APT_CONST void pkgAcquire::Item::Finished()				/*{{{*/
{
}
									/*}}}*/
APT_PURE pkgAcquire * pkgAcquire::Item::GetOwner() const		/*{{{*/
{
   return Owner;
}
									/*}}}*/
APT_CONST pkgAcquire::ItemDesc &pkgAcquire::Item::GetItemDesc()		/*{{{*/
{
   return Desc;
}
									/*}}}*/
APT_CONST bool pkgAcquire::Item::IsTrusted() const			/*{{{*/
{
   return false;
}
									/*}}}*/
// Acquire::Item::Failed - Item failed to download			/*{{{*/
// ---------------------------------------------------------------------
/* We return to an idle state if there are still other queues that could
   fetch this object */
void pkgAcquire::Item::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)
{
   if(ErrorText.empty())
      ErrorText = LookupTag(Message,"Message");
   if (QueueCounter <= 1)
   {
      /* This indicates that the file is not available right now but might
         be sometime later. If we do a retry cycle then this should be
	 retried [CDROMs] */
      if (Cnf != NULL && Cnf->LocalOnly == true &&
	  StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
      {
	 Status = StatIdle;
	 Dequeue();
	 return;
      }

      switch (Status)
      {
	 case StatIdle:
	 case StatFetching:
	 case StatDone:
	    Status = StatError;
	    break;
	 case StatAuthError:
	 case StatError:
	 case StatTransientNetworkError:
	    break;
      }
      Complete = false;
      Dequeue();
   }

   string const FailReason = LookupTag(Message, "FailReason");
   if (FailReason == "MaximumSizeExceeded")
      RenameOnError(MaximumSizeExceeded);
   else if (Status == StatAuthError)
      RenameOnError(HashSumMismatch);

   // report mirror failure back to LP if we actually use a mirror
   if (FailReason.empty() == false)
      ReportMirrorFailure(FailReason);
   else
      ReportMirrorFailure(ErrorText);

   if (QueueCounter > 1)
      Status = StatIdle;
}
									/*}}}*/
// Acquire::Item::Start - Item has begun to download			/*{{{*/
// ---------------------------------------------------------------------
/* Stash status and the file size. Note that setting Complete means
   sub-phases of the acquire process such as decompresion are operating */
void pkgAcquire::Item::Start(string const &/*Message*/, unsigned long long const Size)
{
   Status = StatFetching;
   ErrorText.clear();
   if (FileSize == 0 && Complete == false)
      FileSize = Size;
}
									/*}}}*/
// Acquire::Item::VerifyDone - check if Item was downloaded OK		/*{{{*/
/* Note that hash-verification is 'hardcoded' in acquire-worker and has
 * already passed if this method is called. */
bool pkgAcquire::Item::VerifyDone(std::string const &Message,
	 pkgAcquire::MethodConfig const * const /*Cnf*/)
{
   std::string const FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
      return false;
   }

   return true;
}
									/*}}}*/
// Acquire::Item::Done - Item downloaded OK				/*{{{*/
void pkgAcquire::Item::Done(string const &/*Message*/, HashStringList const &Hashes,
			    pkgAcquire::MethodConfig const * const /*Cnf*/)
{
   // We just downloaded something..
   if (FileSize == 0)
   {
      unsigned long long const downloadedSize = Hashes.FileSize();
      if (downloadedSize != 0)
      {
	 FileSize = downloadedSize;
      }
   }
   Status = StatDone;
   ErrorText = string();
   Owner->Dequeue(this);
}
									/*}}}*/
// Acquire::Item::Rename - Rename a file				/*{{{*/
// ---------------------------------------------------------------------
/* This helper function is used by a lot of item methods as their final
   step */
bool pkgAcquire::Item::Rename(string const &From,string const &To)
{
   if (From == To || rename(From.c_str(),To.c_str()) == 0)
      return true;

   std::string S;
   strprintf(S, _("rename failed, %s (%s -> %s)."), strerror(errno),
	 From.c_str(),To.c_str());
   Status = StatError;
   if (ErrorText.empty())
      ErrorText = S;
   else
      ErrorText = ErrorText + ": " + S;
   return false;
}
									/*}}}*/
void pkgAcquire::Item::Dequeue()					/*{{{*/
{
   Owner->Dequeue(this);
}
									/*}}}*/
bool pkgAcquire::Item::RenameOnError(pkgAcquire::Item::RenameOnErrorState const error)/*{{{*/
{
   if (RealFileExists(DestFile))
      Rename(DestFile, DestFile + ".FAILED");

   std::string errtext;
   switch (error)
   {
      case HashSumMismatch:
	 errtext = _("Hash Sum mismatch");
	 Status = StatAuthError;
	 ReportMirrorFailure("HashChecksumFailure");
	 break;
      case SizeMismatch:
	 errtext = _("Size mismatch");
	 Status = StatAuthError;
	 ReportMirrorFailure("SizeFailure");
	 break;
      case InvalidFormat:
	 errtext = _("Invalid file format");
	 Status = StatError;
	 // do not report as usually its not the mirrors fault, but Portal/Proxy
	 break;
      case SignatureError:
	 errtext = _("Signature error");
	 Status = StatError;
	 break;
      case NotClearsigned:
	 strprintf(errtext, _("Clearsigned file isn't valid, got '%s' (does the network require authentication?)"), "NOSPLIT");
	 Status = StatAuthError;
	 break;
      case MaximumSizeExceeded:
	 // the method is expected to report a good error for this
	 Status = StatError;
	 break;
      case PDiffError:
	 // no handling here, done by callers
	 break;
   }
   if (ErrorText.empty())
      ErrorText = errtext;
   return false;
}
									/*}}}*/
void pkgAcquire::Item::SetActiveSubprocess(const std::string &subprocess)/*{{{*/
{
      ActiveSubprocess = subprocess;
      APT_IGNORE_DEPRECATED(Mode = ActiveSubprocess.c_str();)
}
									/*}}}*/
// Acquire::Item::ReportMirrorFailure					/*{{{*/
void pkgAcquire::Item::ReportMirrorFailure(string const &FailCode)
{
   // we only act if a mirror was used at all
   if(UsedMirror.empty())
      return;
#if 0
   std::cerr << "\nReportMirrorFailure: " 
	     << UsedMirror
	     << " Uri: " << DescURI()
	     << " FailCode: " 
	     << FailCode << std::endl;
#endif
   string report = _config->Find("Methods::Mirror::ProblemReporting", 
				 "/usr/lib/apt/apt-report-mirror-failure");
   if(!FileExists(report))
      return;

   std::vector<char const*> Args;
   Args.push_back(report.c_str());
   Args.push_back(UsedMirror.c_str());
   Args.push_back(DescURI().c_str());
   Args.push_back(FailCode.c_str());
   Args.push_back(NULL);

   pid_t pid = ExecFork();
   if(pid < 0)
   {
      _error->Error("ReportMirrorFailure Fork failed");
      return;
   }
   else if(pid == 0)
   {
      execvp(Args[0], (char**)Args.data());
      std::cerr << "Could not exec " << Args[0] << std::endl;
      _exit(100);
   }
   if(!ExecWait(pid, "report-mirror-failure"))
   {
      _error->Warning("Couldn't report problem to '%s'",
		      _config->Find("Methods::Mirror::ProblemReporting").c_str());
   }
}
									/*}}}*/
std::string pkgAcquire::Item::HashSum() const				/*{{{*/
{
   HashStringList const hashes = GetExpectedHashes();
   HashString const * const hs = hashes.find(NULL);
   return hs != NULL ? hs->toStr() : "";
}
									/*}}}*/

pkgAcqTransactionItem::pkgAcqTransactionItem(pkgAcquire * const Owner,	/*{{{*/
      pkgAcqMetaClearSig * const transactionManager, IndexTarget const &target) :
   pkgAcquire::Item(Owner), d(NULL), Target(target), TransactionManager(transactionManager)
{
   if (TransactionManager != this)
      TransactionManager->Add(this);
}
									/*}}}*/
pkgAcqTransactionItem::~pkgAcqTransactionItem()				/*{{{*/
{
}
									/*}}}*/
HashStringList pkgAcqTransactionItem::GetExpectedHashesFor(std::string const &MetaKey) const	/*{{{*/
{
   return GetExpectedHashesFromFor(TransactionManager->MetaIndexParser, MetaKey);
}
									/*}}}*/

// AcqMetaBase - Constructor						/*{{{*/
pkgAcqMetaBase::pkgAcqMetaBase(pkgAcquire * const Owner,
      pkgAcqMetaClearSig * const TransactionManager,
      std::vector<IndexTarget> const &IndexTargets,
      IndexTarget const &DataTarget)
: pkgAcqTransactionItem(Owner, TransactionManager, DataTarget), d(NULL),
   IndexTargets(IndexTargets),
   AuthPass(false), IMSHit(false)
{
}
									/*}}}*/
// AcqMetaBase::Add - Add a item to the current Transaction		/*{{{*/
void pkgAcqMetaBase::Add(pkgAcqTransactionItem * const I)
{
   Transaction.push_back(I);
}
									/*}}}*/
// AcqMetaBase::AbortTransaction - Abort the current Transaction	/*{{{*/
void pkgAcqMetaBase::AbortTransaction()
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "AbortTransaction: " << TransactionManager << std::endl;

   // ensure the toplevel is in error state too
   for (std::vector<pkgAcqTransactionItem*>::iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      (*I)->TransactionState(TransactionAbort);
   }
   Transaction.clear();
}
									/*}}}*/
// AcqMetaBase::TransactionHasError - Check for errors in Transaction	/*{{{*/
APT_PURE bool pkgAcqMetaBase::TransactionHasError() const
{
   for (std::vector<pkgAcqTransactionItem*>::const_iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      switch((*I)->Status) {
	 case StatDone: break;
	 case StatIdle: break;
	 case StatAuthError: return true;
	 case StatError: return true;
	 case StatTransientNetworkError: return true;
	 case StatFetching: break;
      }
   }
   return false;
}
									/*}}}*/
// AcqMetaBase::CommitTransaction - Commit a transaction		/*{{{*/
void pkgAcqMetaBase::CommitTransaction()
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "CommitTransaction: " << this << std::endl;

   // move new files into place *and* remove files that are not
   // part of the transaction but are still on disk
   for (std::vector<pkgAcqTransactionItem*>::iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      (*I)->TransactionState(TransactionCommit);
   }
   Transaction.clear();
}
									/*}}}*/
// AcqMetaBase::TransactionStageCopy - Stage a file for copying		/*{{{*/
void pkgAcqMetaBase::TransactionStageCopy(pkgAcqTransactionItem * const I,
                                          const std::string &From,
                                          const std::string &To)
{
   I->PartialFile = From;
   I->DestFile = To;
}
									/*}}}*/
// AcqMetaBase::TransactionStageRemoval - Stage a file for removal	/*{{{*/
void pkgAcqMetaBase::TransactionStageRemoval(pkgAcqTransactionItem * const I,
                                             const std::string &FinalFile)
{
   I->PartialFile = "";
   I->DestFile = FinalFile;
}
									/*}}}*/
// AcqMetaBase::GenerateAuthWarning - Check gpg authentication error	/*{{{*/
bool pkgAcqMetaBase::CheckStopAuthentication(pkgAcquire::Item * const I, const std::string &Message)
{
   // FIXME: this entire function can do now that we disallow going to
   //        a unauthenticated state and can cleanly rollback

   string const Final = I->GetFinalFilename();
   if(FileExists(Final))
   {
      I->Status = StatTransientNetworkError;
      _error->Warning(_("An error occurred during the signature "
                        "verification. The repository is not updated "
                        "and the previous index files will be used. "
                        "GPG error: %s: %s"),
                      Desc.Description.c_str(),
                      LookupTag(Message,"Message").c_str());
      RunScripts("APT::Update::Auth-Failure");
      return true;
   } else if (LookupTag(Message,"Message").find("NODATA") != string::npos) {
      /* Invalid signature file, reject (LP: #346386) (Closes: #627642) */
      _error->Error(_("GPG error: %s: %s"),
                    Desc.Description.c_str(),
                    LookupTag(Message,"Message").c_str());
      I->Status = StatAuthError;
      return true;
   } else {
      _error->Warning(_("GPG error: %s: %s"),
                      Desc.Description.c_str(),
                      LookupTag(Message,"Message").c_str());
   }
   // gpgv method failed
   ReportMirrorFailure("GPGFailure");
   return false;
}
									/*}}}*/
// AcqMetaBase::Custom600Headers - Get header for AcqMetaBase		/*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaBase::Custom600Headers() const
{
   std::string Header = "\nIndex-File: true";
   std::string MaximumSize;
   strprintf(MaximumSize, "\nMaximum-Size: %i",
             _config->FindI("Acquire::MaxReleaseFileSize", 10*1000*1000));
   Header += MaximumSize;

   string const FinalFile = GetFinalFilename();
   struct stat Buf;
   if (stat(FinalFile.c_str(),&Buf) == 0)
      Header += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);

   return Header;
}
									/*}}}*/
// AcqMetaBase::QueueForSignatureVerify					/*{{{*/
void pkgAcqMetaBase::QueueForSignatureVerify(pkgAcqTransactionItem * const I, std::string const &File, std::string const &Signature)
{
   AuthPass = true;
   I->Desc.URI = "gpgv:" + Signature;
   I->DestFile = File;
   QueueURI(I->Desc);
   I->SetActiveSubprocess("gpgv");
}
									/*}}}*/
// AcqMetaBase::CheckDownloadDone					/*{{{*/
bool pkgAcqMetaBase::CheckDownloadDone(pkgAcqTransactionItem * const I, const std::string &Message, HashStringList const &Hashes) const
{
   // We have just finished downloading a Release file (it is not
   // verified yet)

   std::string const FileName = LookupTag(Message,"Filename");
   if (FileName != I->DestFile && RealFileExists(I->DestFile) == false)
   {
      I->Local = true;
      I->Desc.URI = "copy:" + FileName;
      I->QueueURI(I->Desc);
      return false;
   }

   // make sure to verify against the right file on I-M-S hit
   bool IMSHit = StringToBool(LookupTag(Message,"IMS-Hit"), false);
   if (IMSHit == false && Hashes.usable())
   {
      // detect IMS-Hits servers haven't detected by Hash comparison
      std::string const FinalFile = I->GetFinalFilename();
      if (RealFileExists(FinalFile) && Hashes.VerifyFile(FinalFile) == true)
      {
	 IMSHit = true;
	 unlink(I->DestFile.c_str());
      }
   }

   if(IMSHit == true)
   {
      // for simplicity, the transaction manager is always InRelease
      // even if it doesn't exist.
      if (TransactionManager != NULL)
	 TransactionManager->IMSHit = true;
      I->PartialFile = I->DestFile = I->GetFinalFilename();
   }

   // set Item to complete as the remaining work is all local (verify etc)
   I->Complete = true;

   return true;
}
									/*}}}*/
bool pkgAcqMetaBase::CheckAuthDone(string const &Message)		/*{{{*/
{
   // At this point, the gpgv method has succeeded, so there is a
   // valid signature from a key in the trusted keyring.  We
   // perform additional verification of its contents, and use them
   // to verify the indexes we are about to download

   if (TransactionManager->IMSHit == false)
   {
      // open the last (In)Release if we have it
      std::string const FinalFile = GetFinalFilename();
      std::string FinalRelease;
      std::string FinalInRelease;
      if (APT::String::Endswith(FinalFile, "InRelease"))
      {
	 FinalInRelease = FinalFile;
	 FinalRelease = FinalFile.substr(0, FinalFile.length() - strlen("InRelease")) + "Release";
      }
      else
      {
	 FinalInRelease = FinalFile.substr(0, FinalFile.length() - strlen("Release")) + "InRelease";
	 FinalRelease = FinalFile;
      }
      if (RealFileExists(FinalInRelease) || RealFileExists(FinalRelease))
      {
	 TransactionManager->LastMetaIndexParser = TransactionManager->MetaIndexParser->UnloadedClone();
	 if (TransactionManager->LastMetaIndexParser != NULL)
	 {
	    _error->PushToStack();
	    if (RealFileExists(FinalInRelease))
	       TransactionManager->LastMetaIndexParser->Load(FinalInRelease, NULL);
	    else
	       TransactionManager->LastMetaIndexParser->Load(FinalRelease, NULL);
	    // its unlikely to happen, but if what we have is bad ignore it
	    if (_error->PendingError())
	    {
	       delete TransactionManager->LastMetaIndexParser;
	       TransactionManager->LastMetaIndexParser = NULL;
	    }
	    _error->RevertToStack();
	 }
      }
   }

   if (TransactionManager->MetaIndexParser->Load(DestFile, &ErrorText) == false)
   {
      Status = StatAuthError;
      return false;
   }

   if (!VerifyVendor(Message))
   {
      Status = StatAuthError;
      return false;
   }

   if (_config->FindB("Debug::pkgAcquire::Auth", false))
      std::cerr << "Signature verification succeeded: "
                << DestFile << std::endl;

   // Download further indexes with verification
   QueueIndexes(true);

   return true;
}
									/*}}}*/
void pkgAcqMetaBase::QueueIndexes(bool const verify)			/*{{{*/
{
   // at this point the real Items are loaded in the fetcher
   ExpectedAdditionalItems = 0;

   for (std::vector <IndexTarget>::const_iterator Target = IndexTargets.begin();
        Target != IndexTargets.end();
        ++Target)
   {
      bool trypdiff = _config->FindB("Acquire::PDiffs", true);
      if (verify == true)
      {
	 if (TransactionManager->MetaIndexParser->Exists(Target->MetaKey) == false)
	 {
	    // optional targets that we do not have in the Release file are skipped
	    if (Target->IsOptional)
	       continue;

	    Status = StatAuthError;
	    strprintf(ErrorText, _("Unable to find expected entry '%s' in Release file (Wrong sources.list entry or malformed file)"), Target->MetaKey.c_str());
	    return;
	 }

	 if (RealFileExists(GetFinalFileNameFromURI(Target->URI)))
	 {
	    if (TransactionManager->LastMetaIndexParser != NULL)
	    {
	       HashStringList const newFile = GetExpectedHashesFromFor(TransactionManager->MetaIndexParser, Target->MetaKey);
	       HashStringList const oldFile = GetExpectedHashesFromFor(TransactionManager->LastMetaIndexParser, Target->MetaKey);
	       if (newFile == oldFile)
	       {
		  // we have the file already, no point in trying to acquire it again
		  new NoActionItem(Owner, *Target);
		  continue;
	       }
	    }
	    else if (TransactionManager->IMSHit == true)
	    {
	       // we have the file already, no point in trying to acquire it again
	       new NoActionItem(Owner, *Target);
	       continue;
	    }
	 }
	 else
	    trypdiff = false; // no file to patch

	 // check if we have patches available
	 trypdiff &= TransactionManager->MetaIndexParser->Exists(Target->MetaKey + ".diff/Index");
      }
      // if we have no file to patch, no point in trying
      trypdiff &= RealFileExists(GetFinalFileNameFromURI(Target->URI));

      // no point in patching from local sources
      if (trypdiff)
      {
	 std::string const proto = Target->URI.substr(0, strlen("file:/"));
	 if (proto == "file:/" || proto == "copy:/" || proto == "cdrom:")
	    trypdiff = false;
      }

      // Queue the Index file (Packages, Sources, Translation-$foo, …)
      if (trypdiff)
         new pkgAcqDiffIndex(Owner, TransactionManager, *Target);
      else
         new pkgAcqIndex(Owner, TransactionManager, *Target);
   }
}
									/*}}}*/
bool pkgAcqMetaBase::VerifyVendor(string const &Message)		/*{{{*/
{
   string::size_type pos;

   // check for missing sigs (that where not fatal because otherwise we had
   // bombed earlier)
   string missingkeys;
   string msg = _("There is no public key available for the "
		  "following key IDs:\n");
   pos = Message.find("NO_PUBKEY ");
   if (pos != std::string::npos)
   {
      string::size_type start = pos+strlen("NO_PUBKEY ");
      string Fingerprint = Message.substr(start, Message.find("\n")-start);
      missingkeys += (Fingerprint);
   }
   if(!missingkeys.empty())
      _error->Warning("%s", (msg + missingkeys).c_str());

   string Transformed = TransactionManager->MetaIndexParser->GetExpectedDist();

   if (Transformed == "../project/experimental")
   {
      Transformed = "experimental";
   }

   pos = Transformed.rfind('/');
   if (pos != string::npos)
   {
      Transformed = Transformed.substr(0, pos);
   }

   if (Transformed == ".")
   {
      Transformed = "";
   }

   if (TransactionManager->MetaIndexParser->GetValidUntil() > 0)
   {
      time_t const invalid_since = time(NULL) - TransactionManager->MetaIndexParser->GetValidUntil();
      if (invalid_since > 0)
      {
	 std::string errmsg;
	 strprintf(errmsg,
	       // TRANSLATOR: The first %s is the URL of the bad Release file, the second is
	       // the time since then the file is invalid - formatted in the same way as in
	       // the download progress display (e.g. 7d 3h 42min 1s)
	       _("Release file for %s is expired (invalid since %s). "
		  "Updates for this repository will not be applied."),
	       Target.URI.c_str(), TimeToStr(invalid_since).c_str());
	 if (ErrorText.empty())
	    ErrorText = errmsg;
	 return _error->Error("%s", errmsg.c_str());
      }
   }

   /* Did we get a file older than what we have? This is a last minute IMS hit and doubles
      as a prevention of downgrading us to older (still valid) files */
   if (TransactionManager->IMSHit == false && TransactionManager->LastMetaIndexParser != NULL &&
	 TransactionManager->LastMetaIndexParser->GetDate() > TransactionManager->MetaIndexParser->GetDate())
   {
      TransactionManager->IMSHit = true;
      unlink(DestFile.c_str());
      PartialFile = DestFile = GetFinalFilename();
      // load the 'old' file in the 'new' one instead of flipping pointers as
      // the new one isn't owned by us, while the old one is so cleanup would be confused.
      TransactionManager->MetaIndexParser->swapLoad(TransactionManager->LastMetaIndexParser);
      delete TransactionManager->LastMetaIndexParser;
      TransactionManager->LastMetaIndexParser = NULL;
   }

   if (_config->FindB("Debug::pkgAcquire::Auth", false)) 
   {
      std::cerr << "Got Codename: " << TransactionManager->MetaIndexParser->GetCodename() << std::endl;
      std::cerr << "Expecting Dist: " << TransactionManager->MetaIndexParser->GetExpectedDist() << std::endl;
      std::cerr << "Transformed Dist: " << Transformed << std::endl;
   }

   if (TransactionManager->MetaIndexParser->CheckDist(Transformed) == false)
   {
      // This might become fatal one day
//       Status = StatAuthError;
//       ErrorText = "Conflicting distribution; expected "
//          + MetaIndexParser->GetExpectedDist() + " but got "
//          + MetaIndexParser->GetCodename();
//       return false;
      if (!Transformed.empty())
      {
         _error->Warning(_("Conflicting distribution: %s (expected %s but got %s)"),
                         Desc.Description.c_str(),
                         Transformed.c_str(),
                         TransactionManager->MetaIndexParser->GetCodename().c_str());
      }
   }

   return true;
}
									/*}}}*/
pkgAcqMetaBase::~pkgAcqMetaBase()
{
}

pkgAcqMetaClearSig::pkgAcqMetaClearSig(pkgAcquire * const Owner,	/*{{{*/
      IndexTarget const &ClearsignedTarget,
      IndexTarget const &DetachedDataTarget, IndexTarget const &DetachedSigTarget,
      std::vector<IndexTarget> const &IndexTargets,
      metaIndex * const MetaIndexParser) :
   pkgAcqMetaIndex(Owner, this, ClearsignedTarget, DetachedSigTarget, IndexTargets),
   d(NULL), ClearsignedTarget(ClearsignedTarget),
   DetachedDataTarget(DetachedDataTarget),
   MetaIndexParser(MetaIndexParser), LastMetaIndexParser(NULL)
{
   // index targets + (worst case:) Release/Release.gpg
   ExpectedAdditionalItems = IndexTargets.size() + 2;
   TransactionManager->Add(this);
}
									/*}}}*/
pkgAcqMetaClearSig::~pkgAcqMetaClearSig()				/*{{{*/
{
   if (LastMetaIndexParser != NULL)
      delete LastMetaIndexParser;
}
									/*}}}*/
// pkgAcqMetaClearSig::Custom600Headers - Insert custom request headers	/*{{{*/
string pkgAcqMetaClearSig::Custom600Headers() const
{
   string Header = pkgAcqMetaBase::Custom600Headers();
   Header += "\nFail-Ignore: true";
   std::string const key = TransactionManager->MetaIndexParser->GetSignedBy();
   if (key.empty() == false)
      Header += "\nSigned-By: " + key;

   return Header;
}
									/*}}}*/
bool pkgAcqMetaClearSig::VerifyDone(std::string const &Message,
	 pkgAcquire::MethodConfig const * const Cnf)
{
   Item::VerifyDone(Message, Cnf);

   if (FileExists(DestFile) && !StartsWithGPGClearTextSignature(DestFile))
      return RenameOnError(NotClearsigned);

   return true;
}
// pkgAcqMetaClearSig::Done - We got a file				/*{{{*/
void pkgAcqMetaClearSig::Done(std::string const &Message,
                              HashStringList const &Hashes,
                              pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Done(Message, Hashes, Cnf);

   if(AuthPass == false)
   {
      if(CheckDownloadDone(this, Message, Hashes) == true)
         QueueForSignatureVerify(this, DestFile, DestFile);
      return;
   }
   else if(CheckAuthDone(Message) == true)
   {
      if (TransactionManager->IMSHit == false)
	 TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());
      else if (RealFileExists(GetFinalFilename()) == false)
      {
	 // We got an InRelease file IMSHit, but we haven't one, which means
	 // we had a valid Release/Release.gpg combo stepping in, which we have
	 // to 'acquire' now to ensure list cleanup isn't removing them
	 new NoActionItem(Owner, DetachedDataTarget);
	 new NoActionItem(Owner, DetachedSigTarget);
      }
   }
}
									/*}}}*/
void pkgAcqMetaClearSig::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf) /*{{{*/
{
   Item::Failed(Message, Cnf);

   // we failed, we will not get additional items from this method
   ExpectedAdditionalItems = 0;

   if (AuthPass == false)
   {
      if (Status == StatAuthError)
      {
	 // if we expected a ClearTextSignature (InRelease) and got a file,
	 // but it wasn't valid we end up here (see VerifyDone).
	 // As these is usually called by web-portals we do not try Release/Release.gpg
	 // as this is gonna fail anyway and instead abort our try (LP#346386)
	 TransactionManager->AbortTransaction();
	 return;
      }

      // Queue the 'old' InRelease file for removal if we try Release.gpg
      // as otherwise the file will stay around and gives a false-auth
      // impression (CVE-2012-0214)
      TransactionManager->TransactionStageRemoval(this, GetFinalFilename());
      Status = StatDone;

      new pkgAcqMetaIndex(Owner, TransactionManager, DetachedDataTarget, DetachedSigTarget, IndexTargets);
   }
   else
   {
      if(CheckStopAuthentication(this, Message))
         return;

      _error->Warning(_("The data from '%s' is not signed. Packages "
                        "from that repository can not be authenticated."),
                      ClearsignedTarget.Description.c_str());

      // No Release file was present, or verification failed, so fall
      // back to queueing Packages files without verification
      // only allow going further if the users explicitely wants it
      if(AllowInsecureRepositories(TransactionManager->MetaIndexParser, TransactionManager, this) == true)
      {
	 Status = StatDone;

	 /* InRelease files become Release files, otherwise
	  * they would be considered as trusted later on */
	 string const FinalRelease = GetFinalFileNameFromURI(DetachedDataTarget.URI);
	 string const PartialRelease = GetPartialFileNameFromURI(DetachedDataTarget.URI);
	 string const FinalReleasegpg = GetFinalFileNameFromURI(DetachedSigTarget.URI);
	 string const FinalInRelease = GetFinalFilename();
	 Rename(DestFile, PartialRelease);
	 TransactionManager->TransactionStageCopy(this, PartialRelease, FinalRelease);

	 if (RealFileExists(FinalReleasegpg) || RealFileExists(FinalInRelease))
	 {
	    // open the last Release if we have it
	    if (TransactionManager->IMSHit == false)
	    {
	       TransactionManager->LastMetaIndexParser = TransactionManager->MetaIndexParser->UnloadedClone();
	       if (TransactionManager->LastMetaIndexParser != NULL)
	       {
		  _error->PushToStack();
		  if (RealFileExists(FinalInRelease))
		     TransactionManager->LastMetaIndexParser->Load(FinalInRelease, NULL);
		  else
		     TransactionManager->LastMetaIndexParser->Load(FinalRelease, NULL);
		  // its unlikely to happen, but if what we have is bad ignore it
		  if (_error->PendingError())
		  {
		     delete TransactionManager->LastMetaIndexParser;
		     TransactionManager->LastMetaIndexParser = NULL;
		  }
		  _error->RevertToStack();
	       }
	    }
	 }

	 // we parse the indexes here because at this point the user wanted
	 // a repository that may potentially harm him
	 if (TransactionManager->MetaIndexParser->Load(PartialRelease, &ErrorText) == false || VerifyVendor(Message) == false)
	    /* expired Release files are still a problem you need extra force for */;
	 else
	    QueueIndexes(true);
      }
   }
}
									/*}}}*/

pkgAcqMetaIndex::pkgAcqMetaIndex(pkgAcquire * const Owner,		/*{{{*/
                                 pkgAcqMetaClearSig * const TransactionManager,
				 IndexTarget const &DataTarget,
				 IndexTarget const &DetachedSigTarget,
				 vector<IndexTarget> const &IndexTargets) :
   pkgAcqMetaBase(Owner, TransactionManager, IndexTargets, DataTarget), d(NULL),
   DetachedSigTarget(DetachedSigTarget)
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgAcqMetaIndex with TransactionManager "
                << this->TransactionManager << std::endl;

   DestFile = GetPartialFileNameFromURI(DataTarget.URI);

   // Create the item
   Desc.Description = DataTarget.Description;
   Desc.Owner = this;
   Desc.ShortDesc = DataTarget.ShortDesc;
   Desc.URI = DataTarget.URI;

   // we expect more item
   ExpectedAdditionalItems = IndexTargets.size();
   QueueURI(Desc);
}
									/*}}}*/
void pkgAcqMetaIndex::Done(string const &Message,			/*{{{*/
                           HashStringList const &Hashes,
			   pkgAcquire::MethodConfig const * const Cfg)
{
   Item::Done(Message,Hashes,Cfg);

   if(CheckDownloadDone(this, Message, Hashes))
   {
      // we have a Release file, now download the Signature, all further
      // verify/queue for additional downloads will be done in the
      // pkgAcqMetaSig::Done() code
      new pkgAcqMetaSig(Owner, TransactionManager, DetachedSigTarget, this);
   }
}
									/*}}}*/
// pkgAcqMetaIndex::Failed - no Release file present			/*{{{*/
void pkgAcqMetaIndex::Failed(string const &Message,
                             pkgAcquire::MethodConfig const * const Cnf)
{
   pkgAcquire::Item::Failed(Message, Cnf);
   Status = StatDone;

   _error->Warning(_("The repository '%s' does not have a Release file. "
                     "This is deprecated, please contact the owner of the "
                     "repository."), Target.Description.c_str());

   // No Release file was present so fall
   // back to queueing Packages files without verification
   // only allow going further if the users explicitely wants it
   if(AllowInsecureRepositories(TransactionManager->MetaIndexParser, TransactionManager, this) == true)
   {
      // ensure old Release files are removed
      TransactionManager->TransactionStageRemoval(this, GetFinalFilename());

      // queue without any kind of hashsum support
      QueueIndexes(false);
   }
}
									/*}}}*/
void pkgAcqMetaIndex::Finished()					/*{{{*/
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "Finished: " << DestFile <<std::endl;
   if(TransactionManager != NULL &&
      TransactionManager->TransactionHasError() == false)
      TransactionManager->CommitTransaction();
}
									/*}}}*/
std::string pkgAcqMetaIndex::DescURI() const				/*{{{*/
{
   return Target.URI;
}
									/*}}}*/
pkgAcqMetaIndex::~pkgAcqMetaIndex() {}

// AcqMetaSig::AcqMetaSig - Constructor					/*{{{*/
pkgAcqMetaSig::pkgAcqMetaSig(pkgAcquire * const Owner,
      pkgAcqMetaClearSig * const TransactionManager,
      IndexTarget const &Target,
      pkgAcqMetaIndex * const MetaIndex) :
   pkgAcqTransactionItem(Owner, TransactionManager, Target), d(NULL), MetaIndex(MetaIndex)
{
   DestFile = GetPartialFileNameFromURI(Target.URI);

   // remove any partial downloaded sig-file in partial/.
   // it may confuse proxies and is too small to warrant a
   // partial download anyway
   unlink(DestFile.c_str());

   // set the TransactionManager
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgAcqMetaSig with TransactionManager "
                << TransactionManager << std::endl;

   // Create the item
   Desc.Description = Target.Description;
   Desc.Owner = this;
   Desc.ShortDesc = Target.ShortDesc;
   Desc.URI = Target.URI;

   // If we got a hit for Release, we will get one for Release.gpg too (or obscure errors),
   // so we skip the download step and go instantly to verification
   if (TransactionManager->IMSHit == true && RealFileExists(GetFinalFilename()))
   {
      Complete = true;
      Status = StatDone;
      PartialFile = DestFile = GetFinalFilename();
      MetaIndexFileSignature = DestFile;
      MetaIndex->QueueForSignatureVerify(this, MetaIndex->DestFile, DestFile);
   }
   else
      QueueURI(Desc);
}
									/*}}}*/
pkgAcqMetaSig::~pkgAcqMetaSig()						/*{{{*/
{
}
									/*}}}*/
// pkgAcqMetaSig::Custom600Headers - Insert custom request headers	/*{{{*/
std::string pkgAcqMetaSig::Custom600Headers() const
{
   std::string Header = pkgAcqTransactionItem::Custom600Headers();
   std::string const key = TransactionManager->MetaIndexParser->GetSignedBy();
   if (key.empty() == false)
      Header += "\nSigned-By: " + key;
   return Header;
}
									/*}}}*/
// AcqMetaSig::Done - The signature was downloaded/verified		/*{{{*/
void pkgAcqMetaSig::Done(string const &Message, HashStringList const &Hashes,
			 pkgAcquire::MethodConfig const * const Cfg)
{
   if (MetaIndexFileSignature.empty() == false)
   {
      DestFile = MetaIndexFileSignature;
      MetaIndexFileSignature.clear();
   }
   Item::Done(Message, Hashes, Cfg);

   if(MetaIndex->AuthPass == false)
   {
      if(MetaIndex->CheckDownloadDone(this, Message, Hashes) == true)
      {
	 // destfile will be modified to point to MetaIndexFile for the
	 // gpgv method, so we need to save it here
	 MetaIndexFileSignature = DestFile;
	 MetaIndex->QueueForSignatureVerify(this, MetaIndex->DestFile, DestFile);
      }
      return;
   }
   else if(MetaIndex->CheckAuthDone(Message) == true)
   {
      if (TransactionManager->IMSHit == false)
      {
	 TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());
	 TransactionManager->TransactionStageCopy(MetaIndex, MetaIndex->DestFile, MetaIndex->GetFinalFilename());
      }
   }
}
									/*}}}*/
void pkgAcqMetaSig::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   Item::Failed(Message,Cnf);

   // check if we need to fail at this point
   if (MetaIndex->AuthPass == true && MetaIndex->CheckStopAuthentication(this, Message))
         return;

   string const FinalRelease = MetaIndex->GetFinalFilename();
   string const FinalReleasegpg = GetFinalFilename();
   string const FinalInRelease = TransactionManager->GetFinalFilename();

   if (RealFileExists(FinalReleasegpg) || RealFileExists(FinalInRelease))
   {
      std::string downgrade_msg;
      strprintf(downgrade_msg, _("The repository '%s' is no longer signed."),
                MetaIndex->Target.Description.c_str());
      if(_config->FindB("Acquire::AllowDowngradeToInsecureRepositories"))
      {
         // meh, the users wants to take risks (we still mark the packages
         // from this repository as unauthenticated)
         _error->Warning("%s", downgrade_msg.c_str());
         _error->Warning(_("This is normally not allowed, but the option "
                           "Acquire::AllowDowngradeToInsecureRepositories was "
                           "given to override it."));
         Status = StatDone;
      } else {
         _error->Error("%s", downgrade_msg.c_str());
	 if (TransactionManager->IMSHit == false)
	    Rename(MetaIndex->DestFile, MetaIndex->DestFile + ".FAILED");
	 Item::Failed("Message: " + downgrade_msg, Cnf);
         TransactionManager->AbortTransaction();
         return;
      }
   }
   else
      _error->Warning(_("The data from '%s' is not signed. Packages "
	       "from that repository can not be authenticated."),
	    MetaIndex->Target.Description.c_str());

   // ensures that a Release.gpg file in the lists/ is removed by the transaction
   TransactionManager->TransactionStageRemoval(this, DestFile);

   // only allow going further if the users explicitely wants it
   if(AllowInsecureRepositories(TransactionManager->MetaIndexParser, TransactionManager, this) == true)
   {
      if (RealFileExists(FinalReleasegpg) || RealFileExists(FinalInRelease))
      {
	 // open the last Release if we have it
	 if (TransactionManager->IMSHit == false)
	 {
	    TransactionManager->LastMetaIndexParser = TransactionManager->MetaIndexParser->UnloadedClone();
	    if (TransactionManager->LastMetaIndexParser != NULL)
	    {
	       _error->PushToStack();
	       if (RealFileExists(FinalInRelease))
		  TransactionManager->LastMetaIndexParser->Load(FinalInRelease, NULL);
	       else
		  TransactionManager->LastMetaIndexParser->Load(FinalRelease, NULL);
	       // its unlikely to happen, but if what we have is bad ignore it
	       if (_error->PendingError())
	       {
		  delete TransactionManager->LastMetaIndexParser;
		  TransactionManager->LastMetaIndexParser = NULL;
	       }
	       _error->RevertToStack();
	    }
	 }
      }

      // we parse the indexes here because at this point the user wanted
      // a repository that may potentially harm him
      if (TransactionManager->MetaIndexParser->Load(MetaIndex->DestFile, &ErrorText) == false || MetaIndex->VerifyVendor(Message) == false)
	 /* expired Release files are still a problem you need extra force for */;
      else
	 MetaIndex->QueueIndexes(true);

      TransactionManager->TransactionStageCopy(MetaIndex, MetaIndex->DestFile, MetaIndex->GetFinalFilename());
   }

   // FIXME: this is used often (e.g. in pkgAcqIndexTrans) so refactor
   if (Cnf->LocalOnly == true ||
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == false)
   {
      // Ignore this
      Status = StatDone;
   }
}
									/*}}}*/


// AcqBaseIndex - Constructor						/*{{{*/
pkgAcqBaseIndex::pkgAcqBaseIndex(pkgAcquire * const Owner,
      pkgAcqMetaClearSig * const TransactionManager,
      IndexTarget const &Target)
: pkgAcqTransactionItem(Owner, TransactionManager, Target), d(NULL)
{
}
									/*}}}*/
pkgAcqBaseIndex::~pkgAcqBaseIndex() {}

// AcqDiffIndex::AcqDiffIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Get the DiffIndex file first and see if there are patches available
 * If so, create a pkgAcqIndexDiffs fetcher that will get and apply the
 * patches. If anything goes wrong in that process, it will fall back to
 * the original packages file
 */
pkgAcqDiffIndex::pkgAcqDiffIndex(pkgAcquire * const Owner,
                                 pkgAcqMetaClearSig * const TransactionManager,
                                 IndexTarget const &Target)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target), d(NULL), diffs(NULL)
{
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   Desc.Owner = this;
   Desc.Description = Target.Description + ".diff/Index";
   Desc.ShortDesc = Target.ShortDesc;
   Desc.URI = Target.URI + ".diff/Index";

   DestFile = GetPartialFileNameFromURI(Desc.URI);

   if(Debug)
      std::clog << "pkgAcqDiffIndex: " << Desc.URI << std::endl;

   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqDiffIndex::Custom600Headers() const
{
   string const Final = GetFinalFilename();

   if(Debug)
      std::clog << "Custom600Header-IMS: " << Final << std::endl;

   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return "\nIndex-File: true";
   
   return "\nIndex-File: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
void pkgAcqDiffIndex::QueueOnIMSHit() const				/*{{{*/
{
   // list cleanup needs to know that this file as well as the already
   // present index is ours, so we create an empty diff to save it for us
   new pkgAcqIndexDiffs(Owner, TransactionManager, Target);
}
									/*}}}*/
bool pkgAcqDiffIndex::ParseDiffIndex(string const &IndexDiffFile)	/*{{{*/
{
   // failing here is fine: our caller will take care of trying to
   // get the complete file if patching fails
   if(Debug)
      std::clog << "pkgAcqDiffIndex::ParseIndexDiff() " << IndexDiffFile
	 << std::endl;

   FileFd Fd(IndexDiffFile,FileFd::ReadOnly);
   pkgTagFile TF(&Fd);
   if (_error->PendingError() == true)
      return false;

   pkgTagSection Tags;
   if(unlikely(TF.Step(Tags) == false))
      return false;

   HashStringList ServerHashes;
   unsigned long long ServerSize = 0;

   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      std::string tagname = *type;
      tagname.append("-Current");
      std::string const tmp = Tags.FindS(tagname.c_str());
      if (tmp.empty() == true)
	 continue;

      string hash;
      unsigned long long size;
      std::stringstream ss(tmp);
      ss >> hash >> size;
      if (unlikely(hash.empty() == true))
	 continue;
      if (unlikely(ServerSize != 0 && ServerSize != size))
	 continue;
      ServerHashes.push_back(HashString(*type, hash));
      ServerSize = size;
   }

   if (ServerHashes.usable() == false)
   {
      if (Debug == true)
	 std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": Did not find a good hashsum in the index" << std::endl;
      return false;
   }

   std::string const CurrentPackagesFile = GetFinalFileNameFromURI(Target.URI);
   HashStringList const TargetFileHashes = GetExpectedHashesFor(Target.MetaKey);
   if (TargetFileHashes.usable() == false || ServerHashes != TargetFileHashes)
   {
      if (Debug == true)
      {
	 std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": Index has different hashes than parser, probably older, so fail pdiffing" << std::endl;
         printHashSumComparision(CurrentPackagesFile, ServerHashes, TargetFileHashes);
      }
      return false;
   }

   HashStringList LocalHashes;
   // try avoiding calculating the hash here as this is costly
   if (TransactionManager->LastMetaIndexParser != NULL)
      LocalHashes = GetExpectedHashesFromFor(TransactionManager->LastMetaIndexParser, Target.MetaKey);
   if (LocalHashes.usable() == false)
   {
      FileFd fd(CurrentPackagesFile, FileFd::ReadOnly);
      Hashes LocalHashesCalc(ServerHashes);
      LocalHashesCalc.AddFD(fd);
      LocalHashes = LocalHashesCalc.GetHashStringList();
   }

   if (ServerHashes == LocalHashes)
   {
      // we have the same sha1 as the server so we are done here
      if(Debug)
	 std::clog << "pkgAcqDiffIndex: Package file " << CurrentPackagesFile << " is up-to-date" << std::endl;
      QueueOnIMSHit();
      return true;
   }

   if(Debug)
      std::clog << "Server-Current: " << ServerHashes.find(NULL)->toStr() << " and we start at "
	 << CurrentPackagesFile << " " << LocalHashes.FileSize() << " " << LocalHashes.find(NULL)->toStr() << std::endl;

   // parse all of (provided) history
   vector<DiffInfo> available_patches;
   bool firstAcceptedHashes = true;
   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      if (LocalHashes.find(*type) == NULL)
	 continue;

      std::string tagname = *type;
      tagname.append("-History");
      std::string const tmp = Tags.FindS(tagname.c_str());
      if (tmp.empty() == true)
	 continue;

      string hash, filename;
      unsigned long long size;
      std::stringstream ss(tmp);

      while (ss >> hash >> size >> filename)
      {
	 if (unlikely(hash.empty() == true || filename.empty() == true))
	    continue;

	 // see if we have a record for this file already
	 std::vector<DiffInfo>::iterator cur = available_patches.begin();
	 for (; cur != available_patches.end(); ++cur)
	 {
	    if (cur->file != filename)
	       continue;
	    cur->result_hashes.push_back(HashString(*type, hash));
	    break;
	 }
	 if (cur != available_patches.end())
	    continue;
	 if (firstAcceptedHashes == true)
	 {
	    DiffInfo next;
	    next.file = filename;
	    next.result_hashes.push_back(HashString(*type, hash));
	    next.result_hashes.FileSize(size);
	    available_patches.push_back(next);
	 }
	 else
	 {
	    if (Debug == true)
	       std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": File " << filename
		  << " wasn't in the list for the first parsed hash! (history)" << std::endl;
	    break;
	 }
      }
      firstAcceptedHashes = false;
   }

   if (unlikely(available_patches.empty() == true))
   {
      if (Debug)
	 std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": "
	    << "Couldn't find any patches for the patch series." << std::endl;
      return false;
   }

   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      if (LocalHashes.find(*type) == NULL)
	 continue;

      std::string tagname = *type;
      tagname.append("-Patches");
      std::string const tmp = Tags.FindS(tagname.c_str());
      if (tmp.empty() == true)
	 continue;

      string hash, filename;
      unsigned long long size;
      std::stringstream ss(tmp);

      while (ss >> hash >> size >> filename)
      {
	 if (unlikely(hash.empty() == true || filename.empty() == true))
	    continue;

	 // see if we have a record for this file already
	 std::vector<DiffInfo>::iterator cur = available_patches.begin();
	 for (; cur != available_patches.end(); ++cur)
	 {
	    if (cur->file != filename)
	       continue;
	    if (cur->patch_hashes.empty())
	       cur->patch_hashes.FileSize(size);
	    cur->patch_hashes.push_back(HashString(*type, hash));
	    break;
	 }
	 if (cur != available_patches.end())
	       continue;
	 if (Debug == true)
	    std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": File " << filename
	       << " wasn't in the list for the first parsed hash! (patches)" << std::endl;
	 break;
      }
   }

   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
   {
      std::string tagname = *type;
      tagname.append("-Download");
      std::string const tmp = Tags.FindS(tagname.c_str());
      if (tmp.empty() == true)
	 continue;

      string hash, filename;
      unsigned long long size;
      std::stringstream ss(tmp);

      // FIXME: all of pdiff supports only .gz compressed patches
      while (ss >> hash >> size >> filename)
      {
	 if (unlikely(hash.empty() == true || filename.empty() == true))
	    continue;
	 if (unlikely(APT::String::Endswith(filename, ".gz") == false))
	    continue;
	 filename.erase(filename.length() - 3);

	 // see if we have a record for this file already
	 std::vector<DiffInfo>::iterator cur = available_patches.begin();
	 for (; cur != available_patches.end(); ++cur)
	 {
	    if (cur->file != filename)
	       continue;
	    if (cur->download_hashes.empty())
	       cur->download_hashes.FileSize(size);
	    cur->download_hashes.push_back(HashString(*type, hash));
	    break;
	 }
	 if (cur != available_patches.end())
	       continue;
	 if (Debug == true)
	    std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": File " << filename
	       << " wasn't in the list for the first parsed hash! (download)" << std::endl;
	 break;
      }
   }


   bool foundStart = false;
   for (std::vector<DiffInfo>::iterator cur = available_patches.begin();
	 cur != available_patches.end(); ++cur)
   {
      if (LocalHashes != cur->result_hashes)
	 continue;

      available_patches.erase(available_patches.begin(), cur);
      foundStart = true;
      break;
   }

   if (foundStart == false || unlikely(available_patches.empty() == true))
   {
      if (Debug)
	 std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": "
	    << "Couldn't find the start of the patch series." << std::endl;
      return false;
   }

   // patching with too many files is rather slow compared to a fast download
   unsigned long const fileLimit = _config->FindI("Acquire::PDiffs::FileLimit", 0);
   if (fileLimit != 0 && fileLimit < available_patches.size())
   {
      if (Debug)
	 std::clog << "Need " << available_patches.size() << " diffs (Limit is " << fileLimit
	    << ") so fallback to complete download" << std::endl;
      return false;
   }

   // calculate the size of all patches we have to get
   // note that all sizes are uncompressed, while we download compressed files
   unsigned long long patchesSize = 0;
   for (std::vector<DiffInfo>::const_iterator cur = available_patches.begin();
	 cur != available_patches.end(); ++cur)
      patchesSize += cur->patch_hashes.FileSize();
   unsigned long long const sizeLimit = ServerSize * _config->FindI("Acquire::PDiffs::SizeLimit", 100);
   if (sizeLimit > 0 && (sizeLimit/100) < patchesSize)
   {
      if (Debug)
	 std::clog << "Need " << patchesSize << " bytes (Limit is " << sizeLimit/100
	    << ") so fallback to complete download" << std::endl;
      return false;
   }

   // we have something, queue the diffs
   string::size_type const last_space = Description.rfind(" ");
   if(last_space != string::npos)
      Description.erase(last_space, Description.size()-last_space);

   /* decide if we should download patches one by one or in one go:
      The first is good if the server merges patches, but many don't so client
      based merging can be attempt in which case the second is better.
      "bad things" will happen if patches are merged on the server,
      but client side merging is attempt as well */
   bool pdiff_merge = _config->FindB("Acquire::PDiffs::Merge", true);
   if (pdiff_merge == true)
   {
      // reprepro adds this flag if it has merged patches on the server
      std::string const precedence = Tags.FindS("X-Patch-Precedence");
      pdiff_merge = (precedence != "merged");
   }

   if (pdiff_merge == false)
      new pkgAcqIndexDiffs(Owner, TransactionManager, Target, available_patches);
   else
   {
      diffs = new std::vector<pkgAcqIndexMergeDiffs*>(available_patches.size());
      for(size_t i = 0; i < available_patches.size(); ++i)
	 (*diffs)[i] = new pkgAcqIndexMergeDiffs(Owner, TransactionManager,
               Target,
	       available_patches[i],
	       diffs);
   }

   Complete = false;
   Status = StatDone;
   Dequeue();
   return true;
}
									/*}}}*/
void pkgAcqDiffIndex::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   Item::Failed(Message,Cnf);
   Status = StatDone;

   if(Debug)
      std::clog << "pkgAcqDiffIndex failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;

   new pkgAcqIndex(Owner, TransactionManager, Target);
}
									/*}}}*/
void pkgAcqDiffIndex::Done(string const &Message,HashStringList const &Hashes,	/*{{{*/
			   pkgAcquire::MethodConfig const * const Cnf)
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   string const FinalFile = GetFinalFilename();
   if(StringToBool(LookupTag(Message,"IMS-Hit"),false))
      DestFile = FinalFile;

   if(ParseDiffIndex(DestFile) == false)
   {
      Failed("Message: Couldn't parse pdiff index", Cnf);
      // queue for final move - this should happen even if we fail
      // while parsing (e.g. on sizelimit) and download the complete file.
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
      return;
   }

   TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);

   Complete = true;
   Status = StatDone;
   Dequeue();

   return;
}
									/*}}}*/
pkgAcqDiffIndex::~pkgAcqDiffIndex()
{
   if (diffs != NULL)
      delete diffs;
}

// AcqIndexDiffs::AcqIndexDiffs - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* The package diff is added to the queue. one object is constructed
 * for each diff and the index
 */
pkgAcqIndexDiffs::pkgAcqIndexDiffs(pkgAcquire * const Owner,
                                   pkgAcqMetaClearSig * const TransactionManager,
                                   IndexTarget const &Target,
				   vector<DiffInfo> const &diffs)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target), d(NULL),
     available_patches(diffs)
{
   DestFile = GetPartialFileNameFromURI(Target.URI);

   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   Desc.Owner = this;
   Description = Target.Description;
   Desc.ShortDesc = Target.ShortDesc;

   if(available_patches.empty() == true)
   {
      // we are done (yeah!), check hashes against the final file
      DestFile = GetFinalFileNameFromURI(Target.URI);
      Finish(true);
   }
   else
   {
      // patching needs to be bootstrapped with the 'old' version
      std::string const PartialFile = GetPartialFileNameFromURI(Target.URI);
      if (RealFileExists(PartialFile) == false)
      {
	 if (symlink(GetFinalFilename().c_str(), PartialFile.c_str()) != 0)
	 {
	    Failed("Link creation of " + PartialFile + " to " + GetFinalFilename() + " failed", NULL);
	    return;
	 }
      }

      // get the next diff
      State = StateFetchDiff;
      QueueNextDiff();
   }
}
									/*}}}*/
void pkgAcqIndexDiffs::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   Item::Failed(Message,Cnf);
   Status = StatDone;

   if(Debug)
      std::clog << "pkgAcqIndexDiffs failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;
   DestFile = GetPartialFileNameFromURI(Target.URI);
   RenameOnError(PDiffError);
   std::string const patchname = GetDiffsPatchFileName(DestFile);
   if (RealFileExists(patchname))
      rename(patchname.c_str(), std::string(patchname + ".FAILED").c_str());
   new pkgAcqIndex(Owner, TransactionManager, Target);
   Finish();
}
									/*}}}*/
// Finish - helper that cleans the item out of the fetcher queue	/*{{{*/
void pkgAcqIndexDiffs::Finish(bool allDone)
{
   if(Debug)
      std::clog << "pkgAcqIndexDiffs::Finish(): " 
                << allDone << " "
                << Desc.URI << std::endl;

   // we restore the original name, this is required, otherwise
   // the file will be cleaned
   if(allDone)
   {
      TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());

      // this is for the "real" finish
      Complete = true;
      Status = StatDone;
      Dequeue();
      if(Debug)
	 std::clog << "\n\nallDone: " << DestFile << "\n" << std::endl;
      return;
   }

   if(Debug)
      std::clog << "Finishing: " << Desc.URI << std::endl;
   Complete = false;
   Status = StatDone;
   Dequeue();
   return;
}
									/*}}}*/
bool pkgAcqIndexDiffs::QueueNextDiff()					/*{{{*/
{
   // calc sha1 of the just patched file
   std::string const FinalFile = GetPartialFileNameFromURI(Target.URI);

   if(!FileExists(FinalFile))
   {
      Failed("Message: No FinalFile " + FinalFile + " available", NULL);
      return false;
   }

   FileFd fd(FinalFile, FileFd::ReadOnly);
   Hashes LocalHashesCalc;
   LocalHashesCalc.AddFD(fd);
   HashStringList const LocalHashes = LocalHashesCalc.GetHashStringList();

   if(Debug)
      std::clog << "QueueNextDiff: " << FinalFile << " (" << LocalHashes.find(NULL)->toStr() << ")" << std::endl;

   HashStringList const TargetFileHashes = GetExpectedHashesFor(Target.MetaKey);
   if (unlikely(LocalHashes.usable() == false || TargetFileHashes.usable() == false))
   {
      Failed("Local/Expected hashes are not usable", NULL);
      return false;
   }


   // final file reached before all patches are applied
   if(LocalHashes == TargetFileHashes)
   {
      Finish(true);
      return true;
   }

   // remove all patches until the next matching patch is found
   // this requires the Index file to be ordered
   for(vector<DiffInfo>::iterator I = available_patches.begin();
       available_patches.empty() == false &&
	  I != available_patches.end() &&
	  I->result_hashes != LocalHashes;
       ++I)
   {
      available_patches.erase(I);
   }

   // error checking and falling back if no patch was found
   if(available_patches.empty() == true)
   {
      Failed("No patches left to reach target", NULL);
      return false;
   }

   // queue the right diff
   Desc.URI = Target.URI + ".diff/" + available_patches[0].file + ".gz";
   Desc.Description = Description + " " + available_patches[0].file + string(".pdiff");
   DestFile = GetPartialFileNameFromURI(Target.URI + ".diff/" + available_patches[0].file);

   if(Debug)
      std::clog << "pkgAcqIndexDiffs::QueueNextDiff(): " << Desc.URI << std::endl;

   QueueURI(Desc);

   return true;
}
									/*}}}*/
void pkgAcqIndexDiffs::Done(string const &Message, HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig const * const Cnf)
{
   if(Debug)
      std::clog << "pkgAcqIndexDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   std::string const FinalFile = GetPartialFileNameFromURI(Target.URI);
   std::string const PatchFile = GetDiffsPatchFileName(FinalFile);

   // success in downloading a diff, enter ApplyDiff state
   if(State == StateFetchDiff)
   {
      Rename(DestFile, PatchFile);

      if(Debug)
	 std::clog << "Sending to rred method: " << FinalFile << std::endl;

      State = StateApplyDiff;
      Local = true;
      Desc.URI = "rred:" + FinalFile;
      QueueURI(Desc);
      SetActiveSubprocess("rred");
      return;
   }

   // success in download/apply a diff, queue next (if needed)
   if(State == StateApplyDiff)
   {
      // remove the just applied patch
      available_patches.erase(available_patches.begin());
      unlink(PatchFile.c_str());

      // move into place
      if(Debug)
      {
	 std::clog << "Moving patched file in place: " << std::endl
		   << DestFile << " -> " << FinalFile << std::endl;
      }
      Rename(DestFile,FinalFile);
      chmod(FinalFile.c_str(),0644);

      // see if there is more to download
      if(available_patches.empty() == false) {
	 new pkgAcqIndexDiffs(Owner, TransactionManager, Target,
			      available_patches);
	 return Finish();
      } else 
         // update
         DestFile = FinalFile;
	 return Finish(true);
   }
}
									/*}}}*/
std::string pkgAcqIndexDiffs::Custom600Headers() const			/*{{{*/
{
   if(State != StateApplyDiff)
      return pkgAcqBaseIndex::Custom600Headers();
   std::ostringstream patchhashes;
   HashStringList const ExpectedHashes = available_patches[0].patch_hashes;
   for (HashStringList::const_iterator hs = ExpectedHashes.begin(); hs != ExpectedHashes.end(); ++hs)
      patchhashes <<  "\nPatch-0-" << hs->HashType() << "-Hash: " << hs->HashValue();
   patchhashes << pkgAcqBaseIndex::Custom600Headers();
   return patchhashes.str();
}
									/*}}}*/
pkgAcqIndexDiffs::~pkgAcqIndexDiffs() {}

// AcqIndexMergeDiffs::AcqIndexMergeDiffs - Constructor			/*{{{*/
pkgAcqIndexMergeDiffs::pkgAcqIndexMergeDiffs(pkgAcquire * const Owner,
                                             pkgAcqMetaClearSig * const TransactionManager,
                                             IndexTarget const &Target,
                                             DiffInfo const &patch,
                                             std::vector<pkgAcqIndexMergeDiffs*> const * const allPatches)
  : pkgAcqBaseIndex(Owner, TransactionManager, Target), d(NULL),
     patch(patch), allPatches(allPatches), State(StateFetchDiff)
{
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   Desc.Owner = this;
   Description = Target.Description;
   Desc.ShortDesc = Target.ShortDesc;

   Desc.URI = Target.URI + ".diff/" + patch.file + ".gz";
   Desc.Description = Description + " " + patch.file + string(".pdiff");

   DestFile = GetPartialFileNameFromURI(Target.URI + ".diff/" + patch.file);

   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs: " << Desc.URI << std::endl;

   QueueURI(Desc);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs failed: " << Desc.URI << " with " << Message << std::endl;

   Item::Failed(Message,Cnf);
   Status = StatDone;

   // check if we are the first to fail, otherwise we are done here
   State = StateDoneDiff;
   for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	 I != allPatches->end(); ++I)
      if ((*I)->State == StateErrorDiff)
	 return;

   // first failure means we should fallback
   State = StateErrorDiff;
   if (Debug)
      std::clog << "Falling back to normal index file acquire" << std::endl;
   DestFile = GetPartialFileNameFromURI(Target.URI);
   RenameOnError(PDiffError);
   std::string const patchname = GetMergeDiffsPatchFileName(DestFile, patch.file);
   if (RealFileExists(patchname))
      rename(patchname.c_str(), std::string(patchname + ".FAILED").c_str());
   new pkgAcqIndex(Owner, TransactionManager, Target);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Done(string const &Message, HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig const * const Cnf)
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   string const FinalFile = GetPartialFileNameFromURI(Target.URI);
   if (State == StateFetchDiff)
   {
      Rename(DestFile, GetMergeDiffsPatchFileName(FinalFile, patch.file));

      // check if this is the last completed diff
      State = StateDoneDiff;
      for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	    I != allPatches->end(); ++I)
	 if ((*I)->State != StateDoneDiff)
	 {
	    if(Debug)
	       std::clog << "Not the last done diff in the batch: " << Desc.URI << std::endl;
	    return;
	 }

      // this is the last completed diff, so we are ready to apply now
      State = StateApplyDiff;

      // patching needs to be bootstrapped with the 'old' version
      if (symlink(GetFinalFilename().c_str(), FinalFile.c_str()) != 0)
      {
	 Failed("Link creation of " + FinalFile + " to " + GetFinalFilename() + " failed", NULL);
	 return;
      }

      if(Debug)
	 std::clog << "Sending to rred method: " << FinalFile << std::endl;

      Local = true;
      Desc.URI = "rred:" + FinalFile;
      QueueURI(Desc);
      SetActiveSubprocess("rred");
      return;
   }
   // success in download/apply all diffs, clean up
   else if (State == StateApplyDiff)
   {
      // move the result into place
      std::string const Final = GetFinalFilename();
      if(Debug)
	 std::clog << "Queue patched file in place: " << std::endl
		   << DestFile << " -> " << Final << std::endl;

      // queue for copy by the transaction manager
      TransactionManager->TransactionStageCopy(this, DestFile, Final);

      // ensure the ed's are gone regardless of list-cleanup
      for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	    I != allPatches->end(); ++I)
      {
	 std::string const PartialFile = GetPartialFileNameFromURI(Target.URI);
	 std::string const patch = GetMergeDiffsPatchFileName(PartialFile, (*I)->patch.file);
	 unlink(patch.c_str());
      }
      unlink(FinalFile.c_str());

      // all set and done
      Complete = true;
      if(Debug)
	 std::clog << "allDone: " << DestFile << "\n" << std::endl;
   }
}
									/*}}}*/
std::string pkgAcqIndexMergeDiffs::Custom600Headers() const		/*{{{*/
{
   if(State != StateApplyDiff)
      return pkgAcqBaseIndex::Custom600Headers();
   std::ostringstream patchhashes;
   unsigned int seen_patches = 0;
   for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	 I != allPatches->end(); ++I)
   {
      HashStringList const ExpectedHashes = (*I)->patch.patch_hashes;
      for (HashStringList::const_iterator hs = ExpectedHashes.begin(); hs != ExpectedHashes.end(); ++hs)
	 patchhashes <<  "\nPatch-" << seen_patches << "-" << hs->HashType() << "-Hash: " << hs->HashValue();
      ++seen_patches;
   }
   patchhashes << pkgAcqBaseIndex::Custom600Headers();
   return patchhashes.str();
}
									/*}}}*/
pkgAcqIndexMergeDiffs::~pkgAcqIndexMergeDiffs() {}

// AcqIndex::AcqIndex - Constructor					/*{{{*/
pkgAcqIndex::pkgAcqIndex(pkgAcquire * const Owner,
                         pkgAcqMetaClearSig * const TransactionManager,
                         IndexTarget const &Target)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target), d(NULL), Stage(STAGE_DOWNLOAD)
{
   // autoselect the compression method
   AutoSelectCompression();
   Init(Target.URI, Target.Description, Target.ShortDesc);

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgIndex with TransactionManager "
                << TransactionManager << std::endl;
}
									/*}}}*/
// AcqIndex::AutoSelectCompression - Select compression			/*{{{*/
void pkgAcqIndex::AutoSelectCompression()
{
   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   CompressionExtensions = "";
   if (TransactionManager->MetaIndexParser != NULL && TransactionManager->MetaIndexParser->Exists(Target.MetaKey))
   {
      for (std::vector<std::string>::const_iterator t = types.begin();
           t != types.end(); ++t)
      {
         std::string CompressedMetaKey = string(Target.MetaKey).append(".").append(*t);
         if (*t == "uncompressed" ||
             TransactionManager->MetaIndexParser->Exists(CompressedMetaKey) == true)
            CompressionExtensions.append(*t).append(" ");
      }
   }
   else
   {
      for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
	 CompressionExtensions.append(*t).append(" ");
   }
   if (CompressionExtensions.empty() == false)
      CompressionExtensions.erase(CompressionExtensions.end()-1);
}
									/*}}}*/
// AcqIndex::Init - defered Constructor					/*{{{*/
void pkgAcqIndex::Init(string const &URI, string const &URIDesc,
                       string const &ShortDesc)
{
   Stage = STAGE_DOWNLOAD;

   DestFile = GetPartialFileNameFromURI(URI);

   size_t const nextExt = CompressionExtensions.find(' ');
   if (nextExt == std::string::npos)
   {
      CurrentCompressionExtension = CompressionExtensions;
      CompressionExtensions.clear();
   }
   else
   {
      CurrentCompressionExtension = CompressionExtensions.substr(0, nextExt);
      CompressionExtensions = CompressionExtensions.substr(nextExt+1);
   }

   if (CurrentCompressionExtension == "uncompressed")
   {
      Desc.URI = URI;
   }
   else if (unlikely(CurrentCompressionExtension.empty()))
      return;
   else
   {
      Desc.URI = URI + '.' + CurrentCompressionExtension;
      DestFile = DestFile + '.' + CurrentCompressionExtension;
   }

   if(TransactionManager->MetaIndexParser != NULL)
      InitByHashIfNeeded();

   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;

   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::AdjustForByHash - modify URI for by-hash support		/*{{{*/
void pkgAcqIndex::InitByHashIfNeeded()
{
   // TODO:
   //  - (maybe?) add support for by-hash into the sources.list as flag
   //  - make apt-ftparchive generate the hashes (and expire?)
   std::string HostKnob = "APT::Acquire::" + ::URI(Desc.URI).Host + "::By-Hash";
   if(_config->FindB("APT::Acquire::By-Hash", false) == true ||
      _config->FindB(HostKnob, false) == true ||
      TransactionManager->MetaIndexParser->GetSupportsAcquireByHash())
   {
      HashStringList const Hashes = GetExpectedHashes();
      if(Hashes.usable())
      {
         // FIXME: should we really use the best hash here? or a fixed one?
         HashString const * const TargetHash = Hashes.find("");
         std::string const ByHash = "/by-hash/" + TargetHash->HashType() + "/" + TargetHash->HashValue();
         size_t const trailing_slash = Desc.URI.find_last_of("/");
         Desc.URI = Desc.URI.replace(
            trailing_slash,
            Desc.URI.substr(trailing_slash+1).size()+1,
            ByHash);
      } else {
         _error->Warning(
            "Fetching ByHash requested but can not find record for %s",
            GetMetaKey().c_str());
      }
   }
}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqIndex::Custom600Headers() const
{
   string Final = GetFinalFilename();

   string msg = "\nIndex-File: true";
   struct stat Buf;
   if (stat(Final.c_str(),&Buf) == 0)
      msg += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);

   if(Target.IsOptional)
      msg += "\nFail-Ignore: true";

   return msg;
}
									/*}}}*/
// AcqIndex::Failed - getting the indexfile failed			/*{{{*/
void pkgAcqIndex::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Failed(Message,Cnf);

   // authorisation matches will not be fixed by other compression types
   if (Status != StatAuthError)
   {
      if (CompressionExtensions.empty() == false)
      {
	 Init(Target.URI, Desc.Description, Desc.ShortDesc);
	 Status = StatIdle;
	 return;
      }
   }

   if(Target.IsOptional && GetExpectedHashes().empty() && Stage == STAGE_DOWNLOAD)
      Status = StatDone;
   else
      TransactionManager->AbortTransaction();
}
									/*}}}*/
// AcqIndex::ReverifyAfterIMS - Reverify index after an ims-hit		/*{{{*/
void pkgAcqIndex::ReverifyAfterIMS()
{
   // update destfile to *not* include the compression extension when doing
   // a reverify (as its uncompressed on disk already)
   DestFile = GetCompressedFileName(Target, GetPartialFileNameFromURI(Target.URI), CurrentCompressionExtension);

   // copy FinalFile into partial/ so that we check the hash again
   string FinalFile = GetFinalFilename();
   Stage = STAGE_DECOMPRESS_AND_VERIFY;
   Desc.URI = "copy:" + FinalFile;
   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::Done - Finished a fetch					/*{{{*/
// ---------------------------------------------------------------------
/* This goes through a number of states.. On the initial fetch the
   method could possibly return an alternate filename which points
   to the uncompressed version of the file. If this is so the file
   is copied into the partial directory. In all other cases the file
   is decompressed with a compressed uri. */
void pkgAcqIndex::Done(string const &Message,
                       HashStringList const &Hashes,
		       pkgAcquire::MethodConfig const * const Cfg)
{
   Item::Done(Message,Hashes,Cfg);

   switch(Stage) 
   {
      case STAGE_DOWNLOAD:
         StageDownloadDone(Message, Hashes, Cfg);
         break;
      case STAGE_DECOMPRESS_AND_VERIFY:
         StageDecompressDone(Message, Hashes, Cfg);
         break;
   }
}
									/*}}}*/
// AcqIndex::StageDownloadDone - Queue for decompress and verify	/*{{{*/
void pkgAcqIndex::StageDownloadDone(string const &Message, HashStringList const &,
                                    pkgAcquire::MethodConfig const * const)
{
   Complete = true;

   // Handle the unzipd case
   std::string FileName = LookupTag(Message,"Alt-Filename");
   if (FileName.empty() == false)
   {
      Stage = STAGE_DECOMPRESS_AND_VERIFY;
      Local = true;
      DestFile += ".decomp";
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      SetActiveSubprocess("copy");
      return;
   }
   FileName = LookupTag(Message,"Filename");

   // Methods like e.g. "file:" will give us a (compressed) FileName that is
   // not the "DestFile" we set, in this case we uncompress from the local file
   if (FileName != DestFile && RealFileExists(DestFile) == false)
      Local = true;
   else
      EraseFileName = FileName;

   // we need to verify the file against the current Release file again
   // on if-modfied-since hit to avoid a stale attack against us
   if(StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
   {
      // The files timestamp matches, reverify by copy into partial/
      EraseFileName = "";
      ReverifyAfterIMS();
      return;
   }

   // If we want compressed indexes, just copy in place for hash verification
   if (Target.KeepCompressed == true)
   {
      DestFile = GetPartialFileNameFromURI(Target.URI + '.' + CurrentCompressionExtension);
      EraseFileName = "";
      Stage = STAGE_DECOMPRESS_AND_VERIFY;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      SetActiveSubprocess("copy");
      return;
    }

   // get the binary name for your used compression type
   string decompProg;
   if(CurrentCompressionExtension == "uncompressed")
      decompProg = "copy";
   else
      decompProg = _config->Find(string("Acquire::CompressionTypes::").append(CurrentCompressionExtension),"");
   if(decompProg.empty() == true)
   {
      _error->Error("Unsupported extension: %s", CurrentCompressionExtension.c_str());
      return;
   }

   // queue uri for the next stage
   Stage = STAGE_DECOMPRESS_AND_VERIFY;
   DestFile += ".decomp";
   Desc.URI = decompProg + ":" + FileName;
   QueueURI(Desc);
   SetActiveSubprocess(decompProg);
}
									/*}}}*/
// AcqIndex::StageDecompressDone - Final verification			/*{{{*/
void pkgAcqIndex::StageDecompressDone(string const &,
                                      HashStringList const &,
                                      pkgAcquire::MethodConfig const * const)
{
   // Done, queue for rename on transaction finished
   TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());
   return;
}
									/*}}}*/
pkgAcqIndex::~pkgAcqIndex() {}


// AcqArchive::AcqArchive - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This just sets up the initial fetch environment and queues the first
   possibilitiy */
pkgAcqArchive::pkgAcqArchive(pkgAcquire * const Owner,pkgSourceList * const Sources,
			     pkgRecords * const Recs,pkgCache::VerIterator const &Version,
			     string &StoreFilename) :
               Item(Owner), d(NULL), LocalSource(false), Version(Version), Sources(Sources), Recs(Recs),
               StoreFilename(StoreFilename), Vf(Version.FileList()),
	       Trusted(false)
{
   Retries = _config->FindI("Acquire::Retries",0);

   if (Version.Arch() == 0)
   {
      _error->Error(_("I wasn't able to locate a file for the %s package. "
		      "This might mean you need to manually fix this package. "
		      "(due to missing arch)"),
		    Version.ParentPkg().FullName().c_str());
      return;
   }
   
   /* We need to find a filename to determine the extension. We make the
      assumption here that all the available sources for this version share
      the same extension.. */
   // Skip not source sources, they do not have file fields.
   for (; Vf.end() == false; ++Vf)
   {
      if (Vf.File().Flagged(pkgCache::Flag::NotSource))
	 continue;
      break;
   }
   
   // Does not really matter here.. we are going to fail out below
   if (Vf.end() != true)
   {     
      // If this fails to get a file name we will bomb out below.
      pkgRecords::Parser &Parse = Recs->Lookup(Vf);
      if (_error->PendingError() == true)
	 return;
            
      // Generate the final file name as: package_version_arch.foo
      StoreFilename = QuoteString(Version.ParentPkg().Name(),"_:") + '_' +
	              QuoteString(Version.VerStr(),"_:") + '_' +
     	              QuoteString(Version.Arch(),"_:.") + 
	              "." + flExtension(Parse.FileName());
   }

   // check if we have one trusted source for the package. if so, switch
   // to "TrustedOnly" mode - but only if not in AllowUnauthenticated mode
   bool const allowUnauth = _config->FindB("APT::Get::AllowUnauthenticated", false);
   bool const debugAuth = _config->FindB("Debug::pkgAcquire::Auth", false);
   bool seenUntrusted = false;
   for (pkgCache::VerFileIterator i = Version.FileList(); i.end() == false; ++i)
   {
      pkgIndexFile *Index;
      if (Sources->FindIndex(i.File(),Index) == false)
         continue;

      if (debugAuth == true)
         std::cerr << "Checking index: " << Index->Describe()
                   << "(Trusted=" << Index->IsTrusted() << ")" << std::endl;

      if (Index->IsTrusted() == true)
      {
         Trusted = true;
	 if (allowUnauth == false)
	    break;
      }
      else
         seenUntrusted = true;
   }

   // "allow-unauthenticated" restores apts old fetching behaviour
   // that means that e.g. unauthenticated file:// uris are higher
   // priority than authenticated http:// uris
   if (allowUnauth == true && seenUntrusted == true)
      Trusted = false;

   // Select a source
   if (QueueNext() == false && _error->PendingError() == false)
      _error->Error(_("Can't find a source to download version '%s' of '%s'"),
		    Version.VerStr(), Version.ParentPkg().FullName(false).c_str());
}
									/*}}}*/
// AcqArchive::QueueNext - Queue the next file source			/*{{{*/
// ---------------------------------------------------------------------
/* This queues the next available file version for download. It checks if
   the archive is already available in the cache and stashs the MD5 for
   checking later. */
bool pkgAcqArchive::QueueNext()
{
   for (; Vf.end() == false; ++Vf)
   {
      pkgCache::PkgFileIterator const PkgF = Vf.File();
      // Ignore not source sources
      if (PkgF.Flagged(pkgCache::Flag::NotSource))
	 continue;

      // Try to cross match against the source list
      pkgIndexFile *Index;
      if (Sources->FindIndex(PkgF, Index) == false)
	    continue;
      LocalSource = PkgF.Flagged(pkgCache::Flag::LocalSource);

      // only try to get a trusted package from another source if that source
      // is also trusted
      if(Trusted && !Index->IsTrusted()) 
	 continue;

      // Grab the text package record
      pkgRecords::Parser &Parse = Recs->Lookup(Vf);
      if (_error->PendingError() == true)
	 return false;

      string PkgFile = Parse.FileName();
      ExpectedHashes = Parse.Hashes();

      if (PkgFile.empty() == true)
	 return _error->Error(_("The package index files are corrupted. No Filename: "
			      "field for package %s."),
			      Version.ParentPkg().Name());

      Desc.URI = Index->ArchiveURI(PkgFile);
      Desc.Description = Index->ArchiveInfo(Version);
      Desc.Owner = this;
      Desc.ShortDesc = Version.ParentPkg().FullName(true);

      // See if we already have the file. (Legacy filenames)
      FileSize = Version->Size;
      string FinalFile = _config->FindDir("Dir::Cache::Archives") + flNotDir(PkgFile);
      struct stat Buf;
      if (stat(FinalFile.c_str(),&Buf) == 0)
      {
	 // Make sure the size matches
	 if ((unsigned long long)Buf.st_size == Version->Size)
	 {
	    Complete = true;
	    Local = true;
	    Status = StatDone;
	    StoreFilename = DestFile = FinalFile;
	    return true;
	 }
	 
	 /* Hmm, we have a file and its size does not match, this means it is
	    an old style mismatched arch */
	 unlink(FinalFile.c_str());
      }

      // Check it again using the new style output filenames
      FinalFile = _config->FindDir("Dir::Cache::Archives") + flNotDir(StoreFilename);
      if (stat(FinalFile.c_str(),&Buf) == 0)
      {
	 // Make sure the size matches
	 if ((unsigned long long)Buf.st_size == Version->Size)
	 {
	    Complete = true;
	    Local = true;
	    Status = StatDone;
	    StoreFilename = DestFile = FinalFile;
	    return true;
	 }
	 
	 /* Hmm, we have a file and its size does not match, this shouldn't
	    happen.. */
	 unlink(FinalFile.c_str());
      }

      DestFile = _config->FindDir("Dir::Cache::Archives") + "partial/" + flNotDir(StoreFilename);
      
      // Check the destination file
      if (stat(DestFile.c_str(),&Buf) == 0)
      {
	 // Hmm, the partial file is too big, erase it
	 if ((unsigned long long)Buf.st_size > Version->Size)
	    unlink(DestFile.c_str());
	 else
	    PartialSize = Buf.st_size;
      }

      // Disables download of archives - useful if no real installation follows,
      // e.g. if we are just interested in proposed installation order
      if (_config->FindB("Debug::pkgAcqArchive::NoQueue", false) == true)
      {
	 Complete = true;
	 Local = true;
	 Status = StatDone;
	 StoreFilename = DestFile = FinalFile;
	 return true;
      }

      // Create the item
      Local = false;
      QueueURI(Desc);

      ++Vf;
      return true;
   }
   return false;
}   
									/*}}}*/
// AcqArchive::Done - Finished fetching					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqArchive::Done(string const &Message, HashStringList const &Hashes,
			 pkgAcquire::MethodConfig const * const Cfg)
{
   Item::Done(Message, Hashes, Cfg);

   // Grab the output filename
   std::string const FileName = LookupTag(Message,"Filename");
   if (DestFile !=  FileName && RealFileExists(DestFile) == false)
   {
      StoreFilename = DestFile = FileName;
      Local = true;
      Complete = true;
      return;
   }

   // Done, move it into position
   string const FinalFile = GetFinalFilename();
   Rename(DestFile,FinalFile);
   StoreFilename = DestFile = FinalFile;
   Complete = true;
}
									/*}}}*/
// AcqArchive::Failed - Failure handler					/*{{{*/
// ---------------------------------------------------------------------
/* Here we try other sources */
void pkgAcqArchive::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Failed(Message,Cnf);

   /* We don't really want to retry on failed media swaps, this prevents
      that. An interesting observation is that permanent failures are not
      recorded. */
   if (Cnf->Removable == true &&
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
   {
      // Vf = Version.FileList();
      while (Vf.end() == false) ++Vf;
      StoreFilename = string();
      return;
   }

   Status = StatIdle;
   if (QueueNext() == false)
   {
      // This is the retry counter
      if (Retries != 0 &&
	  Cnf->LocalOnly == false &&
	  StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
      {
	 Retries--;
	 Vf = Version.FileList();
	 if (QueueNext() == true)
	    return;
      }

      StoreFilename = string();
      Status = StatError;
   }
}
									/*}}}*/
APT_PURE bool pkgAcqArchive::IsTrusted() const				/*{{{*/
{
   return Trusted;
}
									/*}}}*/
void pkgAcqArchive::Finished()						/*{{{*/
{
   if (Status == pkgAcquire::Item::StatDone &&
       Complete == true)
      return;
   StoreFilename = string();
}
									/*}}}*/
std::string pkgAcqArchive::DescURI() const				/*{{{*/
{
   return Desc.URI;
}
									/*}}}*/
std::string pkgAcqArchive::ShortDesc() const				/*{{{*/
{
   return Desc.ShortDesc;
}
									/*}}}*/
pkgAcqArchive::~pkgAcqArchive() {}

// AcqChangelog::pkgAcqChangelog - Constructors				/*{{{*/
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::VerIterator const &Ver,
      std::string const &DestDir, std::string const &DestFilename) :
   pkgAcquire::Item(Owner), d(NULL), SrcName(Ver.SourcePkgName()), SrcVersion(Ver.SourceVerStr())
{
   Desc.URI = URI(Ver);
   Init(DestDir, DestFilename);
}
// some parameters are char* here as they come likely from char* interfaces – which can also return NULL
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::RlsFileIterator const &RlsFile,
      char const * const Component, char const * const SrcName, char const * const SrcVersion,
      const string &DestDir, const string &DestFilename) :
   pkgAcquire::Item(Owner), d(NULL), SrcName(SrcName), SrcVersion(SrcVersion)
{
   Desc.URI = URI(RlsFile, Component, SrcName, SrcVersion);
   Init(DestDir, DestFilename);
}
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner,
      std::string const &URI, char const * const SrcName, char const * const SrcVersion,
      const string &DestDir, const string &DestFilename) :
   pkgAcquire::Item(Owner), d(NULL), SrcName(SrcName), SrcVersion(SrcVersion)
{
   Desc.URI = URI;
   Init(DestDir, DestFilename);
}
void pkgAcqChangelog::Init(std::string const &DestDir, std::string const &DestFilename)
{
   if (Desc.URI.empty())
   {
      Status = StatError;
      // TRANSLATOR: %s=%s is sourcename=sourceversion, e.g. apt=1.1
      strprintf(ErrorText, _("Changelog unavailable for %s=%s"), SrcName.c_str(), SrcVersion.c_str());
      // Let the error message print something sensible rather than "Failed to fetch /"
      if (DestFilename.empty())
	 DestFile = SrcName + ".changelog";
      else
	 DestFile = DestFilename;
      Desc.URI = "changelog:/" + DestFile;
      return;
   }

   if (DestDir.empty())
   {
      std::string const systemTemp = GetTempDir();
      char tmpname[100];
      snprintf(tmpname, sizeof(tmpname), "%s/apt-changelog-XXXXXX", systemTemp.c_str());
      if (NULL == mkdtemp(tmpname))
      {
	 _error->Errno("mkdtemp", "mkdtemp failed in changelog acquire of %s %s", SrcName.c_str(), SrcVersion.c_str());
	 Status = StatError;
	 return;
      }
      DestFile = TemporaryDirectory = tmpname;
   }
   else
      DestFile = DestDir;

   if (DestFilename.empty())
      DestFile = flCombine(DestFile, SrcName + ".changelog");
   else
      DestFile = flCombine(DestFile, DestFilename);

   Desc.ShortDesc = "Changelog";
   strprintf(Desc.Description, "%s %s %s Changelog", URI::SiteOnly(Desc.URI).c_str(), SrcName.c_str(), SrcVersion.c_str());
   Desc.Owner = this;
   QueueURI(Desc);
}
									/*}}}*/
std::string pkgAcqChangelog::URI(pkgCache::VerIterator const &Ver)	/*{{{*/
{
   char const * const SrcName = Ver.SourcePkgName();
   char const * const SrcVersion = Ver.SourceVerStr();
   pkgCache::PkgFileIterator PkgFile;
   // find the first source for this version which promises a changelog
   for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; ++VF)
   {
      pkgCache::PkgFileIterator const PF = VF.File();
      if (PF.Flagged(pkgCache::Flag::NotSource) || PF->Release == 0)
	 continue;
      PkgFile = PF;
      pkgCache::RlsFileIterator const RF = PF.ReleaseFile();
      std::string const uri = URI(RF, PF.Component(), SrcName, SrcVersion);
      if (uri.empty())
	 continue;
      return uri;
   }
   return "";
}
std::string pkgAcqChangelog::URITemplate(pkgCache::RlsFileIterator const &Rls)
{
   if (Rls.end() == true || (Rls->Label == 0 && Rls->Origin == 0))
      return "";
   std::string const serverConfig = "Acquire::Changelogs::URI";
   std::string server;
#define APT_EMPTY_SERVER \
   if (server.empty() == false) \
   { \
      if (server != "no") \
	 return server; \
      return ""; \
   }
#define APT_CHECK_SERVER(X, Y) \
   if (Rls->X != 0) \
   { \
      std::string const specialServerConfig = serverConfig + "::" + Y + #X + "::" + Rls.X(); \
      server = _config->Find(specialServerConfig); \
      APT_EMPTY_SERVER \
   }
   // this way e.g. Debian-Security can fallback to Debian
   APT_CHECK_SERVER(Label, "Override::")
   APT_CHECK_SERVER(Origin, "Override::")

   if (RealFileExists(Rls.FileName()))
   {
      _error->PushToStack();
      FileFd rf;
      /* This can be costly. A caller wanting to get millions of URIs might
	 want to do this on its own once and use Override settings.
	 We don't do this here as Origin/Label are not as unique as they
	 should be so this could produce request order-dependent anomalies */
      if (OpenMaybeClearSignedFile(Rls.FileName(), rf) == true)
      {
	 pkgTagFile TagFile(&rf, rf.Size());
	 pkgTagSection Section;
	 if (TagFile.Step(Section) == true)
	    server = Section.FindS("Changelogs");
      }
      _error->RevertToStack();
      APT_EMPTY_SERVER
   }

   APT_CHECK_SERVER(Label, "")
   APT_CHECK_SERVER(Origin, "")
#undef APT_CHECK_SERVER
#undef APT_EMPTY_SERVER
   return "";
}
std::string pkgAcqChangelog::URI(pkgCache::RlsFileIterator const &Rls,
	 char const * const Component, char const * const SrcName,
	 char const * const SrcVersion)
{
   return URI(URITemplate(Rls), Component, SrcName, SrcVersion);
}
std::string pkgAcqChangelog::URI(std::string const &Template,
	 char const * const Component, char const * const SrcName,
	 char const * const SrcVersion)
{
   if (Template.find("CHANGEPATH") == std::string::npos)
      return "";

   // the path is: COMPONENT/SRC/SRCNAME/SRCNAME_SRCVER, e.g. main/a/apt/1.1 or contrib/liba/libapt/2.0
   std::string Src = SrcName;
   std::string path = APT::String::Startswith(SrcName, "lib") ? Src.substr(0, 4) : Src.substr(0,1);
   path.append("/").append(Src).append("/");
   path.append(Src).append("_").append(StripEpoch(SrcVersion));
   // we omit component for releases without one (= flat-style repositories)
   if (Component != NULL && strlen(Component) != 0)
      path = std::string(Component) + "/" + path;

   return SubstVar(Template, "CHANGEPATH", path);
}
									/*}}}*/
// AcqChangelog::Failed - Failure handler				/*{{{*/
void pkgAcqChangelog::Failed(string const &Message, pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Failed(Message,Cnf);

   std::string errText;
   // TRANSLATOR: %s=%s is sourcename=sourceversion, e.g. apt=1.1
   strprintf(errText, _("Changelog unavailable for %s=%s"), SrcName.c_str(), SrcVersion.c_str());

   // Error is probably something techy like 404 Not Found
   if (ErrorText.empty())
      ErrorText = errText;
   else
      ErrorText = errText + " (" + ErrorText + ")";
   return;
}
									/*}}}*/
// AcqChangelog::Done - Item downloaded OK				/*{{{*/
void pkgAcqChangelog::Done(string const &Message,HashStringList const &CalcHashes,
		      pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Done(Message,CalcHashes,Cnf);

   Complete = true;
}
									/*}}}*/
pkgAcqChangelog::~pkgAcqChangelog()					/*{{{*/
{
   if (TemporaryDirectory.empty() == false)
   {
      unlink(DestFile.c_str());
      rmdir(TemporaryDirectory.c_str());
   }
}
									/*}}}*/

// AcqFile::pkgAcqFile - Constructor					/*{{{*/
pkgAcqFile::pkgAcqFile(pkgAcquire * const Owner,string const &URI, HashStringList const &Hashes,
		       unsigned long long const Size,string const &Dsc,string const &ShortDesc,
		       const string &DestDir, const string &DestFilename,
                       bool const IsIndexFile) :
                       Item(Owner), d(NULL), IsIndexFile(IsIndexFile), ExpectedHashes(Hashes)
{
   Retries = _config->FindI("Acquire::Retries",0);

   if(!DestFilename.empty())
      DestFile = DestFilename;
   else if(!DestDir.empty())
      DestFile = DestDir + "/" + flNotDir(URI);
   else
      DestFile = flNotDir(URI);

   // Create the item
   Desc.URI = URI;
   Desc.Description = Dsc;
   Desc.Owner = this;

   // Set the short description to the archive component
   Desc.ShortDesc = ShortDesc;

   // Get the transfer sizes
   FileSize = Size;
   struct stat Buf;
   if (stat(DestFile.c_str(),&Buf) == 0)
   {
      // Hmm, the partial file is too big, erase it
      if ((Size > 0) && (unsigned long long)Buf.st_size > Size)
	 unlink(DestFile.c_str());
      else
	 PartialSize = Buf.st_size;
   }

   QueueURI(Desc);
}
									/*}}}*/
// AcqFile::Done - Item downloaded OK					/*{{{*/
void pkgAcqFile::Done(string const &Message,HashStringList const &CalcHashes,
		      pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Done(Message,CalcHashes,Cnf);

   std::string const FileName = LookupTag(Message,"Filename");
   Complete = true;

   // The files timestamp matches
   if (StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      return;

   // We have to copy it into place
   if (RealFileExists(DestFile.c_str()) == false)
   {
      Local = true;
      if (_config->FindB("Acquire::Source-Symlinks",true) == false ||
	  Cnf->Removable == true)
      {
	 Desc.URI = "copy:" + FileName;
	 QueueURI(Desc);
	 return;
      }

      // Erase the file if it is a symlink so we can overwrite it
      struct stat St;
      if (lstat(DestFile.c_str(),&St) == 0)
      {
	 if (S_ISLNK(St.st_mode) != 0)
	    unlink(DestFile.c_str());
      }

      // Symlink the file
      if (symlink(FileName.c_str(),DestFile.c_str()) != 0)
      {
	 _error->PushToStack();
	 _error->Errno("pkgAcqFile::Done", "Symlinking file %s failed", DestFile.c_str());
	 std::stringstream msg;
	 _error->DumpErrors(msg);
	 _error->RevertToStack();
	 ErrorText = msg.str();
	 Status = StatError;
	 Complete = false;
      }
   }
}
									/*}}}*/
// AcqFile::Failed - Failure handler					/*{{{*/
// ---------------------------------------------------------------------
/* Here we try other sources */
void pkgAcqFile::Failed(string const &Message, pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Failed(Message,Cnf);

   // This is the retry counter
   if (Retries != 0 &&
       Cnf->LocalOnly == false &&
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
   {
      --Retries;
      QueueURI(Desc);
      Status = StatIdle;
      return;
   }

}
									/*}}}*/
string pkgAcqFile::Custom600Headers() const				/*{{{*/
{
   if (IsIndexFile)
      return "\nIndex-File: true";
   return "";
}
									/*}}}*/
pkgAcqFile::~pkgAcqFile() {}
