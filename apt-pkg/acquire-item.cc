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
#include <apt-pkg/sha1.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgrecords.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <ctime>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

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
static void ChangeOwnerAndPermissionOfFile(char const * const requester, char const * const file, char const * const user, char const * const group, mode_t const mode) /*{{{*/
{
   // ensure the file is owned by root and has good permissions
   struct passwd const * const pw = getpwnam(user);
   struct group const * const gr = getgrnam(group);
   if (getuid() == 0) // if we aren't root, we can't chown, so don't try it
   {
      if (pw != NULL && gr != NULL && chown(file, pw->pw_uid, gr->gr_gid) != 0)
	 _error->WarningE(requester, "chown to %s:%s of file %s failed", user, group, file);
   }
   if (chmod(file, mode) != 0)
      _error->WarningE(requester, "chmod 0%o of file %s failed", mode, file);
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
static std::string GetCompressedFileName(std::string const &URI, std::string const &Name, std::string const &Ext) /*{{{*/
{
   if (Ext.empty() || Ext == "uncompressed")
      return Name;

   // do not reverify cdrom sources as apt-cdrom may rewrite the Packages
   // file when its doing the indexcopy
   if (URI.substr(0,6) == "cdrom:")
      return Name;

   // adjust DestFile if its compressed on disk
   if (_config->FindB("Acquire::GzipIndexes",false) == true)
      return Name + '.' + Ext;
   return Name;
}
									/*}}}*/
static bool AllowInsecureRepositories(indexRecords const * const MetaIndexParser, pkgAcqMetaBase * const TransactionManager, pkgAcquire::Item * const I) /*{{{*/
{
   if(MetaIndexParser->IsAlwaysTrusted() || _config->FindB("Acquire::AllowInsecureRepositories") == true)
      return true;

   _error->Error(_("Use --allow-insecure-repositories to force the update"));
   TransactionManager->AbortTransaction();
   I->Status = pkgAcquire::Item::StatError;
   return false;
}
									/*}}}*/


// Acquire::Item::Item - Constructor					/*{{{*/
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
pkgAcquire::Item::Item(pkgAcquire *Owner,
                       HashStringList const &ExpectedHashes,
                       pkgAcqMetaBase *TransactionManager)
   : Owner(Owner), FileSize(0), PartialSize(0), Mode(0), ID(0), Complete(false),
     Local(false), QueueCounter(0), TransactionManager(TransactionManager),
     ExpectedAdditionalItems(0), ExpectedHashes(ExpectedHashes)
{
   Owner->Add(this);
   Status = StatIdle;
   if(TransactionManager != NULL)
      TransactionManager->Add(this);
}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
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
void pkgAcquire::Item::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   if(ErrorText == "")
      ErrorText = LookupTag(Message,"Message");
   UsedMirror =  LookupTag(Message,"UsedMirror");
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

      Status = StatError;
      Complete = false;
      Dequeue();
   }
   else
      Status = StatIdle;

   // check fail reason
   string FailReason = LookupTag(Message, "FailReason");
   if(FailReason == "MaximumSizeExceeded")
      Rename(DestFile, DestFile+".FAILED");

   // report mirror failure back to LP if we actually use a mirror
   if(FailReason.size() != 0)
      ReportMirrorFailure(FailReason);
   else
      ReportMirrorFailure(ErrorText);
}
									/*}}}*/
// Acquire::Item::Start - Item has begun to download			/*{{{*/
// ---------------------------------------------------------------------
/* Stash status and the file size. Note that setting Complete means 
   sub-phases of the acquire process such as decompresion are operating */
void pkgAcquire::Item::Start(string /*Message*/,unsigned long long Size)
{
   Status = StatFetching;
   if (FileSize == 0 && Complete == false)
      FileSize = Size;
}
									/*}}}*/
// Acquire::Item::Done - Item downloaded OK				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcquire::Item::Done(string Message,unsigned long long Size,HashStringList const &/*Hash*/,
			    pkgAcquire::MethodConfig * /*Cnf*/)
{
   // We just downloaded something..
   string FileName = LookupTag(Message,"Filename");
   UsedMirror = LookupTag(Message,"UsedMirror");
   if (Complete == false && !Local && FileName == DestFile)
   {
      if (Owner->Log != 0)
	 Owner->Log->Fetched(Size,atoi(LookupTag(Message,"Resume-Point","0").c_str()));
   }

   if (FileSize == 0)
      FileSize= Size;
   Status = StatDone;
   ErrorText = string();
   Owner->Dequeue(this);
}
									/*}}}*/
// Acquire::Item::Rename - Rename a file				/*{{{*/
// ---------------------------------------------------------------------
/* This helper function is used by a lot of item methods as their final
   step */
bool pkgAcquire::Item::Rename(string From,string To)
{
   if (rename(From.c_str(),To.c_str()) != 0)
   {
      char S[300];
      snprintf(S,sizeof(S),_("rename failed, %s (%s -> %s)."),strerror(errno),
	      From.c_str(),To.c_str());
      Status = StatError;
      ErrorText += S;
      return false;
   }   
   return true;
}
									/*}}}*/
void pkgAcquire::Item::QueueURI(ItemDesc &Item)				/*{{{*/
{
   if (RealFileExists(DestFile))
   {
      std::string SandboxUser = _config->Find("APT::Sandbox::User");
      ChangeOwnerAndPermissionOfFile("GetPartialFileName", DestFile.c_str(),
                                     SandboxUser.c_str(), "root", 0600);
   }
   Owner->Enqueue(Item);
}
									/*}}}*/
void pkgAcquire::Item::Dequeue()					/*{{{*/
{
   Owner->Dequeue(this);
}
									/*}}}*/
bool pkgAcquire::Item::RenameOnError(pkgAcquire::Item::RenameOnErrorState const error)/*{{{*/
{
   if(FileExists(DestFile))
      Rename(DestFile, DestFile + ".FAILED");

   switch (error)
   {
      case HashSumMismatch:
	 ErrorText = _("Hash Sum mismatch");
	 Status = StatAuthError;
	 ReportMirrorFailure("HashChecksumFailure");
	 break;
      case SizeMismatch:
	 ErrorText = _("Size mismatch");
	 Status = StatAuthError;
	 ReportMirrorFailure("SizeFailure");
	 break;
      case InvalidFormat:
	 ErrorText = _("Invalid file format");
	 Status = StatError;
	 // do not report as usually its not the mirrors fault, but Portal/Proxy
	 break;
      case SignatureError:
	 ErrorText = _("Signature error");
	 Status = StatError;
	 break;
      case NotClearsigned:
         ErrorText = _("Does not start with a cleartext signature");
	 Status = StatError;
	 break;
   }
   return false;
}
									/*}}}*/
void pkgAcquire::Item::SetActiveSubprocess(const std::string &subprocess)/*{{{*/
{
      ActiveSubprocess = subprocess;
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        Mode = ActiveSubprocess.c_str();
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
}
									/*}}}*/
// Acquire::Item::ReportMirrorFailure					/*{{{*/
// ---------------------------------------------------------------------
void pkgAcquire::Item::ReportMirrorFailure(string FailCode)
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
   const char *Args[40];
   unsigned int i = 0;
   string report = _config->Find("Methods::Mirror::ProblemReporting", 
				 "/usr/lib/apt/apt-report-mirror-failure");
   if(!FileExists(report))
      return;
   Args[i++] = report.c_str();
   Args[i++] = UsedMirror.c_str();
   Args[i++] = DescURI().c_str();
   Args[i++] = FailCode.c_str();
   Args[i++] = NULL;
   pid_t pid = ExecFork();
   if(pid < 0) 
   {
      _error->Error("ReportMirrorFailure Fork failed");
      return;
   }
   else if(pid == 0) 
   {
      execvp(Args[0], (char**)Args);
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
// AcqDiffIndex::AcqDiffIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Get the DiffIndex file first and see if there are patches available
 * If so, create a pkgAcqIndexDiffs fetcher that will get and apply the
 * patches. If anything goes wrong in that process, it will fall back to
 * the original packages file
 */
pkgAcqDiffIndex::pkgAcqDiffIndex(pkgAcquire *Owner,
                                 pkgAcqMetaBase *TransactionManager,
                                 IndexTarget const * const Target,
				 HashStringList const &ExpectedHashes,
                                 indexRecords *MetaIndexParser)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHashes, 
                     MetaIndexParser), PackagesFileReadyInPartial(false)
{
   
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   RealURI = Target->URI;
   Desc.Owner = this;
   Desc.Description = Target->Description + ".diff/Index";
   Desc.ShortDesc = Target->ShortDesc;
   Desc.URI = Target->URI + ".diff/Index";

   DestFile = GetPartialFileNameFromURI(Desc.URI);

   if(Debug)
      std::clog << "pkgAcqDiffIndex: " << Desc.URI << std::endl;

   // look for the current package file
   CurrentPackagesFile = _config->FindDir("Dir::State::lists");
   CurrentPackagesFile += URItoFileName(RealURI);

   // FIXME: this file:/ check is a hack to prevent fetching
   //        from local sources. this is really silly, and
   //        should be fixed cleanly as soon as possible
   if(!FileExists(CurrentPackagesFile) || 
      Desc.URI.substr(0,strlen("file:/")) == "file:/")
   {
      // we don't have a pkg file or we don't want to queue
      Failed("No index file, local or canceld by user", NULL);
      return;
   }

   if(Debug)
      std::clog << "pkgAcqDiffIndex::pkgAcqDiffIndex(): "
	 << CurrentPackagesFile << std::endl;

   QueueURI(Desc);

}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqDiffIndex::Custom600Headers() const
{
   string Final = _config->FindDir("Dir::State::lists");
   Final += URItoFileName(Desc.URI);

   if(Debug)
      std::clog << "Custom600Header-IMS: " << Final << std::endl;

   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return "\nIndex-File: true";
   
   return "\nIndex-File: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
bool pkgAcqDiffIndex::ParseDiffIndex(string IndexDiffFile)		/*{{{*/
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

   if (ServerHashes != HashSums())
   {
      if (Debug == true)
      {
	 std::clog << "pkgAcqDiffIndex: " << IndexDiffFile << ": Index has different hashes than parser, probably older, so fail pdiffing" << std::endl;
         printHashSumComparision(CurrentPackagesFile, ServerHashes, HashSums());
      }
      return false;
   }

   if (ServerHashes.VerifyFile(CurrentPackagesFile) == true)
   {
      // we have the same sha1 as the server so we are done here
      if(Debug)
	 std::clog << "pkgAcqDiffIndex: Package file " << CurrentPackagesFile << " is up-to-date" << std::endl;

      // list cleanup needs to know that this file as well as the already
      // present index is ours, so we create an empty diff to save it for us
      new pkgAcqIndexDiffs(Owner, TransactionManager, Target,
                           ExpectedHashes, MetaIndexParser);
      return true;
   }

   FileFd fd(CurrentPackagesFile, FileFd::ReadOnly);
   Hashes LocalHashesCalc;
   LocalHashesCalc.AddFD(fd);
   HashStringList const LocalHashes = LocalHashesCalc.GetHashStringList();

   if(Debug)
      std::clog << "Server-Current: " << ServerHashes.find(NULL)->toStr() << " and we start at "
	 << fd.Name() << " " << fd.FileSize() << " " << LocalHashes.find(NULL)->toStr() << std::endl;

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
	    if (cur->file != filename || unlikely(cur->result_size != size))
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
	    next.result_size = size;
	    next.patch_size = 0;
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
	    if (unlikely(cur->patch_size != 0 && cur->patch_size != size))
	       continue;
	    cur->patch_hashes.push_back(HashString(*type, hash));
	    cur->patch_size = size;
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
      patchesSize += cur->patch_size;
   unsigned long long const sizeLimit = ServerSize * _config->FindI("Acquire::PDiffs::SizeLimit", 100);
   if (false && sizeLimit > 0 && (sizeLimit/100) < patchesSize)
   {
      if (Debug)
	 std::clog << "Need " << patchesSize << " bytes (Limit is " << sizeLimit/100
	    << ") so fallback to complete download" << std::endl;
      return false;
   }

   // FIXME: make this use the method
   PackagesFileReadyInPartial = true;
   std::string const Partial = GetPartialFileNameFromURI(RealURI);
   
   FileFd From(CurrentPackagesFile, FileFd::ReadOnly);
   FileFd To(Partial, FileFd::WriteEmpty);
   if(CopyFile(From, To) == false)
      return _error->Errno("CopyFile", "failed to copy");
   
   if(Debug)
      std::cerr << "Done copying " << CurrentPackagesFile
                << " -> " << Partial
                << std::endl;

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
   {
      new pkgAcqIndexDiffs(Owner, TransactionManager, Target, ExpectedHashes, 
                           MetaIndexParser, available_patches);
   }
   else
   {
      std::vector<pkgAcqIndexMergeDiffs*> *diffs = new std::vector<pkgAcqIndexMergeDiffs*>(available_patches.size());
      for(size_t i = 0; i < available_patches.size(); ++i)
	 (*diffs)[i] = new pkgAcqIndexMergeDiffs(Owner, TransactionManager,
               Target,
	       ExpectedHashes,
	       MetaIndexParser,
	       available_patches[i],
	       diffs);
   }

   Complete = false;
   Status = StatDone;
   Dequeue();
   return true;
}
									/*}}}*/
void pkgAcqDiffIndex::Failed(string Message,pkgAcquire::MethodConfig * Cnf)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;

   new pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser);

   Item::Failed(Message,Cnf);
   Status = StatDone;
}
									/*}}}*/
void pkgAcqDiffIndex::Done(string Message,unsigned long long Size,HashStringList const &Hashes,	/*{{{*/
			   pkgAcquire::MethodConfig *Cnf)
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Size, Hashes, Cnf);

   // verify the index target
   if(Target && Target->MetaKey != "" && MetaIndexParser && Hashes.usable())
   {
      std::string IndexMetaKey  = Target->MetaKey + ".diff/Index";
      indexRecords::checkSum *Record = MetaIndexParser->Lookup(IndexMetaKey);
      if(Record && Record->Hashes.usable() && Hashes != Record->Hashes)
      {
         RenameOnError(HashSumMismatch);
         printHashSumComparision(RealURI, Record->Hashes, Hashes);
         Failed(Message, Cnf);
         return;
      }

   }

   string FinalFile;
   FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(Desc.URI);

   if(StringToBool(LookupTag(Message,"IMS-Hit"),false))
      DestFile = FinalFile;

   if(!ParseDiffIndex(DestFile))
      return Failed("Message: Couldn't parse pdiff index", Cnf);

   // queue for final move
   TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);

   Complete = true;
   Status = StatDone;
   Dequeue();
   return;
}
									/*}}}*/
// AcqIndexDiffs::AcqIndexDiffs - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* The package diff is added to the queue. one object is constructed
 * for each diff and the index
 */
pkgAcqIndexDiffs::pkgAcqIndexDiffs(pkgAcquire *Owner,
                                   pkgAcqMetaBase *TransactionManager,
                                   struct IndexTarget const * const Target,
                                   HashStringList const &ExpectedHashes,
                                   indexRecords *MetaIndexParser,
				   vector<DiffInfo> diffs)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser),
     available_patches(diffs)
{
   DestFile = GetPartialFileNameFromURI(Target->URI);

   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   RealURI = Target->URI;
   Desc.Owner = this;
   Description = Target->Description;
   Desc.ShortDesc = Target->ShortDesc;

   if(available_patches.empty() == true)
   {
      // we are done (yeah!), check hashes against the final file
      DestFile = _config->FindDir("Dir::State::lists");
      DestFile += URItoFileName(Target->URI);
      Finish(true);
   }
   else
   {
      // get the next diff
      State = StateFetchDiff;
      QueueNextDiff();
   }
}
									/*}}}*/
void pkgAcqIndexDiffs::Failed(string Message,pkgAcquire::MethodConfig * /*Cnf*/)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqIndexDiffs failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;
   new pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser);
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
      if(HashSums().usable() && !HashSums().VerifyFile(DestFile))
      {
	 RenameOnError(HashSumMismatch);
	 Dequeue();
	 return;
      }

      // queue for copy
      std::string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);

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
   std::string const FinalFile = GetPartialFileNameFromURI(RealURI);

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

   if (unlikely(LocalHashes.usable() == false || ExpectedHashes.usable() == false))
   {
      Failed("Local/Expected hashes are not usable", NULL);
      return false;
   }


   // final file reached before all patches are applied
   if(LocalHashes == ExpectedHashes)
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
   Desc.URI = RealURI + ".diff/" + available_patches[0].file + ".gz";
   Desc.Description = Description + " " + available_patches[0].file + string(".pdiff");
   DestFile = GetPartialFileNameFromURI(RealURI + ".diff/" + available_patches[0].file);

   if(Debug)
      std::clog << "pkgAcqIndexDiffs::QueueNextDiff(): " << Desc.URI << std::endl;

   QueueURI(Desc);

   return true;
}
									/*}}}*/
void pkgAcqIndexDiffs::Done(string Message,unsigned long long Size, HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig *Cnf)
{
   if(Debug)
      std::clog << "pkgAcqIndexDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Size, Hashes, Cnf);

   // FIXME: verify this download too before feeding it to rred
   std::string const FinalFile = GetPartialFileNameFromURI(RealURI);

   // success in downloading a diff, enter ApplyDiff state
   if(State == StateFetchDiff)
   {
      FileFd fd(DestFile, FileFd::ReadOnly, FileFd::Gzip);
      class Hashes LocalHashesCalc;
      LocalHashesCalc.AddFD(fd);
      HashStringList const LocalHashes = LocalHashesCalc.GetHashStringList();

      if (fd.Size() != available_patches[0].patch_size ||
	    available_patches[0].patch_hashes != LocalHashes)
      {
	 Failed("Patch has Size/Hashsum mismatch", NULL);
	 return;
      }

      // rred excepts the patch as $FinalFile.ed
      Rename(DestFile,FinalFile+".ed");

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
      unlink((FinalFile + ".ed").c_str());

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
			      ExpectedHashes, MetaIndexParser,
			      available_patches);
	 return Finish();
      } else 
         // update
         DestFile = FinalFile;
	 return Finish(true);
   }
}
									/*}}}*/
// AcqIndexMergeDiffs::AcqIndexMergeDiffs - Constructor			/*{{{*/
pkgAcqIndexMergeDiffs::pkgAcqIndexMergeDiffs(pkgAcquire *Owner,
                                             pkgAcqMetaBase *TransactionManager,
                                             struct IndexTarget const * const Target,
                                             HashStringList const &ExpectedHashes,
                                             indexRecords *MetaIndexParser,
                                             DiffInfo const &patch,
                                             std::vector<pkgAcqIndexMergeDiffs*> const * const allPatches)
  : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser),
     patch(patch), allPatches(allPatches), State(StateFetchDiff)
{
   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   RealURI = Target->URI;
   Desc.Owner = this;
   Description = Target->Description;
   Desc.ShortDesc = Target->ShortDesc;

   Desc.URI = RealURI + ".diff/" + patch.file + ".gz";
   Desc.Description = Description + " " + patch.file + string(".pdiff");

   DestFile = GetPartialFileNameFromURI(RealURI + ".diff/" + patch.file);

   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs: " << Desc.URI << std::endl;

   QueueURI(Desc);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Failed(string Message,pkgAcquire::MethodConfig * Cnf)/*{{{*/
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
   std::clog << "Falling back to normal index file acquire" << std::endl;
   new pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Done(string Message,unsigned long long Size,HashStringList const &Hashes,	/*{{{*/
			    pkgAcquire::MethodConfig *Cnf)
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs::Done(): " << Desc.URI << std::endl;

   Item::Done(Message,Size,Hashes,Cnf);

   // FIXME: verify download before feeding it to rred
   string const FinalFile = GetPartialFileNameFromURI(RealURI);

   if (State == StateFetchDiff)
   {
      FileFd fd(DestFile, FileFd::ReadOnly, FileFd::Gzip);
      class Hashes LocalHashesCalc;
      LocalHashesCalc.AddFD(fd);
      HashStringList const LocalHashes = LocalHashesCalc.GetHashStringList();

      if (fd.Size() != patch.patch_size || patch.patch_hashes != LocalHashes)
      {
	 Failed("Patch has Size/Hashsum mismatch", NULL);
	 return;
      }

      // rred expects the patch as $FinalFile.ed.$patchname.gz
      Rename(DestFile, FinalFile + ".ed." + patch.file + ".gz");

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
      // see if we really got the expected file
      if(ExpectedHashes.usable() && !ExpectedHashes.VerifyFile(DestFile))
      {
	 RenameOnError(HashSumMismatch);
	 return;
      }


      std::string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);

      // move the result into place
      if(Debug)
	 std::clog << "Queue patched file in place: " << std::endl
		   << DestFile << " -> " << FinalFile << std::endl;

      // queue for copy by the transaction manager
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);

      // ensure the ed's are gone regardless of list-cleanup
      for (std::vector<pkgAcqIndexMergeDiffs *>::const_iterator I = allPatches->begin();
	    I != allPatches->end(); ++I)
      {
	 std::string const PartialFile = GetPartialFileNameFromURI(RealURI);
	 std::string patch = PartialFile + ".ed." + (*I)->patch.file + ".gz";
	 unlink(patch.c_str());
      }

      // all set and done
      Complete = true;
      if(Debug)
	 std::clog << "allDone: " << DestFile << "\n" << std::endl;
   }
}
									/*}}}*/
// AcqBaseIndex::VerifyHashByMetaKey - verify hash for the given metakey /*{{{*/
bool pkgAcqBaseIndex::VerifyHashByMetaKey(HashStringList const &Hashes)
{
   if(MetaKey != "" && Hashes.usable())
   {
      indexRecords::checkSum *Record = MetaIndexParser->Lookup(MetaKey);
      if(Record && Record->Hashes.usable() && Hashes != Record->Hashes)
      {
         printHashSumComparision(RealURI, Record->Hashes, Hashes);
         return false;
      }
   }
   return true;
}
									/*}}}*/
// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The package file is added to the queue and a second class is
   instantiated to fetch the revision file */
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,
			 string URI,string URIDesc,string ShortDesc,
			 HashStringList const  &ExpectedHash)
   : pkgAcqBaseIndex(Owner, 0, NULL, ExpectedHash, NULL)
{
   RealURI = URI;

   AutoSelectCompression();
   Init(URI, URIDesc, ShortDesc);

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgIndex with TransactionManager "
                << TransactionManager << std::endl;
}
									/*}}}*/
// AcqIndex::AcqIndex - Constructor					/*{{{*/
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,
                         pkgAcqMetaBase *TransactionManager,
                         IndexTarget const *Target,
			 HashStringList const &ExpectedHash,
                         indexRecords *MetaIndexParser)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHash,
                     MetaIndexParser)
{
   RealURI = Target->URI;

   // autoselect the compression method
   AutoSelectCompression();
   Init(Target->URI, Target->Description, Target->ShortDesc);

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
   if (ExpectedHashes.usable())
   {
      for (std::vector<std::string>::const_iterator t = types.begin();
           t != types.end(); ++t)
      {
         std::string CompressedMetaKey = string(Target->MetaKey).append(".").append(*t);
         if (*t == "uncompressed" ||
             MetaIndexParser->Exists(CompressedMetaKey) == true)
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

   CurrentCompressionExtension = CompressionExtensions.substr(0, CompressionExtensions.find(' '));
   if (CurrentCompressionExtension == "uncompressed")
   {
      Desc.URI = URI;
      if(Target)
         MetaKey = string(Target->MetaKey);
   }
   else
   {
      Desc.URI = URI + '.' + CurrentCompressionExtension;
      DestFile = DestFile + '.' + CurrentCompressionExtension;
      if(Target)
         MetaKey = string(Target->MetaKey) + '.' + CurrentCompressionExtension;
   }

   // load the filesize
   if(MetaIndexParser)
   {
      indexRecords::checkSum *Record = MetaIndexParser->Lookup(MetaKey);
      if(Record)
         FileSize = Record->Size;

      InitByHashIfNeeded(MetaKey);
   }

   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;

   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::AdjustForByHash - modify URI for by-hash support		/*{{{*/
void pkgAcqIndex::InitByHashIfNeeded(const std::string MetaKey)
{
   // TODO:
   //  - (maybe?) add support for by-hash into the sources.list as flag
   //  - make apt-ftparchive generate the hashes (and expire?)
   std::string HostKnob = "APT::Acquire::" + ::URI(Desc.URI).Host + "::By-Hash";
   if(_config->FindB("APT::Acquire::By-Hash", false) == true ||
      _config->FindB(HostKnob, false) == true ||
      MetaIndexParser->GetSupportsAcquireByHash())
   {
      indexRecords::checkSum *Record = MetaIndexParser->Lookup(MetaKey);
      if(Record)
      {
         // FIXME: should we really use the best hash here? or a fixed one?
         const HashString *TargetHash = Record->Hashes.find("");
         std::string ByHash = "/by-hash/" + TargetHash->HashType() + "/" + TargetHash->HashValue();
         size_t trailing_slash = Desc.URI.find_last_of("/");
         Desc.URI = Desc.URI.replace(
            trailing_slash,
            Desc.URI.substr(trailing_slash+1).size()+1,
            ByHash);
      } else {
         _error->Warning(
            "Fetching ByHash requested but can not find record for %s",
            MetaKey.c_str());
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

   return msg;
}
									/*}}}*/
// pkgAcqIndex::Failed - getting the indexfile failed			/*{{{*/
void pkgAcqIndex::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   size_t const nextExt = CompressionExtensions.find(' ');
   if (nextExt != std::string::npos)
   {
      CompressionExtensions = CompressionExtensions.substr(nextExt+1);
      Init(RealURI, Desc.Description, Desc.ShortDesc);
      return;
   }

   // on decompression failure, remove bad versions in partial/
   if (Stage == STAGE_DECOMPRESS_AND_VERIFY)
   {
      unlink(EraseFileName.c_str());
   }

   Item::Failed(Message,Cnf);

   /// cancel the entire transaction
   TransactionManager->AbortTransaction();
}
									/*}}}*/
// pkgAcqIndex::GetFinalFilename - Return the full final file path	/*{{{*/
std::string pkgAcqIndex::GetFinalFilename() const
{
   std::string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(RealURI);
   return GetCompressedFileName(RealURI, FinalFile, CurrentCompressionExtension);
}
									/*}}}*/
// AcqIndex::ReverifyAfterIMS - Reverify index after an ims-hit		/*{{{*/
void pkgAcqIndex::ReverifyAfterIMS()
{
   // update destfile to *not* include the compression extension when doing
   // a reverify (as its uncompressed on disk already)
   DestFile = GetCompressedFileName(RealURI, GetPartialFileNameFromURI(RealURI), CurrentCompressionExtension);

   // copy FinalFile into partial/ so that we check the hash again
   string FinalFile = GetFinalFilename();
   Stage = STAGE_DECOMPRESS_AND_VERIFY;
   Desc.URI = "copy:" + FinalFile;
   QueueURI(Desc);
}
									/*}}}*/
// AcqIndex::ValidateFile - Validate the content of the downloaded file	/*{{{*/
bool pkgAcqIndex::ValidateFile(const std::string &FileName)
{
   // FIXME: this can go away once we only ever download stuff that
   //        has a valid hash and we never do GET based probing
   // FIXME2: this also leaks debian-isms into the code and should go therefore

   /* Always validate the index file for correctness (all indexes must
    * have a Package field) (LP: #346386) (Closes: #627642) 
    */
   FileFd fd(FileName, FileFd::ReadOnly, FileFd::Extension);
   // Only test for correctness if the content of the file is not empty
   // (empty is ok)
   if (fd.Size() > 0)
   {
      pkgTagSection sec;
      pkgTagFile tag(&fd);
      
      // all our current indexes have a field 'Package' in each section
      if (_error->PendingError() == true ||
          tag.Step(sec) == false ||
          sec.Exists("Package") == false)
         return false;
   }
   return true;
}
									/*}}}*/
// AcqIndex::Done - Finished a fetch					/*{{{*/
// ---------------------------------------------------------------------
/* This goes through a number of states.. On the initial fetch the
   method could possibly return an alternate filename which points
   to the uncompressed version of the file. If this is so the file
   is copied into the partial directory. In all other cases the file
   is decompressed with a compressed uri. */
void pkgAcqIndex::Done(string Message,
                       unsigned long long Size,
                       HashStringList const &Hashes,
		       pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message,Size,Hashes,Cfg);

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
void pkgAcqIndex::StageDownloadDone(string Message,
                                    HashStringList const &Hashes,
                                    pkgAcquire::MethodConfig *Cfg)
{
   // First check if the calculcated Hash of the (compressed) downloaded
   // file matches the hash we have in the MetaIndexRecords for this file
   if(VerifyHashByMetaKey(Hashes) == false)
   {
      RenameOnError(HashSumMismatch);
      Failed(Message, Cfg);
      return;
   }

   Complete = true;

   // Handle the unzipd case
   string FileName = LookupTag(Message,"Alt-Filename");
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
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
   }

   // Methods like e.g. "file:" will give us a (compressed) FileName that is
   // not the "DestFile" we set, in this case we uncompress from the local file
   if (FileName != DestFile)
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

   // If we have compressed indexes enabled, queue for hash verification
   if (_config->FindB("Acquire::GzipIndexes",false))
   {
      DestFile = GetPartialFileNameFromURI(RealURI + '.' + CurrentCompressionExtension);
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
// pkgAcqIndex::StageDecompressDone - Final verification		/*{{{*/
void pkgAcqIndex::StageDecompressDone(string Message,
                                      HashStringList const &Hashes,
                                      pkgAcquire::MethodConfig *Cfg)
{
   if (ExpectedHashes.usable() && ExpectedHashes != Hashes)
   {
      Desc.URI = RealURI;
      RenameOnError(HashSumMismatch);
      printHashSumComparision(RealURI, ExpectedHashes, Hashes);
      Failed(Message, Cfg);
      return;
   }

   if(!ValidateFile(DestFile))
   {
      RenameOnError(InvalidFormat);
      Failed(Message, Cfg);
      return;
   }

   // remove the compressed version of the file
   unlink(EraseFileName.c_str());

   // Done, queue for rename on transaction finished
   TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());

   return;
}
									/*}}}*/
// AcqIndexTrans::pkgAcqIndexTrans - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* The Translation file is added to the queue */
pkgAcqIndexTrans::pkgAcqIndexTrans(pkgAcquire *Owner,
			    string URI,string URIDesc,string ShortDesc)
  : pkgAcqIndex(Owner, URI, URIDesc, ShortDesc, HashStringList())
{
}
pkgAcqIndexTrans::pkgAcqIndexTrans(pkgAcquire *Owner,
                                   pkgAcqMetaBase *TransactionManager,
                                   IndexTarget const * const Target,
                                   HashStringList const &ExpectedHashes,
                                   indexRecords *MetaIndexParser)
   : pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser)
{
}
									/*}}}*/
// AcqIndexTrans::Custom600Headers - Insert custom request headers	/*{{{*/
string pkgAcqIndexTrans::Custom600Headers() const
{
   string Final = GetFinalFilename();

   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return "\nFail-Ignore: true\nIndex-File: true";
   return "\nFail-Ignore: true\nIndex-File: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
// AcqIndexTrans::Failed - Silence failure messages for missing files	/*{{{*/
void pkgAcqIndexTrans::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   size_t const nextExt = CompressionExtensions.find(' ');
   if (nextExt != std::string::npos)
   {
      CompressionExtensions = CompressionExtensions.substr(nextExt+1);
      Init(RealURI, Desc.Description, Desc.ShortDesc);
      Status = StatIdle;
      return;
   }

   Item::Failed(Message,Cnf);

   // FIXME: this is used often (e.g. in pkgAcqIndexTrans) so refactor
   if (Cnf->LocalOnly == true ||
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == false)
   {
      // Ignore this
      Status = StatDone;
   }
}
									/*}}}*/
// AcqMetaBase::Add - Add a item to the current Transaction		/*{{{*/
void pkgAcqMetaBase::Add(Item *I)
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
   for (std::vector<Item*>::iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      if(_config->FindB("Debug::Acquire::Transaction", false) == true)
         std::clog << "  Cancel: " << (*I)->DestFile << std::endl;
      // the transaction will abort, so stop anything that is idle
      if ((*I)->Status == pkgAcquire::Item::StatIdle)
         (*I)->Status = pkgAcquire::Item::StatDone;

      // kill failed files in partial
      if ((*I)->Status == pkgAcquire::Item::StatError)
      {
         std::string const PartialFile = GetPartialFileName(flNotDir((*I)->DestFile));
         if(FileExists(PartialFile))
            Rename(PartialFile, PartialFile + ".FAILED");
      }
   }
   Transaction.clear();
}
									/*}}}*/
// AcqMetaBase::TransactionHasError - Check for errors in Transaction	/*{{{*/
bool pkgAcqMetaBase::TransactionHasError()
{
   for (pkgAcquire::ItemIterator I = Transaction.begin();
        I != Transaction.end(); ++I)
      if((*I)->Status != pkgAcquire::Item::StatDone &&
         (*I)->Status != pkgAcquire::Item::StatIdle)
         return true;

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
   for (std::vector<Item*>::iterator I = Transaction.begin();
        I != Transaction.end(); ++I)
   {
      if((*I)->PartialFile != "")
      {
	 if(_config->FindB("Debug::Acquire::Transaction", false) == true)
	    std::clog << "mv " << (*I)->PartialFile << " -> "<< (*I)->DestFile << " "
	       << (*I)->DescURI() << std::endl;

	 Rename((*I)->PartialFile, (*I)->DestFile);
	 ChangeOwnerAndPermissionOfFile("CommitTransaction", (*I)->DestFile.c_str(), "root", "root", 0644);

      } else {
         if(_config->FindB("Debug::Acquire::Transaction", false) == true)
            std::clog << "rm "
                      <<  (*I)->DestFile
                      << " "
                      << (*I)->DescURI()
                      << std::endl;
         unlink((*I)->DestFile.c_str());
      }
      // mark that this transaction is finished
      (*I)->TransactionManager = 0;
   }
   Transaction.clear();
}
									/*}}}*/
// AcqMetaBase::TransactionStageCopy - Stage a file for copying		/*{{{*/
void pkgAcqMetaBase::TransactionStageCopy(Item *I,
                                          const std::string &From,
                                          const std::string &To)
{
   I->PartialFile = From;
   I->DestFile = To;
}
									/*}}}*/
// AcqMetaBase::TransactionStageRemoval - Sage a file for removal	/*{{{*/
void pkgAcqMetaBase::TransactionStageRemoval(Item *I,
                                             const std::string &FinalFile)
{
   I->PartialFile = "";
   I->DestFile = FinalFile; 
}
									/*}}}*/
// AcqMetaBase::GenerateAuthWarning - Check gpg authentication error	/*{{{*/
bool pkgAcqMetaBase::CheckStopAuthentication(const std::string &RealURI,
                                             const std::string &Message)
{
   // FIXME: this entire function can do now that we disallow going to
   //        a unauthenticated state and can cleanly rollback

   string Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);

   if(FileExists(Final))
   {
      Status = StatTransientNetworkError;
      _error->Warning(_("An error occurred during the signature "
                        "verification. The repository is not updated "
                        "and the previous index files will be used. "
                        "GPG error: %s: %s\n"),
                      Desc.Description.c_str(),
                      LookupTag(Message,"Message").c_str());
      RunScripts("APT::Update::Auth-Failure");
      return true;
   } else if (LookupTag(Message,"Message").find("NODATA") != string::npos) {
      /* Invalid signature file, reject (LP: #346386) (Closes: #627642) */
      _error->Error(_("GPG error: %s: %s"),
                    Desc.Description.c_str(),
                    LookupTag(Message,"Message").c_str());
      Status = StatError;
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
// AcqMetaSig::AcqMetaSig - Constructor					/*{{{*/
pkgAcqMetaSig::pkgAcqMetaSig(pkgAcquire *Owner,
                             pkgAcqMetaBase *TransactionManager,
			     string URI,string URIDesc,string ShortDesc,
                             string MetaIndexFile,
			     const vector<IndexTarget*>* IndexTargets,
			     indexRecords* MetaIndexParser) :
   pkgAcqMetaBase(Owner, IndexTargets, MetaIndexParser,
                  HashStringList(), TransactionManager),
   RealURI(URI), MetaIndexFile(MetaIndexFile), URIDesc(URIDesc),
   ShortDesc(ShortDesc)
{
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI);

   // remove any partial downloaded sig-file in partial/.
   // it may confuse proxies and is too small to warrant a
   // partial download anyway
   unlink(DestFile.c_str());

   // set the TransactionManager
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgAcqMetaSig with TransactionManager "
                << TransactionManager << std::endl;

   // Create the item
   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;
   Desc.URI = URI;

   QueueURI(Desc);
}
									/*}}}*/
pkgAcqMetaSig::~pkgAcqMetaSig()						/*{{{*/
{
}
									/*}}}*/
// pkgAcqMetaSig::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaSig::Custom600Headers() const
{
   std::string Header = GetCustom600Headers(RealURI);
   return Header;
}
									/*}}}*/
// pkgAcqMetaSig::Done - The signature was downloaded/verified		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
void pkgAcqMetaSig::Done(string Message,unsigned long long Size,
                         HashStringList const &Hashes,
			 pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message, Size, Hashes, Cfg);

   if(AuthPass == false)
   {
      if(CheckDownloadDone(Message, RealURI) == true)
      {
         // destfile will be modified to point to MetaIndexFile for the
         // gpgv method, so we need to save it here
         MetaIndexFileSignature = DestFile;
         QueueForSignatureVerify(MetaIndexFile, MetaIndexFileSignature);
      }
      return;
   }
   else
   {
      if(CheckAuthDone(Message, RealURI) == true)
      {
         std::string FinalFile = _config->FindDir("Dir::State::lists");
         FinalFile += URItoFileName(RealURI);
         TransactionManager->TransactionStageCopy(this, MetaIndexFileSignature, FinalFile);
      }
   }
}
									/*}}}*/
void pkgAcqMetaSig::Failed(string Message,pkgAcquire::MethodConfig *Cnf)/*{{{*/
{
   string Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);

   // check if we need to fail at this point 
   if (AuthPass == true && CheckStopAuthentication(RealURI, Message))
         return;

   // FIXME: meh, this is not really elegant
   string InReleaseURI = RealURI.replace(RealURI.rfind("Release.gpg"), 12,
                                         "InRelease");
   string FinalInRelease = _config->FindDir("Dir::State::lists") + URItoFileName(InReleaseURI);

   if (RealFileExists(Final) || RealFileExists(FinalInRelease))
   {
      std::string downgrade_msg;
      strprintf(downgrade_msg, _("The repository '%s' is no longer signed."),
                URIDesc.c_str());
      if(_config->FindB("Acquire::AllowDowngradeToInsecureRepositories"))
      {
         // meh, the users wants to take risks (we still mark the packages
         // from this repository as unauthenticated)
         _error->Warning("%s", downgrade_msg.c_str());
         _error->Warning(_("This is normally not allowed, but the option "
                           "Acquire::AllowDowngradeToInsecureRepositories was "
                           "given to override it."));
         
      } else {
         _error->Error("%s", downgrade_msg.c_str());
         Rename(MetaIndexFile, MetaIndexFile+".FAILED");
	 Item::Failed("Message: " + downgrade_msg, Cnf);
         TransactionManager->AbortTransaction();
         return;
      }
   }
   else
      _error->Warning(_("The data from '%s' is not signed. Packages "
	       "from that repository can not be authenticated."),
	    URIDesc.c_str());

   // this ensures that any file in the lists/ dir is removed by the
   // transaction
   DestFile = GetPartialFileNameFromURI(RealURI);
   TransactionManager->TransactionStageRemoval(this, DestFile);

   // only allow going further if the users explicitely wants it
   if(AllowInsecureRepositories(MetaIndexParser, TransactionManager, this) == true)
   {
      // we parse the indexes here because at this point the user wanted
      // a repository that may potentially harm him
      MetaIndexParser->Load(MetaIndexFile);
      QueueIndexes(true);
   }

   Item::Failed(Message,Cnf);

   // FIXME: this is used often (e.g. in pkgAcqIndexTrans) so refactor
   if (Cnf->LocalOnly == true ||
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == false)
   {
      // Ignore this
      Status = StatDone;
   }
}
									/*}}}*/
pkgAcqMetaIndex::pkgAcqMetaIndex(pkgAcquire *Owner,			/*{{{*/
                                 pkgAcqMetaBase *TransactionManager,
				 string URI,string URIDesc,string ShortDesc,
                                 string MetaIndexSigURI,string MetaIndexSigURIDesc, string MetaIndexSigShortDesc,
				 const vector<IndexTarget*>* IndexTargets,
				 indexRecords* MetaIndexParser) :
   pkgAcqMetaBase(Owner, IndexTargets, MetaIndexParser, HashStringList(),
                  TransactionManager), 
   RealURI(URI), URIDesc(URIDesc), ShortDesc(ShortDesc),
   MetaIndexSigURI(MetaIndexSigURI), MetaIndexSigURIDesc(MetaIndexSigURIDesc),
   MetaIndexSigShortDesc(MetaIndexSigShortDesc)
{
   if(TransactionManager == NULL)
   {
      this->TransactionManager = this;
      this->TransactionManager->Add(this);
   }

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgAcqMetaIndex with TransactionManager "
                << this->TransactionManager << std::endl;


   Init(URIDesc, ShortDesc);
}
									/*}}}*/
// pkgAcqMetaIndex::Init - Delayed constructor				/*{{{*/
void pkgAcqMetaIndex::Init(std::string URIDesc, std::string ShortDesc)
{
   DestFile = GetPartialFileNameFromURI(RealURI);

   // Create the item
   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;
   Desc.URI = RealURI;

   // we expect more item
   ExpectedAdditionalItems = IndexTargets->size();
   QueueURI(Desc);
}
									/*}}}*/
// pkgAcqMetaIndex::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaIndex::Custom600Headers() const
{
   return GetCustom600Headers(RealURI);
}
									/*}}}*/
void pkgAcqMetaIndex::Done(string Message,unsigned long long Size,	/*{{{*/
                           HashStringList const &Hashes,
			   pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message,Size,Hashes,Cfg);

   if(CheckDownloadDone(Message, RealURI))
   {
      // we have a Release file, now download the Signature, all further
      // verify/queue for additional downloads will be done in the
      // pkgAcqMetaSig::Done() code
      std::string MetaIndexFile = DestFile;
      new pkgAcqMetaSig(Owner, TransactionManager, 
                        MetaIndexSigURI, MetaIndexSigURIDesc,
                        MetaIndexSigShortDesc, MetaIndexFile, IndexTargets, 
                        MetaIndexParser);

      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
   }
}
									/*}}}*/
bool pkgAcqMetaBase::CheckAuthDone(string Message, const string &RealURI)	/*{{{*/
{
   // At this point, the gpgv method has succeeded, so there is a
   // valid signature from a key in the trusted keyring.  We
   // perform additional verification of its contents, and use them
   // to verify the indexes we are about to download

   if (!MetaIndexParser->Load(DestFile))
   {
      Status = StatAuthError;
      ErrorText = MetaIndexParser->ErrorText;
      return false;
   }

   if (!VerifyVendor(Message, RealURI))
   {
      return false;
   }

   if (_config->FindB("Debug::pkgAcquire::Auth", false))
      std::cerr << "Signature verification succeeded: "
                << DestFile << std::endl;

   // Download further indexes with verification 
   //
   // it would be really nice if we could simply do
   //    if (IMSHit == false) QueueIndexes(true)
   // and skip the download if the Release file has not changed
   // - but right now the list cleaner will needs to be tricked
   //   to not delete all our packages/source indexes in this case
   QueueIndexes(true);

   return true;
}
									/*}}}*/
// pkgAcqMetaBase::GetCustom600Headers - Get header for AcqMetaBase     /*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaBase::GetCustom600Headers(const string &RealURI) const
{
   std::string Header = "\nIndex-File: true";
   std::string MaximumSize;
   strprintf(MaximumSize, "\nMaximum-Size: %i",
             _config->FindI("Acquire::MaxReleaseFileSize", 10*1000*1000));
   Header += MaximumSize;

   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(RealURI);

   struct stat Buf;
   if (stat(FinalFile.c_str(),&Buf) == 0)
      Header += "\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);

   return Header;
}
									/*}}}*/
// pkgAcqMetaBase::QueueForSignatureVerify				/*{{{*/
void pkgAcqMetaBase::QueueForSignatureVerify(const std::string &MetaIndexFile,
                                    const std::string &MetaIndexFileSignature)
{
   AuthPass = true;
   Desc.URI = "gpgv:" + MetaIndexFileSignature;
   DestFile = MetaIndexFile;
   QueueURI(Desc);
   SetActiveSubprocess("gpgv");
}
									/*}}}*/
// pkgAcqMetaBase::CheckDownloadDone					/*{{{*/
bool pkgAcqMetaBase::CheckDownloadDone(const std::string &Message,
                                       const std::string &RealURI)
{
   // We have just finished downloading a Release file (it is not
   // verified yet)

   string FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
      return false;
   }

   if (FileName != DestFile)
   {
      Local = true;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      return false;
   }

   // make sure to verify against the right file on I-M-S hit
   IMSHit = StringToBool(LookupTag(Message,"IMS-Hit"),false);
   if(IMSHit)
   {
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      DestFile = FinalFile;
   }

   // set Item to complete as the remaining work is all local (verify etc)
   Complete = true;

   return true;
}
									/*}}}*/
void pkgAcqMetaBase::QueueIndexes(bool verify)				/*{{{*/
{
   bool transInRelease = false;
   {
      std::vector<std::string> const keys = MetaIndexParser->MetaKeys();
      for (std::vector<std::string>::const_iterator k = keys.begin(); k != keys.end(); ++k)
	 // FIXME: Feels wrong to check for hardcoded string here, but what should we do else…
	 if (k->find("Translation-") != std::string::npos)
	 {
	    transInRelease = true;
	    break;
	 }
   }

   // at this point the real Items are loaded in the fetcher
   ExpectedAdditionalItems = 0;
   for (vector <IndexTarget*>::const_iterator Target = IndexTargets->begin();
        Target != IndexTargets->end();
        ++Target)
   {
      HashStringList ExpectedIndexHashes;
      const indexRecords::checkSum *Record = MetaIndexParser->Lookup((*Target)->MetaKey);
      bool compressedAvailable = false;
      if (Record == NULL)
      {
	 if ((*Target)->IsOptional() == true)
	 {
	    std::vector<std::string> types = APT::Configuration::getCompressionTypes();
	    for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
	       if (MetaIndexParser->Exists((*Target)->MetaKey + "." + *t) == true)
	       {
		  compressedAvailable = true;
		  break;
	       }
	 }
	 else if (verify == true)
	 {
	    Status = StatAuthError;
	    strprintf(ErrorText, _("Unable to find expected entry '%s' in Release file (Wrong sources.list entry or malformed file)"), (*Target)->MetaKey.c_str());
	    return;
	 }
      }
      else
      {
	 ExpectedIndexHashes = Record->Hashes;
	 if (_config->FindB("Debug::pkgAcquire::Auth", false))
	 {
	    std::cerr << "Queueing: " << (*Target)->URI << std::endl
	       << "Expected Hash:" << std::endl;
	    for (HashStringList::const_iterator hs = ExpectedIndexHashes.begin(); hs != ExpectedIndexHashes.end(); ++hs)
	       std::cerr <<  "\t- " << hs->toStr() << std::endl;
	    std::cerr << "For: " << Record->MetaKeyFilename << std::endl;
	 }
	 if (verify == true && ExpectedIndexHashes.empty() == true && (*Target)->IsOptional() == false)
	 {
	    Status = StatAuthError;
	    strprintf(ErrorText, _("Unable to find hash sum for '%s' in Release file"), (*Target)->MetaKey.c_str());
	    return;
	 }
      }

      if ((*Target)->IsOptional() == true)
      {
	 if (transInRelease == false || Record != NULL || compressedAvailable == true)
	 {
	    if (_config->FindB("Acquire::PDiffs",true) == true && transInRelease == true &&
		MetaIndexParser->Exists((*Target)->MetaKey + ".diff/Index") == true)
	       new pkgAcqDiffIndex(Owner, TransactionManager, *Target, ExpectedIndexHashes, MetaIndexParser);
	    else
	       new pkgAcqIndexTrans(Owner, TransactionManager, *Target, ExpectedIndexHashes, MetaIndexParser);
	 }
	 continue;
      }

      /* Queue Packages file (either diff or full packages files, depending
         on the users option) - we also check if the PDiff Index file is listed
         in the Meta-Index file. Ideal would be if pkgAcqDiffIndex would test this
         instead, but passing the required info to it is to much hassle */
      if(_config->FindB("Acquire::PDiffs",true) == true && (verify == false ||
	  MetaIndexParser->Exists((*Target)->MetaKey + ".diff/Index") == true))
	 new pkgAcqDiffIndex(Owner, TransactionManager, *Target, ExpectedIndexHashes, MetaIndexParser);
      else
	 new pkgAcqIndex(Owner, TransactionManager, *Target, ExpectedIndexHashes, MetaIndexParser);
   }
}
									/*}}}*/
bool pkgAcqMetaBase::VerifyVendor(string Message, const string &RealURI)/*{{{*/
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

   string Transformed = MetaIndexParser->GetExpectedDist();

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

   if (_config->FindB("Acquire::Check-Valid-Until", true) == true &&
       MetaIndexParser->GetValidUntil() > 0) {
      time_t const invalid_since = time(NULL) - MetaIndexParser->GetValidUntil();
      if (invalid_since > 0)
	 // TRANSLATOR: The first %s is the URL of the bad Release file, the second is
	 // the time since then the file is invalid - formated in the same way as in
	 // the download progress display (e.g. 7d 3h 42min 1s)
	 return _error->Error(
            _("Release file for %s is expired (invalid since %s). "
              "Updates for this repository will not be applied."),
            RealURI.c_str(), TimeToStr(invalid_since).c_str());
   }

   if (_config->FindB("Debug::pkgAcquire::Auth", false)) 
   {
      std::cerr << "Got Codename: " << MetaIndexParser->GetDist() << std::endl;
      std::cerr << "Expecting Dist: " << MetaIndexParser->GetExpectedDist() << std::endl;
      std::cerr << "Transformed Dist: " << Transformed << std::endl;
   }

   if (MetaIndexParser->CheckDist(Transformed) == false)
   {
      // This might become fatal one day
//       Status = StatAuthError;
//       ErrorText = "Conflicting distribution; expected "
//          + MetaIndexParser->GetExpectedDist() + " but got "
//          + MetaIndexParser->GetDist();
//       return false;
      if (!Transformed.empty())
      {
         _error->Warning(_("Conflicting distribution: %s (expected %s but got %s)"),
                         Desc.Description.c_str(),
                         Transformed.c_str(),
                         MetaIndexParser->GetDist().c_str());
      }
   }

   return true;
}
									/*}}}*/
// pkgAcqMetaIndex::Failed - no Release file present			/*{{{*/
void pkgAcqMetaIndex::Failed(string Message,
                             pkgAcquire::MethodConfig * Cnf)
{
   pkgAcquire::Item::Failed(Message, Cnf);
   Status = StatDone;

   string FinalFile = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);

   _error->Warning(_("The repository '%s' does not have a Release file. "
                     "This is deprecated, please contact the owner of the "
                     "repository."), URIDesc.c_str());

   // No Release file was present so fall
   // back to queueing Packages files without verification
   // only allow going further if the users explicitely wants it
   if(AllowInsecureRepositories(MetaIndexParser, TransactionManager, this) == true)
   {
      // Done, queue for rename on transaction finished
      if (FileExists(DestFile)) 
         TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);

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
pkgAcqMetaClearSig::pkgAcqMetaClearSig(pkgAcquire *Owner,		/*{{{*/
		string const &URI, string const &URIDesc, string const &ShortDesc,
		string const &MetaIndexURI, string const &MetaIndexURIDesc, string const &MetaIndexShortDesc,
		string const &MetaSigURI, string const &MetaSigURIDesc, string const &MetaSigShortDesc,
		const vector<IndexTarget*>* IndexTargets,
		indexRecords* MetaIndexParser) :
   pkgAcqMetaIndex(Owner, NULL, URI, URIDesc, ShortDesc, MetaSigURI, MetaSigURIDesc,MetaSigShortDesc, IndexTargets, MetaIndexParser),
       MetaIndexURI(MetaIndexURI), MetaIndexURIDesc(MetaIndexURIDesc), MetaIndexShortDesc(MetaIndexShortDesc),
       MetaSigURI(MetaSigURI), MetaSigURIDesc(MetaSigURIDesc), MetaSigShortDesc(MetaSigShortDesc)
{
   // index targets + (worst case:) Release/Release.gpg
   ExpectedAdditionalItems = IndexTargets->size() + 2;

}
									/*}}}*/
pkgAcqMetaClearSig::~pkgAcqMetaClearSig()				/*{{{*/
{
}
									/*}}}*/
// pkgAcqMetaClearSig::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
string pkgAcqMetaClearSig::Custom600Headers() const
{
   string Header = GetCustom600Headers(RealURI);
   Header += "\nFail-Ignore: true";
   return Header;
}
									/*}}}*/
// pkgAcqMetaClearSig::Done - We got a file                     	/*{{{*/
// ---------------------------------------------------------------------
void pkgAcqMetaClearSig::Done(std::string Message,unsigned long long /*Size*/,
                              HashStringList const &/*Hashes*/,
                              pkgAcquire::MethodConfig *Cnf)
{
   // if we expect a ClearTextSignature (InRelase), ensure that
   // this is what we get and if not fail to queue a 
   // Release/Release.gpg, see #346386
   if (FileExists(DestFile) && !StartsWithGPGClearTextSignature(DestFile))
   {
      pkgAcquire::Item::Failed(Message, Cnf);
      RenameOnError(NotClearsigned);
      TransactionManager->AbortTransaction();
      return;
   }

   if(AuthPass == false)
   {
      if(CheckDownloadDone(Message, RealURI) == true)
         QueueForSignatureVerify(DestFile, DestFile);
      return;
   }
   else
   {
      if(CheckAuthDone(Message, RealURI) == true)
      {
         string FinalFile = _config->FindDir("Dir::State::lists");
         FinalFile += URItoFileName(RealURI);

         // queue for copy in place
         TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
      }
   }
}
									/*}}}*/
void pkgAcqMetaClearSig::Failed(string Message,pkgAcquire::MethodConfig *Cnf) /*{{{*/
{
   Item::Failed(Message, Cnf);

   // we failed, we will not get additional items from this method
   ExpectedAdditionalItems = 0;

   if (AuthPass == false)
   {
      // Queue the 'old' InRelease file for removal if we try Release.gpg
      // as otherwise the file will stay around and gives a false-auth
      // impression (CVE-2012-0214)
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile.append(URItoFileName(RealURI));
      TransactionManager->TransactionStageRemoval(this, FinalFile);
      Status = StatDone;

      new pkgAcqMetaIndex(Owner, TransactionManager,
			MetaIndexURI, MetaIndexURIDesc, MetaIndexShortDesc,
			MetaSigURI, MetaSigURIDesc, MetaSigShortDesc,
			IndexTargets, MetaIndexParser);
   }
   else
   {
      if(CheckStopAuthentication(RealURI, Message))
         return;

      _error->Warning(_("The data from '%s' is not signed. Packages "
                        "from that repository can not be authenticated."),
                      URIDesc.c_str());

      // No Release file was present, or verification failed, so fall
      // back to queueing Packages files without verification
      // only allow going further if the users explicitely wants it
      if(AllowInsecureRepositories(MetaIndexParser, TransactionManager, this) == true)
      {
	 Status = StatDone;

         /* Always move the meta index, even if gpgv failed. This ensures
          * that PackageFile objects are correctly filled in */
         if (FileExists(DestFile))
         {
            string FinalFile = _config->FindDir("Dir::State::lists");
            FinalFile += URItoFileName(RealURI);
            /* InRelease files become Release files, otherwise
             * they would be considered as trusted later on */
            RealURI = RealURI.replace(RealURI.rfind("InRelease"), 9,
                                      "Release");
            FinalFile = FinalFile.replace(FinalFile.rfind("InRelease"), 9,
                                          "Release");

            // Done, queue for rename on transaction finished
            TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
         }
         QueueIndexes(false);
      }
   }
}
									/*}}}*/
// AcqArchive::AcqArchive - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This just sets up the initial fetch environment and queues the first
   possibilitiy */
pkgAcqArchive::pkgAcqArchive(pkgAcquire *Owner,pkgSourceList *Sources,
			     pkgRecords *Recs,pkgCache::VerIterator const &Version,
			     string &StoreFilename) :
               Item(Owner, HashStringList()), Version(Version), Sources(Sources), Recs(Recs), 
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
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) != 0)
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
      // Ignore not source sources
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) != 0)
	 continue;

      // Try to cross match against the source list
      pkgIndexFile *Index;
      if (Sources->FindIndex(Vf.File(),Index) == false)
	    continue;
      
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
	 {
	    PartialSize = Buf.st_size;
            std::string SandboxUser = _config->Find("APT::Sandbox::User");
	    ChangeOwnerAndPermissionOfFile("pkgAcqArchive::QueueNext",DestFile.c_str(), SandboxUser.c_str(), "root", 0600);
	 }
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
void pkgAcqArchive::Done(string Message,unsigned long long Size, HashStringList const &CalcHashes,
			 pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message, Size, CalcHashes, Cfg);
   
   // Check the size
   if (Size != Version->Size)
   {
      RenameOnError(SizeMismatch);
      return;
   }

   // FIXME: could this empty() check impose *any* sort of security issue?
   if(ExpectedHashes.usable() && ExpectedHashes != CalcHashes)
   {
      RenameOnError(HashSumMismatch);
      printHashSumComparision(DestFile, ExpectedHashes, CalcHashes);
      return;
   }

   // Grab the output filename
   string FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
      return;
   }

   // Reference filename
   if (FileName != DestFile)
   {
      StoreFilename = DestFile = FileName;
      Local = true;
      Complete = true;
      return;
   }

   // Done, move it into position
   string FinalFile = _config->FindDir("Dir::Cache::Archives");
   FinalFile += flNotDir(StoreFilename);
   Rename(DestFile,FinalFile);
   ChangeOwnerAndPermissionOfFile("pkgAcqArchive::Done", FinalFile.c_str(), "root", "root", 0644);
   StoreFilename = DestFile = FinalFile;
   Complete = true;
}
									/*}}}*/
// AcqArchive::Failed - Failure handler					/*{{{*/
// ---------------------------------------------------------------------
/* Here we try other sources */
void pkgAcqArchive::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   ErrorText = LookupTag(Message,"Message");
   
   /* We don't really want to retry on failed media swaps, this prevents 
      that. An interesting observation is that permanent failures are not
      recorded. */
   if (Cnf->Removable == true && 
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
   {
      // Vf = Version.FileList();
      while (Vf.end() == false) ++Vf;
      StoreFilename = string();
      Item::Failed(Message,Cnf);
      return;
   }
   
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
      Item::Failed(Message,Cnf);
   }
}
									/*}}}*/
// AcqArchive::IsTrusted - Determine whether this archive comes from a trusted source /*{{{*/
// ---------------------------------------------------------------------
APT_PURE bool pkgAcqArchive::IsTrusted() const
{
   return Trusted;
}
									/*}}}*/
// AcqArchive::Finished - Fetching has finished, tidy up		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqArchive::Finished()
{
   if (Status == pkgAcquire::Item::StatDone &&
       Complete == true)
      return;
   StoreFilename = string();
}
									/*}}}*/
// AcqFile::pkgAcqFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The file is added to the queue */
pkgAcqFile::pkgAcqFile(pkgAcquire *Owner,string URI, HashStringList const &Hashes,
		       unsigned long long Size,string Dsc,string ShortDesc,
		       const string &DestDir, const string &DestFilename,
                       bool IsIndexFile) :
                       Item(Owner, Hashes), IsIndexFile(IsIndexFile)
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
      {
	 PartialSize = Buf.st_size;
         std::string SandboxUser = _config->Find("APT::Sandbox::User");
	 ChangeOwnerAndPermissionOfFile("pkgAcqFile", DestFile.c_str(), SandboxUser.c_str(), "root", 0600);
      }
   }

   QueueURI(Desc);
}
									/*}}}*/
// AcqFile::Done - Item downloaded OK					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqFile::Done(string Message,unsigned long long Size,HashStringList const &CalcHashes,
		      pkgAcquire::MethodConfig *Cnf)
{
   Item::Done(Message,Size,CalcHashes,Cnf);

   // Check the hash
   if(ExpectedHashes.usable() && ExpectedHashes != CalcHashes)
   {
      RenameOnError(HashSumMismatch);
      printHashSumComparision(DestFile, ExpectedHashes, CalcHashes);
      return;
   }
   
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
	 ErrorText = "Link to " + DestFile + " failure ";
	 Status = StatError;
	 Complete = false;
      }      
   }
}
									/*}}}*/
// AcqFile::Failed - Failure handler					/*{{{*/
// ---------------------------------------------------------------------
/* Here we try other sources */
void pkgAcqFile::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   ErrorText = LookupTag(Message,"Message");
   
   // This is the retry counter
   if (Retries != 0 &&
       Cnf->LocalOnly == false &&
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
   {
      Retries--;
      QueueURI(Desc);
      return;
   }
   
   Item::Failed(Message,Cnf);
}
									/*}}}*/
// AcqIndex::Custom600Headers - Insert custom request headers		/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqFile::Custom600Headers() const
{
   if (IsIndexFile)
      return "\nIndex-File: true";
   return "";
}
									/*}}}*/
