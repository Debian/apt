// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <ctime>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

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
static std::string GetKeepCompressedFileName(std::string file, IndexTarget const &Target)/*{{{*/
{
   if (Target.KeepCompressed == false)
      return file;

   std::string const KeepCompressedAs = Target.Option(IndexTarget::KEEPCOMPRESSEDAS);
   if (KeepCompressedAs.empty() == false)
   {
      std::string const ext = KeepCompressedAs.substr(0, KeepCompressedAs.find(' '));
      if (ext != "uncompressed")
	 file.append(".").append(ext);
   }
   return file;
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
static std::string GetExistingFilename(std::string const &File)		/*{{{*/
{
   if (RealFileExists(File))
      return File;
   for (auto const &type : APT::Configuration::getCompressorExtensions())
   {
      std::string const Final = File + type;
      if (RealFileExists(Final))
	 return Final;
   }
   return "";
}
									/*}}}*/
static std::string GetDiffIndexFileName(std::string const &Name)	/*{{{*/
{
   return Name + ".diff/Index";
}
									/*}}}*/
static std::string GetDiffIndexURI(IndexTarget const &Target)		/*{{{*/
{
   return Target.URI + ".diff/Index";
}
									/*}}}*/

static void ReportMirrorFailureToCentral(pkgAcquire::Item const &I, std::string const &FailCode, std::string const &Details)/*{{{*/
{
   // we only act if a mirror was used at all
   if(I.UsedMirror.empty())
      return;
#if 0
   std::cerr << "\nReportMirrorFailure: "
	     << UsedMirror
	     << " Uri: " << DescURI()
	     << " FailCode: "
	     << FailCode << std::endl;
#endif
   string const report = _config->Find("Methods::Mirror::ProblemReporting",
				 LIBEXEC_DIR "/apt-report-mirror-failure");
   if(!FileExists(report))
      return;

   std::vector<char const*> const Args = {
      report.c_str(),
      I.UsedMirror.c_str(),
      I.DescURI().c_str(),
      FailCode.c_str(),
      Details.c_str(),
      NULL
   };

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
      _error->Warning("Couldn't report problem to '%s'", report.c_str());
}
									/*}}}*/

static APT_NONNULL(2) bool MessageInsecureRepository(bool const isError, char const * const msg, std::string const &repo)/*{{{*/
{
   std::string m;
   strprintf(m, msg, repo.c_str());
   if (isError)
   {
      _error->Error("%s", m.c_str());
      _error->Notice("%s", _("Updating from such a repository can't be done securely, and is therefore disabled by default."));
   }
   else
   {
      _error->Warning("%s", m.c_str());
      _error->Notice("%s", _("Data from such a repository can't be authenticated and is therefore potentially dangerous to use."));
   }
   _error->Notice("%s", _("See apt-secure(8) manpage for repository creation and user configuration details."));
   return false;
}
									/*}}}*/
// AllowInsecureRepositories						/*{{{*/
enum class InsecureType { UNSIGNED, WEAK, NORELEASE };
static bool TargetIsAllowedToBe(IndexTarget const &Target, InsecureType const type)
{
   if (_config->FindB("Acquire::AllowInsecureRepositories"))
      return true;

   if (Target.OptionBool(IndexTarget::ALLOW_INSECURE))
      return true;

   switch (type)
   {
      case InsecureType::UNSIGNED: break;
      case InsecureType::NORELEASE: break;
      case InsecureType::WEAK:
	 if (_config->FindB("Acquire::AllowWeakRepositories"))
	    return true;
	 if (Target.OptionBool(IndexTarget::ALLOW_WEAK))
	    return true;
	 break;
   }
   return false;
}
static bool APT_NONNULL(3, 4, 5) AllowInsecureRepositories(InsecureType const msg, std::string const &repo,
      metaIndex const * const MetaIndexParser, pkgAcqMetaClearSig * const TransactionManager, pkgAcquire::Item * const I)
{
   // we skip weak downgrades as its unlikely that a repository gets really weaker –
   // its more realistic that apt got pickier in a newer version
   if (msg != InsecureType::WEAK)
   {
      std::string const FinalInRelease = TransactionManager->GetFinalFilename();
      std::string const FinalReleasegpg = FinalInRelease.substr(0, FinalInRelease.length() - strlen("InRelease")) + "Release.gpg";
      if (RealFileExists(FinalReleasegpg) || RealFileExists(FinalInRelease))
      {
	 char const * msgstr = nullptr;
	 switch (msg)
	 {
	    case InsecureType::UNSIGNED: msgstr = _("The repository '%s' is no longer signed."); break;
	    case InsecureType::NORELEASE: msgstr = _("The repository '%s' no longer has a Release file."); break;
	    case InsecureType::WEAK: /* unreachable */ break;
	 }
	 if (_config->FindB("Acquire::AllowDowngradeToInsecureRepositories") ||
	       TransactionManager->Target.OptionBool(IndexTarget::ALLOW_DOWNGRADE_TO_INSECURE))
	 {
	    // meh, the users wants to take risks (we still mark the packages
	    // from this repository as unauthenticated)
	    _error->Warning(msgstr, repo.c_str());
	    _error->Warning(_("This is normally not allowed, but the option "
		     "Acquire::AllowDowngradeToInsecureRepositories was "
		     "given to override it."));
	 } else {
	    MessageInsecureRepository(true, msgstr, repo);
	    TransactionManager->AbortTransaction();
	    I->Status = pkgAcquire::Item::StatError;
	    return false;
	 }
      }
   }

   if(MetaIndexParser->GetTrusted() == metaIndex::TRI_YES)
      return true;

   char const * msgstr = nullptr;
   switch (msg)
   {
      case InsecureType::UNSIGNED: msgstr = _("The repository '%s' is not signed."); break;
      case InsecureType::NORELEASE: msgstr = _("The repository '%s' does not have a Release file."); break;
      case InsecureType::WEAK: msgstr = _("The repository '%s' provides only weak security information."); break;
   }

   if (TargetIsAllowedToBe(TransactionManager->Target, msg) == true)
   {
      MessageInsecureRepository(false, msgstr, repo);
      return true;
   }

   MessageInsecureRepository(true, msgstr, repo);
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

class pkgAcquire::Item::Private /*{{{*/
{
public:
   struct AlternateURI
   {
      std::string URI;
      std::unordered_map<std::string, std::string> changefields;
      AlternateURI(std::string &&u, decltype(changefields) &&cf) : URI(u), changefields(cf) {}
   };
   std::list<AlternateURI> AlternativeURIs;
   std::vector<std::string> BadAlternativeSites;
   std::vector<std::string> PastRedirections;
   std::unordered_map<std::string, std::string> CustomFields;
   time_point FetchAfter = {};

   Private()
   {
   }
};
									/*}}}*/

// all ::HashesRequired and ::GetExpectedHashes implementations		/*{{{*/
/* ::GetExpectedHashes is abstract and has to be implemented by all subclasses.
   It is best to implement it as broadly as possible, while ::HashesRequired defaults
   to true and should be as restrictive as possible for false cases. Note that if
   a hash is returned by ::GetExpectedHashes it must match. Only if it doesn't
   ::HashesRequired is called to evaluate if its okay to have no hashes. */
APT_PURE bool pkgAcqTransactionItem::HashesRequired() const
{
   /* signed repositories obviously have a parser and good hashes.
      unsigned repositories, too, as even if we can't trust them for security,
      we can at least trust them for integrity of the download itself.
      Only repositories without a Release file can (obviously) not have
      hashes – and they are very uncommon and strongly discouraged */
   if (TransactionManager->MetaIndexParser->GetLoadedSuccessfully() != metaIndex::TRI_YES)
      return false;
   if (TargetIsAllowedToBe(Target, InsecureType::WEAK))
   {
      /* If we allow weak hashes, we check that we have some (weak) and then
         declare hashes not needed. That will tip us in the right direction
	 as if hashes exist, they will be used, even if not required */
      auto const hsl = GetExpectedHashes();
      if (hsl.usable())
	 return true;
      if (hsl.empty() == false)
	 return false;
   }
   return true;
}
HashStringList pkgAcqTransactionItem::GetExpectedHashes() const
{
   return GetExpectedHashesFor(GetMetaKey());
}

APT_PURE bool pkgAcqMetaBase::HashesRequired() const
{
   // Release and co have no hashes 'by design'.
   return false;
}
HashStringList pkgAcqMetaBase::GetExpectedHashes() const
{
   return HashStringList();
}

APT_PURE bool pkgAcqIndexDiffs::HashesRequired() const
{
   /* We can't check hashes of rred result as we don't know what the
      hash of the file will be. We just know the hash of the patch(es),
      the hash of the file they will apply on and the hash of the resulting
      file. */
   if (State == StateFetchDiff)
      return true;
   return false;
}
HashStringList pkgAcqIndexDiffs::GetExpectedHashes() const
{
   if (State == StateFetchDiff)
      return available_patches[0].download_hashes;
   return HashStringList();
}

APT_PURE bool pkgAcqIndexMergeDiffs::HashesRequired() const
{
   /* @see #pkgAcqIndexDiffs::HashesRequired, with the difference that
      we can check the rred result after all patches are applied as
      we know the expected result rather than potentially apply more patches */
   if (State == StateFetchDiff)
      return true;
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

APT_PURE bool pkgAcqArchive::HashesRequired() const
{
   return LocalSource == false;
}
HashStringList pkgAcqArchive::GetExpectedHashes() const
{
   // figured out while parsing the records
   return ExpectedHashes;
}

APT_PURE bool pkgAcqFile::HashesRequired() const
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
   if (TransactionManager->State != TransactionStarted)
   {
      if (_config->FindB("Debug::Acquire::Transaction", false))
	 std::clog << "Skip " << Target.URI << " as transaction was already dealt with!" << std::endl;
      return false;
   }
   std::string const FinalFile = GetFinalFilename();
   if (TransactionManager->IMSHit == true && FileExists(FinalFile) == true)
   {
      PartialFile = DestFile = FinalFile;
      Status = StatDone;
      return false;
   }
   // this ensures we rewrite only once and only the first step
   auto const OldBaseURI = Target.Option(IndexTarget::BASE_URI);
   if (OldBaseURI.empty() || APT::String::Startswith(Item.URI, OldBaseURI) == false)
      return pkgAcquire::Item::QueueURI(Item);
   // the given URI is our last resort
   PushAlternativeURI(std::string(Item.URI), {}, false);
   // If we got the InRelease file via a mirror, pick all indexes directly from this mirror, too
   std::string SameMirrorURI;
   if (TransactionManager->BaseURI.empty() == false && TransactionManager->UsedMirror.empty() == false &&
       URI::SiteOnly(Item.URI) != URI::SiteOnly(TransactionManager->BaseURI))
   {
      auto ExtraPath = Item.URI.substr(OldBaseURI.length());
      auto newURI = flCombine(TransactionManager->BaseURI, std::move(ExtraPath));
      if (IsGoodAlternativeURI(newURI))
      {
	 SameMirrorURI = std::move(newURI);
	 PushAlternativeURI(std::string(SameMirrorURI), {}, false);
      }
   }
   // add URI and by-hash based on it
   if (AcquireByHash())
   {
      // if we use the mirror transport, ask it for by-hash uris
      // we need to stick to the same mirror only for non-unique filenames
      auto const sameMirrorException = [&]() {
	 if (Item.URI.find("mirror") == std::string::npos)
	    return false;
	 ::URI uri(Item.URI);
	 return uri.Access == "mirror" || APT::String::Startswith(uri.Access, "mirror+") ||
	    APT::String::Endswith(uri.Access, "+mirror") || uri.Access.find("+mirror+") != std::string::npos;
      }();
      if (sameMirrorException)
	 SameMirrorURI.clear();
      // now add the actual by-hash uris
      auto const Expected = GetExpectedHashes();
      auto const TargetHash = Expected.find(nullptr);
      auto const PushByHashURI = [&](std::string U) {
	 if (unlikely(TargetHash == nullptr))
	    return false;
	 auto const trailing_slash = U.find_last_of("/");
	 if (unlikely(trailing_slash == std::string::npos))
	    return false;
	 auto byhashSuffix = "/by-hash/" + TargetHash->HashType() + "/" + TargetHash->HashValue();
	 U.replace(trailing_slash, U.length() - trailing_slash, std::move(byhashSuffix));
	 PushAlternativeURI(std::move(U), {}, false);
	 return true;
      };
      PushByHashURI(Item.URI);
      if (SameMirrorURI.empty() == false && PushByHashURI(SameMirrorURI) == false)
	 SameMirrorURI.clear();
   }
   // the last URI added is the first one tried
   if (unlikely(PopAlternativeURI(Item.URI) == false))
      return false;
   if (SameMirrorURI.empty() == false)
   {
      UsedMirror = TransactionManager->UsedMirror;
      if (Item.Description.find(" ") != string::npos)
	 Item.Description.replace(0, Item.Description.find(" "), UsedMirror);
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
   // Beware: Desc.URI is modified by redirections
   return GetFinalFileNameFromURI(Desc.URI);
}
std::string pkgAcqDiffIndex::GetFinalFilename() const
{
   std::string const FinalFile = GetFinalFileNameFromURI(GetDiffIndexURI(Target));
   // we don't want recompress, so lets keep whatever we got
   if (CurrentCompressionExtension == "uncompressed")
      return FinalFile;
   return FinalFile + "." + CurrentCompressionExtension;
}
std::string pkgAcqIndex::GetFinalFilename() const
{
   std::string const FinalFile = GetFinalFileNameFromURI(Target.URI);
   return GetKeepCompressedFileName(FinalFile, Target);
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
   auto const metakey = GetDiffIndexFileName(Target.MetaKey);
   if (CurrentCompressionExtension == "uncompressed")
      return metakey;
   return metakey + "." + CurrentCompressionExtension;
}
									/*}}}*/
//pkgAcqTransactionItem::TransactionState and specialisations for child classes	/*{{{*/
bool pkgAcqTransactionItem::TransactionState(TransactionStates const state)
{
   bool const Debug = _config->FindB("Debug::Acquire::Transaction", false);
   switch(state)
   {
      case TransactionStarted: _error->Fatal("Item %s changed to invalid transaction start state!", Target.URI.c_str()); break;
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
	 if(PartialFile.empty() == false)
	 {
	    bool sameFile = (PartialFile == DestFile);
	    // we use symlinks on IMS-Hit to avoid copies
	    if (RealFileExists(DestFile))
	    {
	       struct stat Buf;
	       if (lstat(PartialFile.c_str(), &Buf) != -1)
	       {
		  if (S_ISLNK(Buf.st_mode) && Buf.st_size > 0)
		  {
		     char partial[Buf.st_size + 1];
		     ssize_t const sp = readlink(PartialFile.c_str(), partial, Buf.st_size);
		     if (sp == -1)
			_error->Errno("pkgAcqTransactionItem::TransactionState-sp", _("Failed to readlink %s"), PartialFile.c_str());
		     else
		     {
			partial[sp] = '\0';
			sameFile = (DestFile == partial);
		     }
		  }
	       }
	       else
		  _error->Errno("pkgAcqTransactionItem::TransactionState-stat", _("Failed to stat %s"), PartialFile.c_str());
	    }
	    if (sameFile == false)
	    {
	       // ensure that even without lists-cleanup all compressions are nuked
	       std::string FinalFile = GetFinalFileNameFromURI(Target.URI);
	       if (FileExists(FinalFile))
	       {
		  if(Debug == true)
		     std::clog << "rm " << FinalFile << " # " << DescURI() << std::endl;
		  if (RemoveFile("TransactionStates-Cleanup", FinalFile) == false)
		     return false;
	       }
	       for (auto const &ext: APT::Configuration::getCompressorExtensions())
	       {
		  auto const Final = FinalFile + ext;
		  if (FileExists(Final))
		  {
		     if(Debug == true)
			std::clog << "rm " << Final << " # " << DescURI() << std::endl;
		     if (RemoveFile("TransactionStates-Cleanup", Final) == false)
			return false;
		  }
	       }
	       if(Debug == true)
		  std::clog << "mv " << PartialFile << " -> "<< DestFile << " # " << DescURI() << std::endl;
	       if (Rename(PartialFile, DestFile) == false)
		  return false;
	    }
	    else if(Debug == true)
	       std::clog << "keep " << PartialFile << " # " << DescURI() << std::endl;

	 } else {
	    if(Debug == true)
	       std::clog << "rm " << DestFile << " # " << DescURI() << std::endl;
	    if (RemoveFile("TransItem::TransactionCommit", DestFile) == false)
	       return false;
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
      case TransactionStarted: _error->Fatal("AcqIndex %s changed to invalid transaction start state!", Target.URI.c_str()); break;
      case TransactionAbort:
	 if (Stage == STAGE_DECOMPRESS_AND_VERIFY)
	 {
	    // keep the compressed file, but drop the decompressed
	    EraseFileName.clear();
	    if (PartialFile.empty() == false && flExtension(PartialFile) != CurrentCompressionExtension)
	       RemoveFile("TransactionAbort", PartialFile);
	 }
	 break;
      case TransactionCommit:
	 if (EraseFileName.empty() == false)
	    RemoveFile("AcqIndex::TransactionCommit", EraseFileName);
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
      case TransactionStarted: _error->Fatal("Item %s changed to invalid transaction start state!", Target.URI.c_str()); break;
      case TransactionCommit:
	 break;
      case TransactionAbort:
	 std::string const Partial = GetPartialFileNameFromURI(Target.URI);
	 RemoveFile("TransactionAbort", Partial);
	 break;
   }

   return true;
}
									/*}}}*/
// pkgAcqTransactionItem::AcquireByHash and specialisations for child classes	/*{{{*/
bool pkgAcqTransactionItem::AcquireByHash() const
{
   if (TransactionManager->MetaIndexParser == nullptr)
      return false;
   auto const useByHashConf = Target.Option(IndexTarget::BY_HASH);
   if (useByHashConf == "force")
      return true;
   return StringToBool(useByHashConf) == true && TransactionManager->MetaIndexParser->GetSupportsAcquireByHash();
}
// pdiff patches have a unique name already, no need for by-hash
bool pkgAcqIndexMergeDiffs::AcquireByHash() const
{
   return false;
}
bool pkgAcqIndexDiffs::AcquireByHash() const
{
   return false;
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
   NoActionItem(pkgAcquire * const Owner, IndexTarget const &Target, std::string const &FinalFile) :
      pkgAcquire::Item(Owner), Target(Target)
   {
      Status = StatDone;
      DestFile = FinalFile;
   }
};
									/*}}}*/
class APT_HIDDEN CleanupItem : public pkgAcqTransactionItem		/*{{{*/
/* This class ensures that a file which was configured but isn't downloaded
   for various reasons isn't kept in an old version in the lists directory.
   In a way its the reverse of NoActionItem as it helps with removing files
   even if the lists-cleanup is deactivated. */
{
   public:
   virtual std::string DescURI() const APT_OVERRIDE {return Target.URI;};
   virtual HashStringList GetExpectedHashes()  const APT_OVERRIDE {return HashStringList();};

   CleanupItem(pkgAcquire * const Owner, pkgAcqMetaClearSig * const TransactionManager, IndexTarget const &Target) :
      pkgAcqTransactionItem(Owner, TransactionManager, Target)
   {
      Status = StatDone;
      DestFile = GetFinalFileNameFromURI(Target.URI);
   }
   bool TransactionState(TransactionStates const state) APT_OVERRIDE
   {
      switch (state)
      {
	 case TransactionStarted:
	    break;
	 case TransactionAbort:
	    break;
	 case TransactionCommit:
	    if (_config->FindB("Debug::Acquire::Transaction", false) == true)
	       std::clog << "rm " << DestFile << " # " << DescURI() << std::endl;
	    if (RemoveFile("TransItem::TransactionCommit", DestFile) == false)
	       return false;
	    break;
      }
      return true;
   }
};
									/*}}}*/

// Acquire::Item::Item - Constructor					/*{{{*/
pkgAcquire::Item::Item(pkgAcquire * const owner) :
   FileSize(0), PartialSize(0), ID(0), Complete(false), Local(false),
    QueueCounter(0), ExpectedAdditionalItems(0), Retries(_config->FindI("Acquire::Retries", 3)), Owner(owner), d(new Private())
{
   Owner->Add(this);
   Status = StatIdle;
}
									/*}}}*/
// Acquire::Item::~Item - Destructor					/*{{{*/
pkgAcquire::Item::~Item()
{
   Owner->Remove(this);
   delete d;
}
									/*}}}*/
std::string pkgAcquire::Item::Custom600Headers() const			/*{{{*/
{
   std::ostringstream header;
   for (auto const &f : d->CustomFields)
      if (f.second.empty() == false)
	 header << '\n'
		<< f.first << ": " << f.second;
   return header.str();
}
									/*}}}*/
std::unordered_map<std::string, std::string> &pkgAcquire::Item::ModifyCustomFields() /*{{{*/
{
   return d->CustomFields;
}
									/*}}}*/
bool pkgAcquire::Item::PopAlternativeURI(std::string &NewURI) /*{{{*/
{
   if (d->AlternativeURIs.empty())
      return false;
   auto const AltUri = d->AlternativeURIs.front();
   d->AlternativeURIs.pop_front();
   NewURI = AltUri.URI;
   auto &CustomFields = ModifyCustomFields();
   for (auto const &f : AltUri.changefields)
   {
      if (f.second.empty())
	 CustomFields.erase(f.first);
      else
	 CustomFields[f.first] = f.second;
   }
   return true;
}
									/*}}}*/
bool pkgAcquire::Item::IsGoodAlternativeURI(std::string const &AltUri) const/*{{{*/
{
   return std::find(d->PastRedirections.cbegin(), d->PastRedirections.cend(), AltUri) == d->PastRedirections.cend() &&
	 std::find(d->BadAlternativeSites.cbegin(), d->BadAlternativeSites.cend(), URI::SiteOnly(AltUri)) == d->BadAlternativeSites.cend();
}
									/*}}}*/
void pkgAcquire::Item::PushAlternativeURI(std::string &&NewURI, std::unordered_map<std::string, std::string> &&fields, bool const at_the_back) /*{{{*/
{
   if (IsGoodAlternativeURI(NewURI) == false)
      return;
   if (at_the_back)
      d->AlternativeURIs.emplace_back(std::move(NewURI), std::move(fields));
   else
      d->AlternativeURIs.emplace_front(std::move(NewURI), std::move(fields));
}
									/*}}}*/
void pkgAcquire::Item::RemoveAlternativeSite(std::string &&OldSite) /*{{{*/
{
   d->AlternativeURIs.erase(std::remove_if(d->AlternativeURIs.begin(), d->AlternativeURIs.end(),
					   [&](decltype(*d->AlternativeURIs.cbegin()) AltUri) {
					      return URI::SiteOnly(AltUri.URI) == OldSite;
					   }),
			    d->AlternativeURIs.end());
   d->BadAlternativeSites.push_back(std::move(OldSite));
}
									/*}}}*/
std::string pkgAcquire::Item::ShortDesc() const				/*{{{*/
{
   return DescURI();
}
									/*}}}*/
void pkgAcquire::Item::Finished()					/*{{{*/
{
}
									/*}}}*/
APT_PURE pkgAcquire * pkgAcquire::Item::GetOwner() const		/*{{{*/
{
   return Owner;
}
									/*}}}*/
APT_PURE pkgAcquire::ItemDesc &pkgAcquire::Item::GetItemDesc()		/*{{{*/
{
   return Desc;
}
									/*}}}*/
APT_PURE bool pkgAcquire::Item::IsTrusted() const			/*{{{*/
{
   return false;
}
									/*}}}*/
// Acquire::Item::Failed - Item failed to download			/*{{{*/
// ---------------------------------------------------------------------
/* We return to an idle state if there are still other queues that could
   fetch this object */
static void formatHashsum(std::ostream &out, HashString const &hs)
{
   auto const type = hs.HashType();
   if (type == "Checksum-FileSize")
      out << " - Filesize";
   else
      out << " - " << type;
   out << ':' << hs.HashValue();
   if (hs.usable() == false)
      out << " [weak]";
   out << std::endl;
}
void pkgAcquire::Item::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)
{
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

   FailMessage(Message);

   if (QueueCounter > 1)
      Status = StatIdle;
}
void pkgAcquire::Item::FailMessage(string const &Message)
{
   string const FailReason = LookupTag(Message, "FailReason");
   enum { MAXIMUM_SIZE_EXCEEDED, HASHSUM_MISMATCH, WEAK_HASHSUMS, REDIRECTION_LOOP, OTHER } failreason = OTHER;
   if ( FailReason == "MaximumSizeExceeded")
      failreason = MAXIMUM_SIZE_EXCEEDED;
   else if ( FailReason == "WeakHashSums")
      failreason = WEAK_HASHSUMS;
   else if (FailReason == "RedirectionLoop")
      failreason = REDIRECTION_LOOP;
   else if (Status == StatAuthError)
      failreason = HASHSUM_MISMATCH;

   if(ErrorText.empty())
   {
      std::ostringstream out;
      switch (failreason)
      {
	 case HASHSUM_MISMATCH:
	    out << _("Hash Sum mismatch") << std::endl;
	    break;
	 case WEAK_HASHSUMS:
	    out << _("Insufficient information available to perform this download securely") << std::endl;
	    break;
	 case REDIRECTION_LOOP:
	    out << "Redirection loop encountered" << std::endl;
	    break;
	 case MAXIMUM_SIZE_EXCEEDED:
	    out << LookupTag(Message, "Message") << std::endl;
	    break;
	 case OTHER:
	    out << LookupTag(Message, "Message");
	    break;
      }

      if (Status == StatAuthError)
      {
	 auto const ExpectedHashes = GetExpectedHashes();
	 if (ExpectedHashes.empty() == false)
	 {
	    out << "Hashes of expected file:" << std::endl;
	    for (auto const &hs: ExpectedHashes)
	       formatHashsum(out, hs);
	 }
	 if (failreason == HASHSUM_MISMATCH)
	 {
	    out << "Hashes of received file:" << std::endl;
	    for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
	    {
	       std::string const tagname = std::string(*type) + "-Hash";
	       std::string const hashsum = LookupTag(Message, tagname.c_str());
	       if (hashsum.empty() == false)
		  formatHashsum(out, HashString(*type, hashsum));
	    }
	 }
	 auto const lastmod = LookupTag(Message, "Last-Modified", "");
	 if (lastmod.empty() == false)
	    out << "Last modification reported: " << lastmod << std::endl;
      }
      ErrorText = out.str();
   }

   switch (failreason)
   {
      case MAXIMUM_SIZE_EXCEEDED: RenameOnError(MaximumSizeExceeded); break;
      case HASHSUM_MISMATCH: RenameOnError(HashSumMismatch); break;
      case WEAK_HASHSUMS: break;
      case REDIRECTION_LOOP: break;
      case OTHER: break;
   }

   if (FailReason.empty() == false)
      ReportMirrorFailureToCentral(*this, FailReason, ErrorText);
   else
      ReportMirrorFailureToCentral(*this, ErrorText, ErrorText);
}
									/*}}}*/
// Acquire::Item::Start - Item has begun to download			/*{{{*/
// ---------------------------------------------------------------------
/* Stash status and the file size. Note that setting Complete means
   sub-phases of the acquire process such as decompression are operating */
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
   ErrorText.clear();
   Dequeue();
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
   d->AlternativeURIs.clear();
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
	 break;
      case SizeMismatch:
	 errtext = _("Size mismatch");
	 Status = StatAuthError;
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
}
									/*}}}*/
std::string pkgAcquire::Item::HashSum() const				/*{{{*/
{
   HashStringList const hashes = GetExpectedHashes();
   HashString const * const hs = hashes.find(NULL);
   return hs != NULL ? hs->toStr() : "";
}
									/*}}}*/
void pkgAcquire::Item::FetchAfter(time_point FetchAfter) /*{{{*/
{
   d->FetchAfter = FetchAfter;
}
pkgAcquire::time_point pkgAcquire::Item::FetchAfter()
{
   return d->FetchAfter;
}
									/*}}}*/
bool pkgAcquire::Item::IsRedirectionLoop(std::string const &NewURI)	/*{{{*/
{
   // store can fail due to permission errors and the item will "loop" then
   if (APT::String::Startswith(NewURI, "store:"))
      return false;
   if (d->PastRedirections.empty())
   {
      d->PastRedirections.push_back(NewURI);
      return false;
   }
   auto const LastURI = std::prev(d->PastRedirections.end());
   // redirections to the same file are a way of restarting/resheduling,
   // individual methods will have to make sure that they aren't looping this way
   if (*LastURI == NewURI)
      return false;
   if (std::find(d->PastRedirections.begin(), LastURI, NewURI) != LastURI)
      return true;
   d->PastRedirections.push_back(NewURI);
   return false;
}
									/*}}}*/
int pkgAcquire::Item::Priority()					/*{{{*/
{
   // Stage 0: Files requested by methods
   // - they will usually not end up here, but if they do we make sure
   //   to get them as soon as possible as they are probably blocking
   //   the processing of files by the requesting method
   if (dynamic_cast<pkgAcqAuxFile *>(this) != nullptr)
      return 5000;
   // Stage 1: Meta indices and diff indices
   // - those need to be fetched first to have progress reporting working
   //   for the rest
   if (dynamic_cast<pkgAcqMetaSig*>(this) != nullptr
       || dynamic_cast<pkgAcqMetaBase*>(this) != nullptr
       || dynamic_cast<pkgAcqDiffIndex*>(this) != nullptr)
      return 1000;
   // Stage 2: Diff files
   // - fetch before complete indexes so we can apply the diffs while fetching
   //   larger files.
   if (dynamic_cast<pkgAcqIndexDiffs*>(this) != nullptr ||
       dynamic_cast<pkgAcqIndexMergeDiffs*>(this) != nullptr)
      return 800;

   // Stage 3: The rest - complete index files and other stuff
   return 500;
}
									/*}}}*/

pkgAcqTransactionItem::pkgAcqTransactionItem(pkgAcquire * const Owner,	/*{{{*/
      pkgAcqMetaClearSig * const transactionManager, IndexTarget const &target) :
   pkgAcquire::Item(Owner), d(NULL), Target(target), TransactionManager(transactionManager)
{
   if (TransactionManager != this)
      TransactionManager->Add(this);
   ModifyCustomFields() = {
      {"Target-Site", Target.Option(IndexTarget::SITE)},
      {"Target-Repo-URI", Target.Option(IndexTarget::REPO_URI)},
      {"Target-Base-URI", Target.Option(IndexTarget::BASE_URI)},
      {"Target-Component", Target.Option(IndexTarget::COMPONENT)},
      {"Target-Release", Target.Option(IndexTarget::RELEASE)},
      {"Target-Architecture", Target.Option(IndexTarget::ARCHITECTURE)},
      {"Target-Language", Target.Option(IndexTarget::LANGUAGE)},
      {"Target-Type", "index"},
   };
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

static void LoadLastMetaIndexParser(pkgAcqMetaClearSig * const TransactionManager, std::string const &FinalRelease, std::string const &FinalInRelease)/*{{{*/
{
   if (TransactionManager->IMSHit == true)
      return;
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
									/*}}}*/

// AcqMetaBase - Constructor						/*{{{*/
pkgAcqMetaBase::pkgAcqMetaBase(pkgAcquire * const Owner,
      pkgAcqMetaClearSig * const TransactionManager,
      IndexTarget const &DataTarget)
: pkgAcqTransactionItem(Owner, TransactionManager, DataTarget), d(NULL),
   AuthPass(false), IMSHit(false), State(TransactionStarted)
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

   switch (TransactionManager->State)
   {
      case TransactionStarted: break;
      case TransactionAbort: _error->Fatal("Transaction %s was already aborted and is aborted again", TransactionManager->Target.URI.c_str()); return;
      case TransactionCommit: _error->Fatal("Transaction %s was already aborted and is now committed", TransactionManager->Target.URI.c_str()); return;
   }
   TransactionManager->State = TransactionAbort;
   TransactionManager->ExpectedAdditionalItems = 0;

   // ensure the toplevel is in error state too
   for (std::vector<pkgAcqTransactionItem*>::iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      (*I)->ExpectedAdditionalItems = 0;
      if ((*I)->Status != pkgAcquire::Item::StatFetching)
	 (*I)->Dequeue();
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

   switch (TransactionManager->State)
   {
      case TransactionStarted: break;
      case TransactionAbort: _error->Fatal("Transaction %s was already committed and is now aborted", TransactionManager->Target.URI.c_str()); return;
      case TransactionCommit: _error->Fatal("Transaction %s was already committed and is again committed", TransactionManager->Target.URI.c_str()); return;
   }
   TransactionManager->State = TransactionCommit;

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
/* This method is called from ::Failed handlers. If it returns true,
   no fallback to other files or modi is performed */
bool pkgAcqMetaBase::CheckStopAuthentication(pkgAcquire::Item * const I, const std::string &Message)
{
   string const Final = I->GetFinalFilename();
   std::string const GPGError = LookupTag(Message, "Message");
   if (FileExists(Final))
   {
      I->Status = StatTransientNetworkError;
      _error->Warning(_("An error occurred during the signature verification. "
	       "The repository is not updated and the previous index files will be used. "
	       "GPG error: %s: %s"),
	    Desc.Description.c_str(),
	    GPGError.c_str());
      RunScripts("APT::Update::Auth-Failure");
      return true;
   } else if (LookupTag(Message,"Message").find("NODATA") != string::npos) {
      /* Invalid signature file, reject (LP: #346386) (Closes: #627642) */
      _error->Error(_("GPG error: %s: %s"),
	    Desc.Description.c_str(),
	    GPGError.c_str());
      I->Status = StatAuthError;
      return true;
   } else {
      _error->Warning(_("GPG error: %s: %s"),
	    Desc.Description.c_str(),
	    GPGError.c_str());
   }
   // gpgv method failed
   ReportMirrorFailureToCentral(*this, "GPGFailure", GPGError);
   return false;
}
									/*}}}*/
// AcqMetaBase::Custom600Headers - Get header for AcqMetaBase		/*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaBase::Custom600Headers() const
{
   std::string Header = pkgAcqTransactionItem::Custom600Headers();
   Header.append("\nIndex-File: true");
   std::string MaximumSize;
   strprintf(MaximumSize, "\nMaximum-Size: %i",
             _config->FindI("Acquire::MaxReleaseFileSize", 10*1000*1000));
   Header += MaximumSize;

   string const FinalFile = GetFinalFilename();
   struct stat Buf;
   if (stat(FinalFile.c_str(),&Buf) == 0)
      Header += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime, false);

   return Header;
}
									/*}}}*/
// AcqMetaBase::QueueForSignatureVerify					/*{{{*/
void pkgAcqMetaBase::QueueForSignatureVerify(pkgAcqTransactionItem * const I, std::string const &File, std::string const &Signature)
{
   AuthPass = true;
   I->Desc.URI = "gpgv:" + pkgAcquire::URIEncode(Signature);
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

   // Save the final base URI we got this Release file from
   if (I->UsedMirror.empty() == false && _config->FindB("Acquire::SameMirrorForAllIndexes", true))
   {
      auto InReleasePath = Target.Option(IndexTarget::INRELEASE_PATH);
      if (InReleasePath.empty())
	 InReleasePath = "InRelease";

      if (APT::String::Endswith(I->Desc.URI, InReleasePath))
      {
	 TransactionManager->BaseURI = I->Desc.URI.substr(0, I->Desc.URI.length() - InReleasePath.length());
	 TransactionManager->UsedMirror = I->UsedMirror;
      }
      else if (APT::String::Endswith(I->Desc.URI, "Release"))
      {
	 TransactionManager->BaseURI = I->Desc.URI.substr(0, I->Desc.URI.length() - strlen("Release"));
	 TransactionManager->UsedMirror = I->UsedMirror;
      }
   }

   std::string const FileName = LookupTag(Message,"Filename");
   if (FileName != I->DestFile && RealFileExists(I->DestFile) == false)
   {
      I->Local = true;
      I->Desc.URI = "copy:" + pkgAcquire::URIEncode(FileName);
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
	 RemoveFile("CheckDownloadDone", I->DestFile);
      }
   }

   if(IMSHit == true)
   {
      // for simplicity, the transaction manager is always InRelease
      // even if it doesn't exist.
      TransactionManager->IMSHit = true;
      I->PartialFile = I->DestFile = I->GetFinalFilename();
   }

   // set Item to complete as the remaining work is all local (verify etc)
   I->Complete = true;

   return true;
}
									/*}}}*/
bool pkgAcqMetaBase::CheckAuthDone(string const &Message, pkgAcquire::MethodConfig const *const Cnf) /*{{{*/
{
   /* If we work with a recent version of our gpgv method, we expect that it tells us
      which key(s) have signed the file so stuff like CVE-2018-0501 is harder in the future */
   if (Cnf->Version != "1.0" && LookupTag(Message, "Signed-By").empty())
   {
      std::string errmsg;
      strprintf(errmsg, "Internal Error: Signature on %s seems good, but expected details are missing! (%s)", Target.URI.c_str(), "Signed-By");
      if (ErrorText.empty())
	 ErrorText = errmsg;
      Status = StatAuthError;
      return _error->Error("%s", errmsg.c_str());
   }

   // At this point, the gpgv method has succeeded, so there is a
   // valid signature from a key in the trusted keyring.  We
   // perform additional verification of its contents, and use them
   // to verify the indexes we are about to download
   if (_config->FindB("Debug::pkgAcquire::Auth", false))
      std::cerr << "Signature verification succeeded: " << DestFile << std::endl;

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
      LoadLastMetaIndexParser(TransactionManager, FinalRelease, FinalInRelease);
   }

   bool const GoodAuth = TransactionManager->MetaIndexParser->Load(DestFile, &ErrorText);
   if (GoodAuth == false && AllowInsecureRepositories(InsecureType::WEAK, Target.Description, TransactionManager->MetaIndexParser, TransactionManager, this) == false)
   {
      Status = StatAuthError;
      return false;
   }

   if (!VerifyVendor(Message))
   {
      Status = StatAuthError;
      return false;
   }

   // Download further indexes with verification
   TransactionManager->QueueIndexes(GoodAuth);

   return GoodAuth;
}
									/*}}}*/
void pkgAcqMetaClearSig::QueueIndexes(bool const verify)			/*{{{*/
{
   // at this point the real Items are loaded in the fetcher
   ExpectedAdditionalItems = 0;

   std::unordered_set<std::string> targetsSeen, componentsSeen;
   bool const hasReleaseFile = TransactionManager->MetaIndexParser != NULL;
   bool hasHashes = true;
   auto IndexTargets = TransactionManager->MetaIndexParser->GetIndexTargets();
   if (hasReleaseFile && verify == false)
      hasHashes = std::any_of(IndexTargets.begin(), IndexTargets.end(),
	    [&](IndexTarget const &Target) { return TransactionManager->MetaIndexParser->Exists(Target.MetaKey); });
   if (_config->FindB("Acquire::IndexTargets::Randomized", true) && likely(IndexTargets.empty() == false))
   {
      /* For fallback handling and to have some reasonable progress information
	 we can't randomize everything, but at least the order in the same type
	 can be as we shouldn't be telling the mirrors (and everyone else watching)
	 which is native/foreign arch, specific order of preference of translations, … */
      auto range_start = IndexTargets.begin();
      auto seed = (std::chrono::high_resolution_clock::now().time_since_epoch() /  std::chrono::nanoseconds(1)) ^ getpid();
      std::default_random_engine g(seed);
      do {
	 auto const type = range_start->Option(IndexTarget::CREATED_BY);
	 auto const range_end = std::find_if_not(range_start, IndexTargets.end(),
	       [&type](IndexTarget const &T) { return type == T.Option(IndexTarget::CREATED_BY); });
	 std::shuffle(range_start, range_end, g);
	 range_start = range_end;
      } while (range_start != IndexTargets.end());
   }
   /* Collect all components for which files exist to prevent apt from warning users
      about "hidden" components for which not all files exist like main/debian-installer
      and Translation files */
   if (hasReleaseFile == true)
      for (auto const &Target : IndexTargets)
	 if (TransactionManager->MetaIndexParser->Exists(Target.MetaKey))
	 {
	    auto component = Target.Option(IndexTarget::COMPONENT);
	    if (component.empty() == false)
	       componentsSeen.emplace(std::move(component));
	 }

   for (auto&& Target: IndexTargets)
   {
      // if we have seen a target which is created-by a target this one here is declared a
      // fallback to, we skip acquiring the fallback (but we make sure we clean up)
      if (targetsSeen.find(Target.Option(IndexTarget::FALLBACK_OF)) != targetsSeen.end())
      {
	 targetsSeen.emplace(Target.Option(IndexTarget::CREATED_BY));
	 new CleanupItem(Owner, TransactionManager, Target);
	 continue;
      }
      // all is an implementation detail. Users shouldn't use this as arch
      // We need this support trickery here as e.g. Debian has binary-all files already,
      // but arch:all packages are still in the arch:any files, so we would waste precious
      // download time, bandwidth and diskspace for nothing, BUT Debian doesn't feature all
      // in the set of supported architectures, so we can filter based on this property rather
      // than invent an entirely new flag we would need to carry for all of eternity.
      if (hasReleaseFile && Target.Option(IndexTarget::ARCHITECTURE) == "all")
      {
	 if (TransactionManager->MetaIndexParser->IsArchitectureAllSupportedFor(Target) == false)
	 {
	    new CleanupItem(Owner, TransactionManager, Target);
	    continue;
	 }
      }

      bool trypdiff = Target.OptionBool(IndexTarget::PDIFFS);
      if (hasReleaseFile == true)
      {
	 if (TransactionManager->MetaIndexParser->Exists(Target.MetaKey) == false)
	 {
	    auto const component = Target.Option(IndexTarget::COMPONENT);
	    if (component.empty() == false &&
		componentsSeen.find(component) == std::end(componentsSeen) &&
		TransactionManager->MetaIndexParser->HasSupportForComponent(component) == false)
	    {
	       new CleanupItem(Owner, TransactionManager, Target);
	       _error->Warning(_("Skipping acquire of configured file '%s' as repository '%s' doesn't have the component '%s' (component misspelt in sources.list?)"),
		     Target.MetaKey.c_str(), TransactionManager->Target.Description.c_str(), component.c_str());
	       continue;

	    }

	    // optional targets that we do not have in the Release file are skipped
	    if (hasHashes == true && Target.IsOptional)
	    {
	       new CleanupItem(Owner, TransactionManager, Target);
	       continue;
	    }

	    std::string const &arch = Target.Option(IndexTarget::ARCHITECTURE);
	    if (arch.empty() == false)
	    {
	       if (TransactionManager->MetaIndexParser->IsArchitectureSupported(arch) == false)
	       {
		  new CleanupItem(Owner, TransactionManager, Target);
		  _error->Notice(_("Skipping acquire of configured file '%s' as repository '%s' doesn't support architecture '%s'"),
			Target.MetaKey.c_str(), TransactionManager->Target.Description.c_str(), arch.c_str());
		  continue;
	       }
	       // if the architecture is officially supported but currently no packages for it available,
	       // ignore silently as this is pretty much the same as just shipping an empty file.
	       // if we don't know which architectures are supported, we do NOT ignore it to notify user about this
	       if (hasHashes == true && TransactionManager->MetaIndexParser->IsArchitectureSupported("*undefined*") == false)
	       {
		  new CleanupItem(Owner, TransactionManager, Target);
		  continue;
	       }
	    }

	    if (hasHashes == true)
	    {
	       new CleanupItem(Owner, TransactionManager, Target);
	       _error->Warning(_("Skipping acquire of configured file '%s' as repository '%s' does not seem to provide it (sources.list entry misspelt?)"),
		     Target.MetaKey.c_str(), TransactionManager->Target.Description.c_str());
	       continue;
	    }
	    else
	    {
	       new pkgAcqIndex(Owner, TransactionManager, Target);
	       continue;
	    }
	 }
	 else if (verify)
	 {
	    auto const hashes = GetExpectedHashesFor(Target.MetaKey);
	    if (hashes.empty() == false)
	    {
	       if (hashes.usable() == false && TargetIsAllowedToBe(TransactionManager->Target, InsecureType::WEAK) == false)
	       {
		  new CleanupItem(Owner, TransactionManager, Target);
		  _error->Warning(_("Skipping acquire of configured file '%s' as repository '%s' provides only weak security information for it"),
			Target.MetaKey.c_str(), TransactionManager->Target.Description.c_str());
		  continue;
	       }
	       // empty files are skipped as acquiring the very small compressed files is a waste of time
	       else if (hashes.FileSize() == 0)
	       {
		  new CleanupItem(Owner, TransactionManager, Target);
		  targetsSeen.emplace(Target.Option(IndexTarget::CREATED_BY));
		  continue;
	       }
	    }
	 }

	 // autoselect the compression method
	 std::vector<std::string> types = VectorizeString(Target.Option(IndexTarget::COMPRESSIONTYPES), ' ');
	 types.erase(std::remove_if(types.begin(), types.end(), [&](std::string const &t) {
	    if (t == "uncompressed")
	       return TransactionManager->MetaIndexParser->Exists(Target.MetaKey) == false;
	    std::string const MetaKey = Target.MetaKey + "." + t;
	    return TransactionManager->MetaIndexParser->Exists(MetaKey) == false;
	 }), types.end());
	 if (types.empty() == false)
	 {
	    std::ostringstream os;
	    std::copy(types.begin(), types.end()-1, std::ostream_iterator<std::string>(os, " "));
	    os << *types.rbegin();
	    Target.Options["COMPRESSIONTYPES"] = os.str();
	 }
	 else
	    Target.Options["COMPRESSIONTYPES"].clear();

	 std::string filename = GetExistingFilename(GetFinalFileNameFromURI(Target.URI));
	 if (filename.empty() == false)
	 {
	    // if the Release file is a hit and we have an index it must be the current one
	    if (TransactionManager->IMSHit == true)
	       ;
	    else if (TransactionManager->LastMetaIndexParser != NULL)
	    {
	       // see if the file changed since the last Release file
	       // we use the uncompressed files as we might compress differently compared to the server,
	       // so the hashes might not match, even if they contain the same data.
	       HashStringList const newFile = GetExpectedHashesFromFor(TransactionManager->MetaIndexParser, Target.MetaKey);
	       HashStringList const oldFile = GetExpectedHashesFromFor(TransactionManager->LastMetaIndexParser, Target.MetaKey);
	       if (newFile != oldFile)
		  filename.clear();
	    }
	    else
	       filename.clear();
	 }
	 else
	    trypdiff = false; // no file to patch

	 if (filename.empty() == false)
	 {
	    new NoActionItem(Owner, Target, filename);
	    std::string const idxfilename = GetFinalFileNameFromURI(GetDiffIndexURI(Target));
	    if (FileExists(idxfilename))
	       new NoActionItem(Owner, Target, idxfilename);
	    targetsSeen.emplace(Target.Option(IndexTarget::CREATED_BY));
	    continue;
	 }

	 // check if we have patches available
	 trypdiff &= TransactionManager->MetaIndexParser->Exists(GetDiffIndexFileName(Target.MetaKey));
      }
      else
      {
	 // if we have no file to patch, no point in trying
	 trypdiff &= (GetExistingFilename(GetFinalFileNameFromURI(Target.URI)).empty() == false);
      }

      // no point in patching from local sources
      if (trypdiff)
      {
	 std::string const proto = Target.URI.substr(0, strlen("file:/"));
	 if (proto == "file:/" || proto == "copy:/" || proto == "cdrom:")
	    trypdiff = false;
      }

      // Queue the Index file (Packages, Sources, Translation-$foo, …)
      targetsSeen.emplace(Target.Option(IndexTarget::CREATED_BY));
      if (trypdiff)
         new pkgAcqDiffIndex(Owner, TransactionManager, Target);
      else
         new pkgAcqIndex(Owner, TransactionManager, Target);
   }
}
									/*}}}*/
bool pkgAcqMetaBase::VerifyVendor(string const &)			/*{{{*/
{
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

   if (TransactionManager->MetaIndexParser->GetNotBefore() > 0)
   {
      time_t const invalid_for = TransactionManager->MetaIndexParser->GetNotBefore() - time(nullptr);
      if (invalid_for > 0)
      {
	 std::string errmsg;
	 strprintf(errmsg,
		   // TRANSLATOR: The first %s is the URL of the bad Release file, the second is
		   // the time until the file will be valid - formatted in the same way as in
		   // the download progress display (e.g. 7d 3h 42min 1s)
		   _("Release file for %s is not valid yet (invalid for another %s). "
		     "Updates for this repository will not be applied."),
		   Target.URI.c_str(), TimeToStr(invalid_for).c_str());
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
      RemoveFile("VerifyVendor", DestFile);
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
      std::cerr << "Got Suite: " << TransactionManager->MetaIndexParser->GetSuite() << std::endl;
      std::cerr << "Expecting Dist: " << TransactionManager->MetaIndexParser->GetExpectedDist() << std::endl;
   }

   // One day that might become fatal…
   auto const ExpectedDist = TransactionManager->MetaIndexParser->GetExpectedDist();
   auto const NowCodename = TransactionManager->MetaIndexParser->GetCodename();
   if (TransactionManager->MetaIndexParser->CheckDist(ExpectedDist) == false)
      _error->Warning(_("Conflicting distribution: %s (expected %s but got %s)"),
	    Desc.Description.c_str(), ExpectedDist.c_str(), NowCodename.c_str());

   // changed info potentially breaks user config like pinning
   if (TransactionManager->LastMetaIndexParser != nullptr)
   {
      std::vector<pkgAcquireStatus::ReleaseInfoChange> Changes;
      auto const AllowInfoChange = _config->FindB("Acquire::AllowReleaseInfoChange", false);
      auto const quietInfoChange = _config->FindB("quiet::ReleaseInfoChange", false);
      struct {
	 char const * const Type;
	 bool const Allowed;
	 decltype(&metaIndex::GetOrigin) const Getter;
      } checkers[] = {
	 { "Origin", AllowInfoChange, &metaIndex::GetOrigin },
	 { "Label", AllowInfoChange, &metaIndex::GetLabel },
	 { "Version", true, &metaIndex::GetVersion }, // numbers change all the time, that is okay
	 { "Suite", true, &metaIndex::GetSuite },
	 { "Codename", AllowInfoChange, &metaIndex::GetCodename },
	 { nullptr, false, nullptr }
      };
      auto const CheckReleaseInfo = [&](char const * const Type, bool const AllowChange, decltype(checkers[0].Getter) const Getter) {
	 std::string const Last = (TransactionManager->LastMetaIndexParser->*Getter)();
	 std::string const Now = (TransactionManager->MetaIndexParser->*Getter)();
	 if (Last == Now)
	    return;
	 auto const Allow = _config->FindB(std::string("Acquire::AllowReleaseInfoChange::").append(Type), AllowChange);
	 if (Allow == true && _config->FindB(std::string("quiet::ReleaseInfoChange::").append(Type), quietInfoChange) == true)
	    return;
	 std::string msg;
	 strprintf(msg, _("Repository '%s' changed its '%s' value from '%s' to '%s'"),
	       Desc.Description.c_str(), Type, Last.c_str(), Now.c_str());
	 Changes.push_back({Type, std::move(Last), std::move(Now), std::move(msg), Allow});
      };
      for (short i = 0; checkers[i].Type != nullptr; ++i)
	 CheckReleaseInfo(checkers[i].Type, checkers[i].Allowed, checkers[i].Getter);

      {
	 auto const Last = TransactionManager->LastMetaIndexParser->GetDefaultPin();
	 auto const Now = TransactionManager->MetaIndexParser->GetDefaultPin();
	 if (Last != Now)
	 {
	    auto const Allow = _config->FindB("Acquire::AllowReleaseInfoChange::DefaultPin", AllowInfoChange);
	    if (Allow == false || _config->FindB("quiet::ReleaseInfoChange::DefaultPin", quietInfoChange) == false)
	    {
	       std::string msg;
	       strprintf(msg, _("Repository '%s' changed its default priority for %s from %hi to %hi."),
		     Desc.Description.c_str(), "apt_preferences(5)", Last, Now);
	       Changes.push_back({"DefaultPin", std::to_string(Last), std::to_string(Now), std::move(msg), Allow});
	    }
	 }
      }
      if (Changes.empty() == false)
      {
	 auto const notes = TransactionManager->MetaIndexParser->GetReleaseNotes();
	 if (notes.empty() == false)
	 {
	    std::string msg;
	    // TRANSLATOR: the "this" refers to changes in the repository like a new release or owner change
	    strprintf(msg, _("More information about this can be found online in the Release notes at: %s"), notes.c_str());
	    Changes.push_back({"Release-Notes", "", std::move(notes), std::move(msg), true});
	 }
	 if (std::any_of(Changes.begin(),Changes.end(),[](pkgAcquireStatus::ReleaseInfoChange const &c) { return c.DefaultAction == false; }))
	 {
	    std::string msg;
	    // TRANSLATOR: %s is the name of the manpage in question, e.g. apt-secure(8)
	    strprintf(msg, _("This must be accepted explicitly before updates for "
		     "this repository can be applied. See %s manpage for details."), "apt-secure(8)");
	    Changes.push_back({"Confirmation", "", "", std::move(msg), true});
	 }

      }
      if (Owner->Log == nullptr)
	 return pkgAcquireStatus::ReleaseInfoChangesAsGlobalErrors(std::move(Changes));
      return Owner->Log->ReleaseInfoChanges(TransactionManager->LastMetaIndexParser, TransactionManager->MetaIndexParser, std::move(Changes));
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
      metaIndex * const MetaIndexParser) :
   pkgAcqMetaIndex(Owner, this, ClearsignedTarget, DetachedSigTarget),
   d(NULL), DetachedDataTarget(DetachedDataTarget),
   MetaIndexParser(MetaIndexParser), LastMetaIndexParser(NULL)
{
   // index targets + (worst case:) Release/Release.gpg
   ExpectedAdditionalItems = std::numeric_limits<decltype(ExpectedAdditionalItems)>::max();
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
      Header += "\nSigned-By: " + QuoteString(key, "");

   return Header;
}
									/*}}}*/
void pkgAcqMetaClearSig::Finished()					/*{{{*/
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "Finished: " << DestFile <<std::endl;
   if(TransactionManager->State == TransactionStarted &&
      TransactionManager->TransactionHasError() == false)
      TransactionManager->CommitTransaction();
}
									/*}}}*/
bool pkgAcqMetaClearSig::VerifyDone(std::string const &Message,		/*{{{*/
	 pkgAcquire::MethodConfig const * const Cnf)
{
   if (Item::VerifyDone(Message, Cnf) == false)
      return false;

   if (FileExists(DestFile) && !StartsWithGPGClearTextSignature(DestFile))
      return RenameOnError(NotClearsigned);

   return true;
}
									/*}}}*/
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
   else if (CheckAuthDone(Message, Cnf) == true)
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
   else if (Status != StatAuthError)
   {
      string const FinalFile = GetFinalFileNameFromURI(DetachedDataTarget.URI);
      string const OldFile = GetFinalFilename();
      if (TransactionManager->IMSHit == false)
	 TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
      else if (RealFileExists(OldFile) == false)
	 new NoActionItem(Owner, DetachedDataTarget);
      else
	 TransactionManager->TransactionStageCopy(this, OldFile, FinalFile);
   }
}
									/*}}}*/
void pkgAcqMetaClearSig::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf) /*{{{*/
{
   Item::Failed(Message, Cnf);

   if (AuthPass == false)
   {
      if (Status == StatTransientNetworkError)
      {
	 TransactionManager->AbortTransaction();
	 return;
      }
      auto const failreason = LookupTag(Message, "FailReason");
      auto const httperror = "HttpError";
      if (Status == StatAuthError ||
	  Target.Option(IndexTarget::INRELEASE_PATH).empty() == false || /* do not fallback if InRelease was requested */
	  (strncmp(failreason.c_str(), httperror, strlen(httperror)) == 0 &&
	   failreason != "HttpError404"))
      {
	 // if we expected a ClearTextSignature (InRelease) but got a network
	 // error or got a file, but it wasn't valid, we end up here (see VerifyDone).
	 // As these is usually called by web-portals we do not try Release/Release.gpg
	 // as this is going to fail anyway and instead abort our try (LP#346386)
	 _error->PushToStack();
	 _error->Error(_("Failed to fetch %s  %s"), Target.URI.c_str(), ErrorText.c_str());
	 if (Target.Option(IndexTarget::INRELEASE_PATH).empty() == true && AllowInsecureRepositories(InsecureType::UNSIGNED, Target.Description, TransactionManager->MetaIndexParser, TransactionManager, this) == true)
	    _error->RevertToStack();
	 else
	    return;
      }

      // Queue the 'old' InRelease file for removal if we try Release.gpg
      // as otherwise the file will stay around and gives a false-auth
      // impression (CVE-2012-0214)
      TransactionManager->TransactionStageRemoval(this, GetFinalFilename());
      Status = StatDone;

      new pkgAcqMetaIndex(Owner, TransactionManager, DetachedDataTarget, DetachedSigTarget);
   }
   else
   {
      if(CheckStopAuthentication(this, Message))
         return;

      if(AllowInsecureRepositories(InsecureType::UNSIGNED, Target.Description, TransactionManager->MetaIndexParser, TransactionManager, this) == true)
      {
	 Status = StatDone;

	 /* InRelease files become Release files, otherwise
	  * they would be considered as trusted later on */
	 string const FinalRelease = GetFinalFileNameFromURI(DetachedDataTarget.URI);
	 string const PartialRelease = GetPartialFileNameFromURI(DetachedDataTarget.URI);
	 string const FinalInRelease = GetFinalFilename();
	 Rename(DestFile, PartialRelease);
	 TransactionManager->TransactionStageCopy(this, PartialRelease, FinalRelease);
	 LoadLastMetaIndexParser(TransactionManager, FinalRelease, FinalInRelease);

	 // we parse the indexes here because at this point the user wanted
	 // a repository that may potentially harm him
	 if (TransactionManager->MetaIndexParser->Load(PartialRelease, &ErrorText) == false || VerifyVendor(Message) == false)
	    /* expired Release files are still a problem you need extra force for */;
	 else
	    TransactionManager->QueueIndexes(true);
      }
   }
}
									/*}}}*/

pkgAcqMetaIndex::pkgAcqMetaIndex(pkgAcquire * const Owner,		/*{{{*/
                                 pkgAcqMetaClearSig * const TransactionManager,
				 IndexTarget const &DataTarget,
				 IndexTarget const &DetachedSigTarget) :
   pkgAcqMetaBase(Owner, TransactionManager, DataTarget), d(NULL),
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

   // Rewrite the description URI if INRELEASE_PATH was specified so
   // we download the specified file instead.
   auto InReleasePath = DataTarget.Option(IndexTarget::INRELEASE_PATH);
   if (InReleasePath.empty() == false && APT::String::Endswith(DataTarget.URI, "/InRelease"))
   {
      Desc.URI = DataTarget.URI.substr(0, DataTarget.URI.size() - strlen("InRelease")) + InReleasePath;
   }
   else
   {
      Desc.URI = DataTarget.URI;
   }

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

   // No Release file was present so fall
   // back to queueing Packages files without verification
   // only allow going further if the user explicitly wants it
   if(AllowInsecureRepositories(InsecureType::NORELEASE, Target.Description, TransactionManager->MetaIndexParser, TransactionManager, this) == true)
   {
      // ensure old Release files are removed
      TransactionManager->TransactionStageRemoval(this, GetFinalFilename());

      // queue without any kind of hashsum support
      TransactionManager->QueueIndexes(false);
   }
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
   RemoveFile("pkgAcqMetaSig", DestFile);

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
      Header += "\nSigned-By: " + QuoteString(key, "");
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
   else if (MetaIndex->CheckAuthDone(Message, Cfg) == true)
   {
      auto const Releasegpg = GetFinalFilename();
      auto const Release = MetaIndex->GetFinalFilename();
      // if this is an IMS-Hit on Release ensure we also have the Release.gpg file stored
      // (previously an unknown pubkey) – but only if the Release file exists locally (unlikely
      // event of InRelease removed from the mirror causing fallback but still an IMS-Hit)
      if (TransactionManager->IMSHit == false ||
	    (FileExists(Releasegpg) == false && FileExists(Release) == true))
      {
	 TransactionManager->TransactionStageCopy(this, DestFile, Releasegpg);
	 TransactionManager->TransactionStageCopy(MetaIndex, MetaIndex->DestFile, Release);
      }
   }
   else if (MetaIndex->Status != StatAuthError)
   {
      std::string const FinalFile = MetaIndex->GetFinalFilename();
      if (TransactionManager->IMSHit == false)
	 TransactionManager->TransactionStageCopy(MetaIndex, MetaIndex->DestFile, FinalFile);
      else
	 TransactionManager->TransactionStageCopy(MetaIndex, FinalFile, FinalFile);
   }
}
									/*}}}*/
void pkgAcqMetaSig::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   Item::Failed(Message,Cnf);

   // check if we need to fail at this point
   if (MetaIndex->AuthPass == true && MetaIndex->CheckStopAuthentication(this, Message))
         return;

   // ensures that a Release.gpg file in the lists/ is removed by the transaction
   if (not MetaIndexFileSignature.empty())
   {
      DestFile = MetaIndexFileSignature;
      MetaIndexFileSignature.clear();
   }
   TransactionManager->TransactionStageRemoval(this, DestFile);

   // only allow going further if the user explicitly wants it
   if (AllowInsecureRepositories(InsecureType::UNSIGNED, MetaIndex->Target.Description, TransactionManager->MetaIndexParser, TransactionManager, this) == true)
   {
      string const FinalRelease = MetaIndex->GetFinalFilename();
      string const FinalInRelease = TransactionManager->GetFinalFilename();
      LoadLastMetaIndexParser(TransactionManager, FinalRelease, FinalInRelease);

      // we parse the indexes here because at this point the user wanted
      // a repository that may potentially harm him
      bool const GoodLoad = TransactionManager->MetaIndexParser->Load(MetaIndex->DestFile, &ErrorText);
      if (MetaIndex->VerifyVendor(Message) == false)
	 /* expired Release files are still a problem you need extra force for */;
      else
	 TransactionManager->QueueIndexes(GoodLoad);

      TransactionManager->TransactionStageCopy(MetaIndex, MetaIndex->DestFile, FinalRelease);
   }
   else if (TransactionManager->IMSHit == false)
      Rename(MetaIndex->DestFile, MetaIndex->DestFile + ".FAILED");

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
void pkgAcqBaseIndex::Failed(std::string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   pkgAcquire::Item::Failed(Message, Cnf);
   if (Status != StatAuthError)
      return;

   ErrorText.append("Release file created at: ");
   auto const timespec = TransactionManager->MetaIndexParser->GetDate();
   if (timespec == 0)
      ErrorText.append("<unknown>");
   else
      ErrorText.append(TimeRFC1123(timespec, true));
   ErrorText.append("\n");
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
   : pkgAcqIndex(Owner, TransactionManager, Target, true), d(NULL), diffs(NULL)
{
   // FIXME: Magic number as an upper bound on pdiffs we will reasonably acquire
   ExpectedAdditionalItems = 40;
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   CompressionExtensions.clear();
   {
      std::vector<std::string> types = APT::Configuration::getCompressionTypes();
      if (types.empty() == false)
      {
	 std::ostringstream os;
	 std::copy_if(types.begin(), types.end()-1, std::ostream_iterator<std::string>(os, " "), [&](std::string const type) {
	       if (type == "uncompressed")
	          return true;
	       return TransactionManager->MetaIndexParser->Exists(GetDiffIndexFileName(Target.MetaKey) + '.' + type);
	 });
	 os << *types.rbegin();
	 CompressionExtensions = os.str();
      }
   }
   Init(GetDiffIndexURI(Target), GetDiffIndexFileName(Target.Description), Target.ShortDesc);

   if(Debug)
      std::clog << "pkgAcqDiffIndex: " << Desc.URI << std::endl;
}
									/*}}}*/
void pkgAcqDiffIndex::QueueOnIMSHit() const				/*{{{*/
{
   // list cleanup needs to know that this file as well as the already
   // present index is ours, so we create an empty diff to save it for us
   new pkgAcqIndexDiffs(Owner, TransactionManager, Target);
}
									/*}}}*/
static bool RemoveFileForBootstrapLinking(std::string &ErrorText, std::string const &For, std::string const &Boot)/*{{{*/
{
   if (FileExists(Boot) && RemoveFile("Bootstrap-linking", Boot) == false)
   {
      strprintf(ErrorText, "Bootstrap for patching %s by removing stale %s failed!", For.c_str(), Boot.c_str());
      return false;
   }
   return true;
}
									/*}}}*/
bool pkgAcqDiffIndex::ParseDiffIndex(string const &IndexDiffFile)	/*{{{*/
{
   available_patches.clear();
   ExpectedAdditionalItems = 0;
   // failing here is fine: our caller will take care of trying to
   // get the complete file if patching fails
   if(Debug)
      std::clog << "pkgAcqDiffIndex::ParseIndexDiff() " << IndexDiffFile
	 << std::endl;

   FileFd Fd(IndexDiffFile, FileFd::ReadOnly, FileFd::Extension);
   pkgTagFile TF(&Fd);
   if (Fd.IsOpen() == false || Fd.Failed())
      return false;

   pkgTagSection Tags;
   if(unlikely(TF.Step(Tags) == false))
      return false;

   HashStringList ServerHashes;
   unsigned long long ServerSize = 0;

   auto const &posix = std::locale::classic();
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
      ss.imbue(posix);
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
      ErrorText = "Did not find a good hashsum in the index";
      return false;
   }

   std::string const CurrentPackagesFile = GetFinalFileNameFromURI(Target.URI);
   HashStringList const TargetFileHashes = GetExpectedHashesFor(Target.MetaKey);
   if (TargetFileHashes.usable() == false || ServerHashes != TargetFileHashes)
   {
      ErrorText = "Index has different hashes than parser (probably older)";
      return false;
   }

   HashStringList LocalHashes;
   // try avoiding calculating the hash here as this is costly
   if (TransactionManager->LastMetaIndexParser != NULL)
      LocalHashes = GetExpectedHashesFromFor(TransactionManager->LastMetaIndexParser, Target.MetaKey);
   if (LocalHashes.usable() == false)
   {
      FileFd fd(CurrentPackagesFile, FileFd::ReadOnly, FileFd::Auto);
      Hashes LocalHashesCalc(ServerHashes);
      LocalHashesCalc.AddFD(fd);
      LocalHashes = LocalHashesCalc.GetHashStringList();
   }

   if (ServerHashes == LocalHashes)
   {
      available_patches.clear();
      return true;
   }

   if(Debug)
      std::clog << "Server-Current: " << ServerHashes.find(NULL)->toStr() << " and we start at "
	 << CurrentPackagesFile << " " << LocalHashes.FileSize() << " " << LocalHashes.find(NULL)->toStr() << std::endl;

   // historically, older hashes have more info than newer ones, so start
   // collecting with older ones first to avoid implementing complicated
   // information merging techniques… a failure is after all always
   // recoverable with a complete file and hashes aren't changed that often.
   std::vector<char const *> types;
   for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
      types.push_back(*type);

   // parse all of (provided) history
   bool firstAcceptedHashes = true;
   for (auto type = types.crbegin(); type != types.crend(); ++type)
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
      ss.imbue(posix);

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
      ErrorText = "Couldn't find any patches for the patch series";
      return false;
   }

   for (auto type = types.crbegin(); type != types.crend(); ++type)
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
      ss.imbue(posix);

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

   for (auto type = types.crbegin(); type != types.crend(); ++type)
   {
      std::string tagname = *type;
      tagname.append("-Download");
      std::string const tmp = Tags.FindS(tagname.c_str());
      if (tmp.empty() == true)
	 continue;

      string hash, filename;
      unsigned long long size;
      std::stringstream ss(tmp);
      ss.imbue(posix);

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

   {
      auto const foundStart = std::find_if(available_patches.rbegin(), available_patches.rend(),
	    [&](auto const &cur) { return LocalHashes == cur.result_hashes; });
      if (foundStart == available_patches.rend() || unlikely(available_patches.empty()))
      {
	 ErrorText = "Couldn't find the start of the patch series";
	 return false;
      }
      available_patches.erase(available_patches.begin(), std::prev(foundStart.base()));

      auto const patch = std::find_if(available_patches.cbegin(), available_patches.cend(), [](auto const &patch) {
	 return not patch.result_hashes.usable() ||
		not patch.patch_hashes.usable() ||
		not patch.download_hashes.usable();
      });
      if (patch != available_patches.cend())
      {
	 strprintf(ErrorText, "Provides no usable hashes for %s", patch->file.c_str());
	 return false;
      }
   }

   // patching with too many files is rather slow compared to a fast download
   unsigned long const fileLimit = _config->FindI("Acquire::PDiffs::FileLimit", 0);
   if (fileLimit != 0 && fileLimit < available_patches.size())
   {
      strprintf(ErrorText, "Need %lu diffs, but limit is %lu", available_patches.size(), fileLimit);
      return false;
   }

   /* decide if we should download patches one by one or in one go:
      The first is good if the server merges patches, but many don't so client
      based merging can be attempt in which case the second is better.
      "bad things" will happen if patches are merged on the server,
      but client side merging is attempt as well */
   pdiff_merge = _config->FindB("Acquire::PDiffs::Merge", true);
   if (pdiff_merge == true)
   {
      // reprepro and dak add this flag if they merge patches on the server
      std::string const precedence = Tags.FindS("X-Patch-Precedence");
      pdiff_merge = (precedence != "merged");
   }

   // calculate the size of all patches we have to get
   unsigned short const sizeLimitPercent = _config->FindI("Acquire::PDiffs::SizeLimit", 100);
   if (sizeLimitPercent > 0)
   {
      unsigned long long downloadSize = 0;
      if (pdiff_merge)
	 downloadSize = std::accumulate(available_patches.begin(), available_patches.end(), 0llu,
					[](unsigned long long const T, DiffInfo const &I) {
					   return T + I.download_hashes.FileSize();
					});
      // if server-side merging, assume we will need only the first patch
      else if (not available_patches.empty())
	 downloadSize = available_patches.front().download_hashes.FileSize();
      if (downloadSize != 0)
      {
	 unsigned long long downloadSizeIdx = 0;
	 auto const types = VectorizeString(Target.Option(IndexTarget::COMPRESSIONTYPES), ' ');
	 for (auto const &t : types)
	 {
	    std::string MetaKey = Target.MetaKey;
	    if (t != "uncompressed")
	       MetaKey += '.' + t;
	    HashStringList const hsl = GetExpectedHashesFor(MetaKey);
	    if (unlikely(hsl.usable() == false))
	       continue;
	    downloadSizeIdx = hsl.FileSize();
	    break;
	 }
	 unsigned long long const sizeLimit = downloadSizeIdx * sizeLimitPercent;
	 if ((sizeLimit/100) < downloadSize)
	 {
	    strprintf(ErrorText, "Need %llu compressed bytes, but limit is %llu and original is %llu", downloadSize, (sizeLimit/100), downloadSizeIdx);
	    return false;
	 }
      }
   }

   // clean the plate
   {
      std::string const Final = GetExistingFilename(CurrentPackagesFile);
      if (unlikely(Final.empty())) // because we wouldn't be called in such a case
	 return false;
      std::string const PartialFile = GetPartialFileNameFromURI(Target.URI);
      std::string const PatchedFile = GetKeepCompressedFileName(PartialFile + "-patched", Target);
      if (not RemoveFileForBootstrapLinking(ErrorText, CurrentPackagesFile, PartialFile) ||
	  not RemoveFileForBootstrapLinking(ErrorText, CurrentPackagesFile, PatchedFile))
	 return false;
      {
	 auto const exts = APT::Configuration::getCompressorExtensions();
	 if (not std::all_of(exts.cbegin(), exts.cend(), [&](auto const &ext) {
		return RemoveFileForBootstrapLinking(ErrorText, CurrentPackagesFile, PartialFile + ext) &&
		       RemoveFileForBootstrapLinking(ErrorText, CurrentPackagesFile, PatchedFile + ext);
	     }))
	    return false;
      }
      std::string const Ext = Final.substr(CurrentPackagesFile.length());
      std::string const Partial = PartialFile + Ext;
      if (symlink(Final.c_str(), Partial.c_str()) != 0)
      {
	 strprintf(ErrorText, "Bootstrap for patching by linking %s to %s failed!", Final.c_str(), Partial.c_str());
	 return false;
      }
   }

   return true;
}
									/*}}}*/
void pkgAcqDiffIndex::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   if (CommonFailed(GetDiffIndexURI(Target), Message, Cnf))
      return;

   RenameOnError(PDiffError);
   Status = StatDone;
   ExpectedAdditionalItems = 0;

   if(Debug)
      std::clog << "pkgAcqDiffIndex failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;

   new pkgAcqIndex(Owner, TransactionManager, Target);
}
									/*}}}*/
bool pkgAcqDiffIndex::VerifyDone(std::string const &Message, pkgAcquire::MethodConfig const * const)/*{{{*/
{
   string const FinalFile = GetFinalFilename();
   if(StringToBool(LookupTag(Message,"IMS-Hit"),false))
      DestFile = FinalFile;

   if (ParseDiffIndex(DestFile))
      return true;

   Status = StatError;
   if (ErrorText.empty())
      ErrorText = "Couldn't parse pdiff index";
   return false;
}
									/*}}}*/
void pkgAcqDiffIndex::Done(string const &Message,HashStringList const &Hashes,	/*{{{*/
			   pkgAcquire::MethodConfig const * const Cnf)
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   if (available_patches.empty())
   {
      // we have the same sha1 as the server so we are done here
      if(Debug)
	 std::clog << "pkgAcqDiffIndex: Package file is up-to-date" << std::endl;
      QueueOnIMSHit();
   }
   else
   {
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
   }

   TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());

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
pkgAcqIndexDiffs::pkgAcqIndexDiffs(pkgAcquire *const Owner,
				   pkgAcqMetaClearSig *const TransactionManager,
				   IndexTarget const &Target,
				   vector<DiffInfo> const &diffs)
    : pkgAcqBaseIndex(Owner, TransactionManager, Target),
      available_patches(diffs)
{
   DestFile = GetKeepCompressedFileName(GetPartialFileNameFromURI(Target.URI), Target);

   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   Desc.Owner = this;
   Desc.ShortDesc = Target.ShortDesc;

   if(available_patches.empty() == true)
   {
      // we are done (yeah!), check hashes against the final file
      DestFile = GetKeepCompressedFileName(GetFinalFileNameFromURI(Target.URI), Target);
      Finish(true);
   }
   else
   {
      State = StateFetchDiff;
      QueueNextDiff();
   }
}
									/*}}}*/
void pkgAcqIndexDiffs::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   pkgAcqBaseIndex::Failed(Message,Cnf);
   Status = StatDone;

   DestFile = GetKeepCompressedFileName(GetPartialFileNameFromURI(Target.URI), Target);
   if(Debug)
      std::clog << "pkgAcqIndexDiffs failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire " << std::endl;
   RenameOnError(PDiffError);
   std::string const patchname = GetDiffsPatchFileName(DestFile);
   if (RealFileExists(patchname))
      Rename(patchname, patchname + ".FAILED");
   std::string const UnpatchedFile = GetExistingFilename(GetPartialFileNameFromURI(Target.URI));
   if (UnpatchedFile.empty() == false && FileExists(UnpatchedFile))
      Rename(UnpatchedFile, UnpatchedFile + ".FAILED");
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
      std::string const Final = GetKeepCompressedFileName(GetFinalFilename(), Target);
      TransactionManager->TransactionStageCopy(this, DestFile, Final);

      // this is for the "real" finish
      Complete = true;
      Status = StatDone;
      Dequeue();
      if(Debug)
	 std::clog << "\n\nallDone: " << DestFile << "\n" << std::endl;
      return;
   }
   else
      DestFile.clear();

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
   std::string const PartialFile = GetExistingFilename(GetPartialFileNameFromURI(Target.URI));
   if(unlikely(PartialFile.empty()))
   {
      Failed("Message: The file " + GetPartialFileNameFromURI(Target.URI) + " isn't available", NULL);
      return false;
   }

   FileFd fd(PartialFile, FileFd::ReadOnly, FileFd::Extension);
   Hashes LocalHashesCalc;
   LocalHashesCalc.AddFD(fd);
   HashStringList const LocalHashes = LocalHashesCalc.GetHashStringList();

   if(Debug)
      std::clog << "QueueNextDiff: " << PartialFile << " (" << LocalHashes.find(NULL)->toStr() << ")" << std::endl;

   HashStringList const TargetFileHashes = GetExpectedHashesFor(Target.MetaKey);
   if (unlikely(LocalHashes.usable() == false || TargetFileHashes.usable() == false))
   {
      Failed("Local/Expected hashes are not usable for " + PartialFile, NULL);
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
   available_patches.erase(available_patches.begin(),
	 std::find_if(available_patches.begin(), available_patches.end(), [&](DiffInfo const &I) {
	    return I.result_hashes == LocalHashes;
	    }));

   // error checking and falling back if no patch was found
   if(available_patches.empty() == true)
   {
      Failed("No patches left to reach target for " + PartialFile, NULL);
      return false;
   }

   // queue the right diff
   auto const BaseFileURI = Target.URI + ".diff/" + pkgAcquire::URIEncode(available_patches[0].file);
   Desc.URI = BaseFileURI + ".gz";
   Desc.Description = Target.Description + " " + available_patches[0].file + string(".pdiff");
   DestFile = GetKeepCompressedFileName(GetPartialFileNameFromURI(BaseFileURI), Target);

   if(Debug)
      std::clog << "pkgAcqIndexDiffs::QueueNextDiff(): " << Desc.URI << std::endl;

   QueueURI(Desc);

   return true;
}
									/*}}}*/
void pkgAcqIndexDiffs::Done(string const &Message, HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig const * const Cnf)
{
   if (Debug)
      std::clog << "pkgAcqIndexDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   std::string const UncompressedUnpatchedFile = GetPartialFileNameFromURI(Target.URI);
   std::string const UnpatchedFile = GetExistingFilename(UncompressedUnpatchedFile);
   std::string const PatchFile = GetDiffsPatchFileName(UnpatchedFile);
   std::string const PatchedFile = GetKeepCompressedFileName(UncompressedUnpatchedFile, Target);

   switch (State)
   {
      // success in downloading a diff, enter ApplyDiff state
      case StateFetchDiff:
	 Rename(DestFile, PatchFile);
	 DestFile = GetKeepCompressedFileName(UncompressedUnpatchedFile + "-patched", Target);
	 if(Debug)
	    std::clog << "Sending to rred method: " << UnpatchedFile << std::endl;
	 State = StateApplyDiff;
	 Local = true;
	 Desc.URI = "rred:" + pkgAcquire::URIEncode(UnpatchedFile);
	 QueueURI(Desc);
	 SetActiveSubprocess("rred");
	 return;
      // success in download/apply a diff, queue next (if needed)
      case StateApplyDiff:
	 // remove the just applied patch and base file
	 available_patches.erase(available_patches.begin());
	 RemoveFile("pkgAcqIndexDiffs::Done", PatchFile);
	 RemoveFile("pkgAcqIndexDiffs::Done", UnpatchedFile);
	 if(Debug)
	    std::clog << "Moving patched file in place: " << std::endl
	       << DestFile << " -> " << PatchedFile << std::endl;
	 Rename(DestFile, PatchedFile);

	 // see if there is more to download
	 if(available_patches.empty() == false)
	 {
	    new pkgAcqIndexDiffs(Owner, TransactionManager, Target, available_patches);
	    Finish();
	 } else {
	    DestFile = PatchedFile;
	    Finish(true);
	 }
	 return;
   }
}
									/*}}}*/
std::string pkgAcqIndexDiffs::Custom600Headers() const			/*{{{*/
{
   if(State != StateApplyDiff)
      return pkgAcqBaseIndex::Custom600Headers();
   std::ostringstream patchhashes;
   for (auto && hs : available_patches[0].result_hashes)
      patchhashes <<  "\nStart-" << hs.HashType() << "-Hash: " << hs.HashValue();
   for (auto && hs : available_patches[0].patch_hashes)
      patchhashes <<  "\nPatch-0-" << hs.HashType() << "-Hash: " << hs.HashValue();
   patchhashes << pkgAcqBaseIndex::Custom600Headers();
   return patchhashes.str();
}
									/*}}}*/
pkgAcqIndexDiffs::~pkgAcqIndexDiffs() {}

// AcqIndexMergeDiffs::AcqIndexMergeDiffs - Constructor			/*{{{*/
pkgAcqIndexMergeDiffs::pkgAcqIndexMergeDiffs(pkgAcquire *const Owner,
					     pkgAcqMetaClearSig *const TransactionManager,
					     IndexTarget const &Target,
					     DiffInfo const &patch,
					     std::vector<pkgAcqIndexMergeDiffs *> const *const allPatches)
    : pkgAcqBaseIndex(Owner, TransactionManager, Target),
      patch(patch), allPatches(allPatches), State(StateFetchDiff)
{
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   Desc.Owner = this;
   Desc.ShortDesc = Target.ShortDesc;
   Desc.URI = Target.URI + ".diff/" + pkgAcquire::URIEncode(patch.file) + ".gz";
   Desc.Description = Target.Description + " " + patch.file + ".pdiff";
   DestFile = GetPartialFileNameFromURI(Desc.URI);

   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs: " << Desc.URI << std::endl;

   QueueURI(Desc);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs failed: " << Desc.URI << " with " << Message << std::endl;

   pkgAcqBaseIndex::Failed(Message,Cnf);
   Status = StatDone;

   // check if we are the first to fail, otherwise we are done here
   State = StateDoneDiff;
   for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	 I != allPatches->end(); ++I)
      if ((*I)->State == StateErrorDiff)
      {
	 State = StateErrorDiff;
	 return;
      }

   // first failure means we should fallback
   State = StateErrorDiff;
   if (Debug)
      std::clog << "Falling back to normal index file acquire" << std::endl;
   RenameOnError(PDiffError);
   std::string const UnpatchedFile = GetExistingFilename(GetPartialFileNameFromURI(Target.URI));
   if (UnpatchedFile.empty() == false && FileExists(UnpatchedFile))
      Rename(UnpatchedFile, UnpatchedFile + ".FAILED");
   DestFile.clear();
   new pkgAcqIndex(Owner, TransactionManager, Target);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Done(string const &Message, HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig const * const Cnf)
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Hashes, Cnf);

   if (std::any_of(allPatches->begin(), allPatches->end(),
	    [](pkgAcqIndexMergeDiffs const * const P) { return P->State == StateErrorDiff; }))
   {
      if(Debug)
	 std::clog << "Another patch failed already, no point in processing this one." << std::endl;
      State = StateErrorDiff;
      return;
   }

   std::string const UncompressedUnpatchedFile = GetPartialFileNameFromURI(Target.URI);
   std::string const UnpatchedFile = GetExistingFilename(UncompressedUnpatchedFile);
   if (UnpatchedFile.empty())
   {
      _error->Fatal("Unpatched file %s doesn't exist (anymore)!", UncompressedUnpatchedFile.c_str());
      State = StateErrorDiff;
      return;
   }
   std::string const PatchedFile = GetKeepCompressedFileName(UncompressedUnpatchedFile, Target);

   switch (State)
   {
      case StateFetchDiff:
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
	 for (auto * diff : *allPatches)
	    Rename(diff->DestFile, GetMergeDiffsPatchFileName(UnpatchedFile, diff->patch.file));
	 // this is the last completed diff, so we are ready to apply now
	 DestFile = GetKeepCompressedFileName(UncompressedUnpatchedFile + "-patched", Target);
	 if(Debug)
	    std::clog << "Sending to rred method: " << UnpatchedFile << std::endl;
	 State = StateApplyDiff;
	 Local = true;
	 Desc.URI = "rred:" + pkgAcquire::URIEncode(UnpatchedFile);
	 QueueURI(Desc);
	 SetActiveSubprocess("rred");
	 return;
      case StateApplyDiff:
	 // success in download & apply all diffs, finialize and clean up
	 if(Debug)
	    std::clog << "Queue patched file in place: " << std::endl
	       << DestFile << " -> " << PatchedFile << std::endl;

	 // queue for copy by the transaction manager
	 TransactionManager->TransactionStageCopy(this, DestFile, GetKeepCompressedFileName(GetFinalFilename(), Target));

	 // ensure the ed's are gone regardless of list-cleanup
	 for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	       I != allPatches->end(); ++I)
	    RemoveFile("pkgAcqIndexMergeDiffs::Done", GetMergeDiffsPatchFileName(UnpatchedFile, (*I)->patch.file));
	 RemoveFile("pkgAcqIndexMergeDiffs::Done", UnpatchedFile);

	 // all set and done
	 Complete = true;
	 if(Debug)
	    std::clog << "allDone: " << DestFile << "\n" << std::endl;
	 return;
      case StateDoneDiff: _error->Fatal("Done called for %s which is in an invalid Done state", patch.file.c_str()); break;
      case StateErrorDiff: _error->Fatal("Done called for %s which is in an invalid Error state", patch.file.c_str()); break;
   }
}
									/*}}}*/
std::string pkgAcqIndexMergeDiffs::Custom600Headers() const		/*{{{*/
{
   if(State != StateApplyDiff)
      return pkgAcqBaseIndex::Custom600Headers();
   std::ostringstream patchhashes;
   unsigned int seen_patches = 0;
   for (auto && hs : (*allPatches)[0]->patch.result_hashes)
      patchhashes <<  "\nStart-" << hs.HashType() << "-Hash: " << hs.HashValue();
   for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	 I != allPatches->end(); ++I)
   {
      HashStringList const ExpectedHashes = (*I)->patch.patch_hashes;
      for (HashStringList::const_iterator hs = ExpectedHashes.begin(); hs != ExpectedHashes.end(); ++hs)
	 patchhashes <<  "\nPatch-" << std::to_string(seen_patches) << "-" << hs->HashType() << "-Hash: " << hs->HashValue();
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
                         IndexTarget const &Target, bool const Derived)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target), d(NULL), Stage(STAGE_DOWNLOAD),
   CompressionExtensions(Target.Option(IndexTarget::COMPRESSIONTYPES))
{
   if (Derived)
      return;
   Init(Target.URI, Target.Description, Target.ShortDesc);

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgIndex with TransactionManager "
                << TransactionManager << std::endl;
}
									/*}}}*/
// AcqIndex::Init - deferred Constructor				/*{{{*/
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

   // store file size of the download to ensure the fetcher gives
   // accurate progress reporting
   FileSize = GetExpectedHashes().FileSize();

   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;

   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqIndex::Custom600Headers() const
{
   std::string msg = pkgAcqBaseIndex::Custom600Headers();
   msg.append("\nIndex-File: true");

   if (TransactionManager->LastMetaIndexParser == NULL)
   {
      std::string const Final = GetFinalFilename();

      struct stat Buf;
      if (stat(Final.c_str(),&Buf) == 0)
	 msg += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime, false);
   }

   if(Target.IsOptional)
      msg += "\nFail-Ignore: true";

   return msg;
}
									/*}}}*/
// AcqIndex::Failed - getting the indexfile failed			/*{{{*/
bool pkgAcqIndex::CommonFailed(std::string const &TargetURI,
			       std::string const &Message, pkgAcquire::MethodConfig const *const Cnf)
{
   pkgAcqBaseIndex::Failed(Message,Cnf);
   // authorisation matches will not be fixed by other compression types
   if (Status != StatAuthError)
   {
      if (CompressionExtensions.empty() == false)
      {
	 Status = StatIdle;
	 Init(TargetURI, Desc.Description, Desc.ShortDesc);
	 return true;
      }
   }
   return false;
}
void pkgAcqIndex::Failed(string const &Message,pkgAcquire::MethodConfig const * const Cnf)
{
   if (CommonFailed(Target.URI, Message, Cnf))
      return;

   if(Target.IsOptional && GetExpectedHashes().empty() && Stage == STAGE_DOWNLOAD)
      Status = StatDone;
   else
      TransactionManager->AbortTransaction();
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
         StageDownloadDone(Message);
         break;
      case STAGE_DECOMPRESS_AND_VERIFY:
         StageDecompressDone();
         break;
   }
}
									/*}}}*/
// AcqIndex::StageDownloadDone - Queue for decompress and verify	/*{{{*/
void pkgAcqIndex::StageDownloadDone(string const &Message)
{
   Local = true;
   Complete = true;

   std::string const AltFilename = LookupTag(Message,"Alt-Filename");
   std::string Filename = LookupTag(Message,"Filename");

   // we need to verify the file against the current Release file again
   // on if-modified-since hit to avoid a stale attack against us
   if (StringToBool(LookupTag(Message, "IMS-Hit"), false))
   {
      Filename = GetExistingFilename(GetFinalFileNameFromURI(Target.URI));
      EraseFileName = DestFile = flCombine(flNotFile(DestFile), flNotDir(Filename));
      if (symlink(Filename.c_str(), DestFile.c_str()) != 0)
	 _error->WarningE("pkgAcqIndex::StageDownloadDone", "Symlinking file %s to %s failed", Filename.c_str(), DestFile.c_str());
      Stage = STAGE_DECOMPRESS_AND_VERIFY;
      Desc.URI = "store:" + pkgAcquire::URIEncode(DestFile);
      QueueURI(Desc);
      SetActiveSubprocess(::URI(Desc.URI).Access);
      return;
   }
   // methods like file:// give us an alternative (uncompressed) file
   else if (Target.KeepCompressed == false && AltFilename.empty() == false)
   {
      Filename = AltFilename;
      EraseFileName.clear();
   }
   // Methods like e.g. "file:" will give us a (compressed) FileName that is
   // not the "DestFile" we set, in this case we uncompress from the local file
   else if (Filename != DestFile && RealFileExists(DestFile) == false)
   {
      // symlinking ensures that the filename can be used for compression detection
      // that is e.g. needed for by-hash which has no extension over file
      if (symlink(Filename.c_str(),DestFile.c_str()) != 0)
	 _error->WarningE("pkgAcqIndex::StageDownloadDone", "Symlinking file %s to %s failed", Filename.c_str(), DestFile.c_str());
      else
      {
	 EraseFileName = DestFile;
	 Filename = DestFile;
      }
   }

   Stage = STAGE_DECOMPRESS_AND_VERIFY;
   DestFile = GetKeepCompressedFileName(GetPartialFileNameFromURI(Target.URI), Target);
   if (Filename != DestFile && flExtension(Filename) == flExtension(DestFile))
      Desc.URI = "copy:" + pkgAcquire::URIEncode(Filename);
   else
      Desc.URI = "store:" + pkgAcquire::URIEncode(Filename);
   if (DestFile == Filename)
   {
      if (CurrentCompressionExtension == "uncompressed")
	 return StageDecompressDone();
      DestFile = "/dev/null";
   }

   if (EraseFileName.empty() && Filename != AltFilename)
      EraseFileName = Filename;

   // queue uri for the next stage
   QueueURI(Desc);
   SetActiveSubprocess(::URI(Desc.URI).Access);
}
									/*}}}*/
// AcqIndex::StageDecompressDone - Final verification			/*{{{*/
void pkgAcqIndex::StageDecompressDone()
{
   if (DestFile == "/dev/null")
      DestFile = GetKeepCompressedFileName(GetPartialFileNameFromURI(Target.URI), Target);

   // Done, queue for rename on transaction finished
   TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());
}
									/*}}}*/
pkgAcqIndex::~pkgAcqIndex() {}

// AcqArchive::AcqArchive - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This just sets up the initial fetch environment and queues the first
   possibilitiy */
pkgAcqArchive::pkgAcqArchive(pkgAcquire *const Owner, pkgSourceList *const Sources,
			     pkgRecords *const Recs, pkgCache::VerIterator const &Version,
			     string &StoreFilename) : Item(Owner), d(NULL), LocalSource(false), Version(Version), Sources(Sources), Recs(Recs),
						      StoreFilename(StoreFilename),
						      Trusted(false)
{
   if (Version.Arch() == 0)
   {
      _error->Error(_("I wasn't able to locate a file for the %s package. "
		      "This might mean you need to manually fix this package. "
		      "(due to missing arch)"),
		    Version.ParentPkg().FullName().c_str());
      return;
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

   StoreFilename.clear();
   for (auto Vf = Version.FileList(); Vf.end() == false; ++Vf)
   {
      auto const PkgF = Vf.File();
      if (unlikely(PkgF.end()))
	 continue;
      if (PkgF.Flagged(pkgCache::Flag::NotSource))
	 continue;
      pkgIndexFile *Index;
      if (Sources->FindIndex(PkgF, Index) == false)
	 continue;
      if (Trusted && Index->IsTrusted() == false)
	 continue;

      pkgRecords::Parser &Parse = Recs->Lookup(Vf);
      // collect the hashes from the indexes
      auto hsl = Parse.Hashes();
      if (ExpectedHashes.empty())
	 ExpectedHashes = hsl;
      else
      {
	 // bad things will likely happen, but the user might be "lucky" still
	 // if the sources provide the same hashtypes (so that they aren't mixed)
	 for (auto const &hs : hsl)
	    if (ExpectedHashes.push_back(hs) == false)
	    {
	       _error->Warning("Sources disagree on hashes for supposely identical version '%s' of '%s'.",
			       Version.VerStr(), Version.ParentPkg().FullName(false).c_str());
	       break;
	    }
      }
      // only allow local volatile sources to have no hashes
      if (PkgF.Flagged(pkgCache::Flag::LocalSource))
	 LocalSource = true;
      else if (hsl.empty())
	 continue;

      std::string poolfilename = Parse.FileName();
      if (poolfilename.empty())
	 continue;

      std::remove_reference<decltype(ModifyCustomFields())>::type fields;
      {
	 auto const debIndex = dynamic_cast<pkgDebianIndexTargetFile const *const>(Index);
	 if (debIndex != nullptr)
	 {
	    auto const IT = debIndex->GetIndexTarget();
	    fields.emplace("Target-Repo-URI", IT.Option(IndexTarget::REPO_URI));
	    fields.emplace("Target-Release", IT.Option(IndexTarget::RELEASE));
	    fields.emplace("Target-Site", IT.Option(IndexTarget::SITE));
	 }
      }
      fields.emplace("Target-Base-URI", Index->ArchiveURI(""));
      if (PkgF->Component != 0)
	 fields.emplace("Target-Component", PkgF.Component());
      auto const RelF = PkgF.ReleaseFile();
      if (RelF.end() == false)
      {
	 if (RelF->Codename != 0)
	    fields.emplace("Target-Codename", RelF.Codename());
	 if (RelF->Archive != 0)
	    fields.emplace("Target-Suite", RelF.Archive());
      }
      fields.emplace("Target-Architecture", Version.Arch());
      fields.emplace("Target-Type", flExtension(poolfilename));

      if (StoreFilename.empty())
      {
	 /* We pick a filename based on the information we have for the version,
	    but we don't know what extension such a file should have, so we look
	    at the filenames used online and assume that they are the same for
	    all repositories containing this file */
	 StoreFilename = QuoteString(Version.ParentPkg().Name(), "_:") + '_' +
			 QuoteString(Version.VerStr(), "_:") + '_' +
			 QuoteString(Version.Arch(), "_:.") +
			 "." + flExtension(poolfilename);

	 Desc.URI = Index->ArchiveURI(poolfilename);
	 Desc.Description = Index->ArchiveInfo(Version);
	 Desc.Owner = this;
	 Desc.ShortDesc = Version.ParentPkg().FullName(true);
	 auto &customfields = ModifyCustomFields();
	 for (auto const &f : fields)
	    customfields[f.first] = f.second;
	 FileSize = Version->Size;
      }
      else
	 PushAlternativeURI(Index->ArchiveURI(poolfilename), std::move(fields), true);
   }
   if (StoreFilename.empty())
   {
      _error->Error(_("Can't find a source to download version '%s' of '%s'"),
		    Version.VerStr(), Version.ParentPkg().FullName(false).c_str());
      return;
   }
   if (FileSize == 0 && not _config->FindB("Acquire::AllowUnsizedPackages", false))
   {
      _error->Error("Repository is broken: %s has no Size information",
		    Desc.Description.c_str());
      return;
   }

   // Check if we already downloaded the file
   struct stat Buf;
   auto FinalFile = _config->FindDir("Dir::Cache::Archives") + flNotDir(StoreFilename);
   if (stat(FinalFile.c_str(), &Buf) == 0)
   {
      // Make sure the size matches
      if ((unsigned long long)Buf.st_size == Version->Size)
      {
	 Complete = true;
	 Local = true;
	 Status = StatDone;
	 StoreFilename = DestFile = FinalFile;
	 return;
      }

      /* Hmm, we have a file and its size does not match, this shouldn't
	 happen.. */
      RemoveFile("pkgAcqArchive::QueueNext", FinalFile);
   }

   // Check the destination file
   DestFile = _config->FindDir("Dir::Cache::Archives") + "partial/" + flNotDir(StoreFilename);
   if (stat(DestFile.c_str(), &Buf) == 0)
   {
      // Hmm, the partial file is too big, erase it
      if ((unsigned long long)Buf.st_size > Version->Size)
	 RemoveFile("pkgAcqArchive::QueueNext", DestFile);
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
      return;
   }

   // Create the item
   Local = false;
   QueueURI(Desc);
}
									/*}}}*/
bool pkgAcqArchive::QueueNext() /*{{{*/
{
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
class pkgAcqChangelog::Private
{
   public:
   std::string FinalFile;
};
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::VerIterator const &Ver,
      std::string const &DestDir, std::string const &DestFilename) :
   pkgAcquire::Item(Owner), d(new pkgAcqChangelog::Private()), SrcName(Ver.SourcePkgName()), SrcVersion(Ver.SourceVerStr())
{
   Desc.URI = URI(Ver);
   Init(DestDir, DestFilename);
}
// some parameters are char* here as they come likely from char* interfaces – which can also return NULL
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner, pkgCache::RlsFileIterator const &RlsFile,
      char const * const Component, char const * const SrcName, char const * const SrcVersion,
      const string &DestDir, const string &DestFilename) :
   pkgAcquire::Item(Owner), d(new pkgAcqChangelog::Private()), SrcName(SrcName), SrcVersion(SrcVersion)
{
   Desc.URI = URI(RlsFile, Component, SrcName, SrcVersion);
   Init(DestDir, DestFilename);
}
pkgAcqChangelog::pkgAcqChangelog(pkgAcquire * const Owner,
      std::string const &URI, char const * const SrcName, char const * const SrcVersion,
      const string &DestDir, const string &DestFilename) :
   pkgAcquire::Item(Owner), d(new pkgAcqChangelog::Private()), SrcName(SrcName), SrcVersion(SrcVersion)
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
      Desc.URI = "changelog:/" + pkgAcquire::URIEncode(DestFile);
      return;
   }

   std::string DestFileName;
   if (DestFilename.empty())
      DestFileName = flCombine(DestFile, SrcName + ".changelog");
   else
      DestFileName = flCombine(DestFile, DestFilename);

   std::string const SandboxUser = _config->Find("APT::Sandbox::User");
   std::string const systemTemp = GetTempDir(SandboxUser);
   char tmpname[1000];
   snprintf(tmpname, sizeof(tmpname), "%s/apt-changelog-XXXXXX", systemTemp.c_str());
   if (NULL == mkdtemp(tmpname))
   {
      _error->Errno("mkdtemp", "mkdtemp failed in changelog acquire of %s %s", SrcName.c_str(), SrcVersion.c_str());
      Status = StatError;
      return;
   }
   TemporaryDirectory = tmpname;

   ChangeOwnerAndPermissionOfFile("pkgAcqChangelog::Init", TemporaryDirectory.c_str(),
	 SandboxUser.c_str(), ROOT_GROUP, 0700);

   DestFile = flCombine(TemporaryDirectory, DestFileName);
   if (DestDir.empty() == false)
   {
      d->FinalFile = flCombine(DestDir, DestFileName);
      if (RealFileExists(d->FinalFile))
      {
	 FileFd file1, file2;
	 if (file1.Open(DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive) &&
	       file2.Open(d->FinalFile, FileFd::ReadOnly) && CopyFile(file2, file1))
	 {
	    ChangeOwnerAndPermissionOfFile("pkgAcqChangelog::Init", DestFile.c_str(), "root", ROOT_GROUP, 0644);
	    struct timeval times[2];
	    times[0].tv_sec = times[1].tv_sec = file2.ModificationTime();
	    times[0].tv_usec = times[1].tv_usec = 0;
	    utimes(DestFile.c_str(), times);
	 }
      }
   }

   Desc.ShortDesc = "Changelog";
   strprintf(Desc.Description, "%s %s %s Changelog", URI::SiteOnly(Desc.URI).c_str(), SrcName.c_str(), SrcVersion.c_str());
   Desc.Owner = this;
   QueueURI(Desc);
}
									/*}}}*/
std::string pkgAcqChangelog::URI(pkgCache::VerIterator const &Ver)	/*{{{*/
{
   std::string const confOnline = "Acquire::Changelogs::AlwaysOnline";
   bool AlwaysOnline = _config->FindB(confOnline, false);
   if (AlwaysOnline == false)
      for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; ++VF)
      {
	 pkgCache::PkgFileIterator const PF = VF.File();
	 if (PF.Flagged(pkgCache::Flag::NotSource) || PF->Release == 0)
	    continue;
	 pkgCache::RlsFileIterator const RF = PF.ReleaseFile();
	 if (RF->Origin != 0 && _config->FindB(confOnline + "::Origin::" + RF.Origin(), false))
	 {
	    AlwaysOnline = true;
	    break;
	 }
      }
   if (AlwaysOnline == false)
   {
      pkgCache::PkgIterator const Pkg = Ver.ParentPkg();
      if (Pkg->CurrentVer != 0 && Pkg.CurrentVer() == Ver)
      {
	 std::string const root = _config->FindDir("Dir");
	 std::string const basename = root + std::string("usr/share/doc/") + Pkg.Name() + "/changelog";
	 std::string const debianname = basename + ".Debian";
	 if (FileExists(debianname))
	    return "copy://" + debianname;
	 else if (FileExists(debianname + ".gz"))
	    return "store://" + debianname + ".gz";
	 else if (FileExists(basename))
	    return "copy://" + basename;
	 else if (FileExists(basename + ".gz"))
	    return "store://" + basename + ".gz";
      }
   }

   char const * const SrcName = Ver.SourcePkgName();
   char const * const SrcVersion = Ver.SourceVerStr();
   // find the first source for this version which promises a changelog
   for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; ++VF)
   {
      pkgCache::PkgFileIterator const PF = VF.File();
      if (PF.Flagged(pkgCache::Flag::NotSource) || PF->Release == 0)
	 continue;
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
   if (Template.find("@CHANGEPATH@") == std::string::npos)
      return "";

   // the path is: COMPONENT/SRC/SRCNAME/SRCNAME_SRCVER, e.g. main/a/apt/apt_1.1 or contrib/liba/libapt/libapt_2.0
   std::string const Src{SrcName};
   std::string path = pkgAcquire::URIEncode(APT::String::Startswith(SrcName, "lib") ? Src.substr(0, 4) : Src.substr(0,1));
   path.append("/").append(pkgAcquire::URIEncode(Src)).append("/");
   path.append(pkgAcquire::URIEncode(Src)).append("_").append(pkgAcquire::URIEncode(StripEpoch(SrcVersion)));
   // we omit component for releases without one (= flat-style repositories)
   if (Component != NULL && strlen(Component) != 0)
      path = pkgAcquire::URIEncode(Component) + "/" + path;

   return SubstVar(Template, "@CHANGEPATH@", path);
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
}
									/*}}}*/
// AcqChangelog::Done - Item downloaded OK				/*{{{*/
void pkgAcqChangelog::Done(string const &Message,HashStringList const &CalcHashes,
		      pkgAcquire::MethodConfig const * const Cnf)
{
   Item::Done(Message,CalcHashes,Cnf);
   if (d->FinalFile.empty() == false)
   {
      if (RemoveFile("pkgAcqChangelog::Done", d->FinalFile) == false ||
	    Rename(DestFile, d->FinalFile) == false)
	 Status = StatError;
   }

   Complete = true;
}
									/*}}}*/
pkgAcqChangelog::~pkgAcqChangelog()					/*{{{*/
{
   if (TemporaryDirectory.empty() == false)
   {
      RemoveFile("~pkgAcqChangelog", DestFile);
      rmdir(TemporaryDirectory.c_str());
   }
   delete d;
}
									/*}}}*/

// AcqFile::pkgAcqFile - Constructor					/*{{{*/
pkgAcqFile::pkgAcqFile(pkgAcquire *const Owner, string const &URI, HashStringList const &Hashes,
		       unsigned long long const Size, string const &Dsc, string const &ShortDesc,
		       const string &DestDir, const string &DestFilename,
		       bool const IsIndexFile) : Item(Owner), d(NULL), IsIndexFile(IsIndexFile), ExpectedHashes(Hashes)
{
   ::URI url{URI};
   if (url.Path.find(' ') != std::string::npos || url.Path.find('%') == std::string::npos)
      url.Path = pkgAcquire::URIEncode(url.Path);

   if(!DestFilename.empty())
      DestFile = DestFilename;
   else if(!DestDir.empty())
      DestFile = DestDir + "/" + DeQuoteString(flNotDir(url.Path));
   else
      DestFile = DeQuoteString(flNotDir(url.Path));

   // Create the item
   Desc.URI = std::string(url);
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
	 RemoveFile("pkgAcqFile", DestFile);
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
	 Desc.URI = "copy:" + pkgAcquire::URIEncode(FileName);
	 QueueURI(Desc);
	 return;
      }

      // Erase the file if it is a symlink so we can overwrite it
      struct stat St;
      if (lstat(DestFile.c_str(),&St) == 0)
      {
	 if (S_ISLNK(St.st_mode) != 0)
	    RemoveFile("pkgAcqFile::Done", DestFile);
      }

      // Symlink the file
      if (symlink(FileName.c_str(),DestFile.c_str()) != 0)
      {
	 _error->PushToStack();
	 _error->Errno("pkgAcqFile::Done", "Symlinking file %s failed", DestFile.c_str());
	 std::stringstream msg;
	 _error->DumpErrors(msg, GlobalError::DEBUG, false);
	 _error->RevertToStack();
	 ErrorText = msg.str();
	 Status = StatError;
	 Complete = false;
      }
   }
}
									/*}}}*/
string pkgAcqFile::Custom600Headers() const				/*{{{*/
{
   string Header = pkgAcquire::Item::Custom600Headers();
   if (not IsIndexFile)
      return Header;
   return Header + "\nIndex-File: true";
}
									/*}}}*/
pkgAcqFile::~pkgAcqFile() {}

void pkgAcqAuxFile::Failed(std::string const &Message, pkgAcquire::MethodConfig const *const Cnf) /*{{{*/
{
   pkgAcqFile::Failed(Message, Cnf);
   if (Status == StatIdle)
      return;
   if (RealFileExists(DestFile))
      Rename(DestFile, DestFile + ".FAILED");
   Worker->ReplyAux(Desc);
}
									/*}}}*/
void pkgAcqAuxFile::Done(std::string const &Message, HashStringList const &CalcHashes, /*{{{*/
			 pkgAcquire::MethodConfig const *const Cnf)
{
   pkgAcqFile::Done(Message, CalcHashes, Cnf);
   if (Status == StatDone)
      Worker->ReplyAux(Desc);
   else if (Status == StatAuthError || Status == StatError)
      Worker->ReplyAux(Desc);
}
									/*}}}*/
std::string pkgAcqAuxFile::Custom600Headers() const /*{{{*/
{
   if (MaximumSize == 0)
      return pkgAcqFile::Custom600Headers();
   std::string maxsize;
   strprintf(maxsize, "\nMaximum-Size: %llu", MaximumSize);
   return pkgAcqFile::Custom600Headers().append(maxsize);
}
									/*}}}*/
void pkgAcqAuxFile::Finished() /*{{{*/
{
   auto dirname = flCombine(_config->FindDir("Dir::State::lists"), "auxfiles/");
   if (APT::String::Startswith(DestFile, dirname))
   {
      // the file is never returned by method requesting it, so fix up the permission now
      if (FileExists(DestFile))
      {
	 ChangeOwnerAndPermissionOfFile("pkgAcqAuxFile", DestFile.c_str(), "root", ROOT_GROUP, 0644);
	 if (Status == StatDone)
	    return;
      }
   }
   else
   {
      dirname = flNotFile(DestFile);
      RemoveFile("pkgAcqAuxFile::Finished", DestFile);
      RemoveFile("pkgAcqAuxFile::Finished", DestFile + ".FAILED");
      rmdir(dirname.c_str());
   }
   DestFile.clear();
}
									/*}}}*/
// GetAuxFileNameFromURI						/*{{{*/
static std::string GetAuxFileNameFromURIInLists(std::string const &uri)
{
   // check if we have write permission for our usual location.
   auto const dirname = flCombine(_config->FindDir("Dir::State::lists"), "auxfiles/");
   char const * const filetag = ".apt-acquire-privs-test.XXXXXX";
   std::string const tmpfile_tpl = flCombine(dirname, filetag);
   std::unique_ptr<char, decltype(std::free) *> tmpfile { strdup(tmpfile_tpl.c_str()), std::free };
   int const fd = mkstemp(tmpfile.get());
   if (fd == -1)
      return "";
   RemoveFile("GetAuxFileNameFromURI", tmpfile.get());
   close(fd);
   return flCombine(dirname, URItoFileName(uri));
}
static std::string GetAuxFileNameFromURI(std::string const &uri)
{
   auto const lists = GetAuxFileNameFromURIInLists(uri);
   if (lists.empty() == false)
      return lists;

   std::string tmpdir_tpl;
   strprintf(tmpdir_tpl, "%s/apt-auxfiles-XXXXXX", GetTempDir().c_str());
   std::unique_ptr<char, decltype(std::free) *> tmpdir { strndup(tmpdir_tpl.data(), tmpdir_tpl.length()), std::free };
   if (mkdtemp(tmpdir.get()) == nullptr)
   {
      _error->Errno("GetAuxFileNameFromURI", "mkdtemp of %s failed", tmpdir.get());
      return flCombine("/nonexistent/auxfiles/", URItoFileName(uri));
   }
   chmod(tmpdir.get(), 0755);
   auto const filename = flCombine(tmpdir.get(), URItoFileName(uri));
   _error->PushToStack();
   FileFd in(flCombine(flCombine(_config->FindDir("Dir::State::lists"), "auxfiles/"), URItoFileName(uri)), FileFd::ReadOnly);
   if (in.IsOpen())
   {
      FileFd out(filename, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive);
      CopyFile(in, out);
      ChangeOwnerAndPermissionOfFile("GetAuxFileNameFromURI", filename.c_str(), "root", ROOT_GROUP, 0644);
   }
   _error->RevertToStack();
   return filename;
}
									/*}}}*/
pkgAcqAuxFile::pkgAcqAuxFile(pkgAcquire::Item *const Owner, pkgAcquire::Worker *const Worker,
			     std::string const &ShortDesc, std::string const &Desc, std::string const &URI,
			     HashStringList const &Hashes, unsigned long long const MaximumSize) : pkgAcqFile(Owner->GetOwner(), URI, Hashes, Hashes.FileSize(), Desc, ShortDesc, "", GetAuxFileNameFromURI(URI), false),
												   Owner(Owner), Worker(Worker), MaximumSize(MaximumSize)
{
   /* very bad failures can happen while constructing which causes
      us to hang as the aux request is never answered (e.g. method not available)
      Ideally we catch failures earlier, but a safe guard can't hurt. */
   if (Status == pkgAcquire::Item::StatIdle || Status == pkgAcquire::Item::StatFetching)
      return;
   Failed(std::string("400 URI Failure\n") +
	     "URI: " + URI + "\n" +
	     "Filename: " + DestFile,
	  nullptr);
}
pkgAcqAuxFile::~pkgAcqAuxFile() {}
