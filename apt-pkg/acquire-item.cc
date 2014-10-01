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
   Status = StatIdle;
   if(ErrorText == "")
      ErrorText = LookupTag(Message,"Message");
   UsedMirror =  LookupTag(Message,"UsedMirror");
   if (QueueCounter <= 1)
   {
      /* This indicates that the file is not available right now but might
         be sometime later. If we do a retry cycle then this should be
	 retried [CDROMs] */
      if (Cnf->LocalOnly == true &&
	  StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
      {
	 Status = StatIdle;
	 Dequeue();
	 return;
      }

      Status = StatError;
      Dequeue();
   }   

   // report mirror failure back to LP if we actually use a mirror
   string FailReason = LookupTag(Message, "FailReason");
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
   Desc.Description = Target->Description + "/DiffIndex";
   Desc.ShortDesc = Target->ShortDesc;
   Desc.URI = Target->URI + ".diff/Index";

   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Desc.URI);

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
      if(Debug)
	 std::clog << "No index file, local or canceld by user" << std::endl;
      Failed("", NULL);
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
   if(Debug)
      std::clog << "pkgAcqDiffIndex::ParseIndexDiff() " << IndexDiffFile
	 << std::endl;

   pkgTagSection Tags;
   string ServerSha1;
   vector<DiffInfo> available_patches;
   
   FileFd Fd(IndexDiffFile,FileFd::ReadOnly);
   pkgTagFile TF(&Fd);
   if (_error->PendingError() == true)
      return false;

   if(TF.Step(Tags) == true)
   {
      bool found = false;
      DiffInfo d;
      string size;

      string const tmp = Tags.FindS("SHA1-Current");
      std::stringstream ss(tmp);
      ss >> ServerSha1 >> size;
      unsigned long const ServerSize = atol(size.c_str());

      FileFd fd(CurrentPackagesFile, FileFd::ReadOnly);
      SHA1Summation SHA1;
      SHA1.AddFD(fd);
      string const local_sha1 = SHA1.Result();

      if(local_sha1 == ServerSha1)
      {
	 // we have the same sha1 as the server so we are done here
	 if(Debug)
	    std::clog << "Package file is up-to-date" << std::endl;
         // ensure we have no leftovers from previous runs
         std::string Partial = _config->FindDir("Dir::State::lists");
         Partial += "partial/" + URItoFileName(RealURI);
         unlink(Partial.c_str());
	 // list cleanup needs to know that this file as well as the already
	 // present index is ours, so we create an empty diff to save it for us
	 new pkgAcqIndexDiffs(Owner, TransactionManager, Target, 
                              ExpectedHashes, MetaIndexParser, 
                              ServerSha1, available_patches);
	 return true;
      }
      else
      {
	 if(Debug)
	    std::clog << "SHA1-Current: " << ServerSha1 << " and we start at "<< fd.Name() << " " << fd.Size() << " " << local_sha1 << std::endl;

	 // check the historie and see what patches we need
	 string const history = Tags.FindS("SHA1-History");
	 std::stringstream hist(history);
	 while(hist >> d.sha1 >> size >> d.file)
	 {
	    // read until the first match is found
	    // from that point on, we probably need all diffs
	    if(d.sha1 == local_sha1) 
	       found=true;
	    else if (found == false)
	       continue;

	    if(Debug)
	       std::clog << "Need to get diff: " << d.file << std::endl;
	    available_patches.push_back(d);
	 }

	 if (available_patches.empty() == false)
	 {
	    // patching with too many files is rather slow compared to a fast download
	    unsigned long const fileLimit = _config->FindI("Acquire::PDiffs::FileLimit", 0);
	    if (fileLimit != 0 && fileLimit < available_patches.size())
	    {
	       if (Debug)
		  std::clog << "Need " << available_patches.size() << " diffs (Limit is " << fileLimit
			<< ") so fallback to complete download" << std::endl;
	       return false;
	    }

	    // see if the patches are too big
	    found = false; // it was true and it will be true again at the end
	    d = *available_patches.begin();
	    string const firstPatch = d.file;
	    unsigned long patchesSize = 0;
	    std::stringstream patches(Tags.FindS("SHA1-Patches"));
	    while(patches >> d.sha1 >> size >> d.file)
	    {
	       if (firstPatch == d.file)
		  found = true;
	       else if (found == false)
		  continue;

	       patchesSize += atol(size.c_str());
	    }
	    unsigned long const sizeLimit = ServerSize * _config->FindI("Acquire::PDiffs::SizeLimit", 100);
	    if (sizeLimit > 0 && (sizeLimit/100) < patchesSize)
	    {
	       if (Debug)
		  std::clog << "Need " << patchesSize << " bytes (Limit is " << sizeLimit/100
			<< ") so fallback to complete download" << std::endl;
	       return false;
	    }
	 }
      }

      // we have something, queue the next diff
      if(found)
      {
         // FIXME: make this use the method
         PackagesFileReadyInPartial = true;
         std::string Partial = _config->FindDir("Dir::State::lists");
         Partial += "partial/" + URItoFileName(RealURI);

         FileFd From(CurrentPackagesFile, FileFd::ReadOnly);
         FileFd To(Partial, FileFd::WriteEmpty);
         if(CopyFile(From, To) == false)
            return _error->Errno("CopyFile", "failed to copy");

         if(Debug)
            std::cerr << "Done copying " << CurrentPackagesFile
                      << " -> " << Partial
                      << std::endl;

	 // queue the diffs
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
                                 MetaIndexParser,
                                 ServerSha1, available_patches);
         }
         else
	 {
	    std::vector<pkgAcqIndexMergeDiffs*> *diffs = new std::vector<pkgAcqIndexMergeDiffs*>(available_patches.size());
	    for(size_t i = 0; i < available_patches.size(); ++i)
	       (*diffs)[i] = new pkgAcqIndexMergeDiffs(Owner,
                                                       TransactionManager,
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
   }
   
   // Nothing found, report and return false
   // Failing here is ok, if we return false later, the full
   // IndexFile is queued
   if(Debug)
      std::clog << "Can't find a patch in the index file" << std::endl;
   return false;
}
									/*}}}*/
void pkgAcqDiffIndex::Failed(string Message,pkgAcquire::MethodConfig * /*Cnf*/)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex failed: " << Desc.URI << " with " << Message << std::endl
		<< "Falling back to normal index file acquire" << std::endl;

   new pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser);

   Complete = false;
   Status = StatDone;
   Dequeue();
}
									/*}}}*/
void pkgAcqDiffIndex::Done(string Message,unsigned long long Size,HashStringList const &Hashes,	/*{{{*/
			   pkgAcquire::MethodConfig *Cnf)
{
   if(Debug)
      std::clog << "pkgAcqDiffIndex::Done(): " << Desc.URI << std::endl;

   Item::Done(Message, Size, Hashes, Cnf);

   // verify the index target
   if(Target && Target->MetaKey != "" && MetaIndexParser && Hashes.size() > 0)
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
   FinalFile = _config->FindDir("Dir::State::lists")+URItoFileName(RealURI);

   // success in downloading the index
   // rename the index
   FinalFile += string(".IndexDiff");
   if(Debug)
      std::clog << "Renaming: " << DestFile << " -> " << FinalFile 
		<< std::endl;
   Rename(DestFile,FinalFile);
   chmod(FinalFile.c_str(),0644);
   DestFile = FinalFile;

   if(!ParseDiffIndex(DestFile))
      return Failed("", NULL);

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
				   string ServerSha1,
				   vector<DiffInfo> diffs)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser),
     available_patches(diffs), ServerSha1(ServerSha1)
{
   
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Target->URI);

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
      PartialFile = _config->FindDir("Dir::State::lists")+"partial/"+URItoFileName(RealURI);

      DestFile = _config->FindDir("Dir::State::lists");
      DestFile += URItoFileName(RealURI);

      // this happens if we have a up-to-date indexfile
      if(!FileExists(PartialFile))
         PartialFile = DestFile;

      TransactionManager->TransactionStageCopy(this, PartialFile, DestFile);

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
   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += "partial/" + URItoFileName(RealURI);

   if(!FileExists(FinalFile))
   {
      Failed("No FinalFile " + FinalFile + " available", NULL);
      return false;
   }

   FileFd fd(FinalFile, FileFd::ReadOnly);
   SHA1Summation SHA1;
   SHA1.AddFD(fd);
   string local_sha1 = string(SHA1.Result());
   if(Debug)
      std::clog << "QueueNextDiff: " 
		<< FinalFile << " (" << local_sha1 << ")"<<std::endl;


   // final file reached before all patches are applied
   if(local_sha1 == ServerSha1)
   {
      Finish(true);
      return true;
   }

   // remove all patches until the next matching patch is found
   // this requires the Index file to be ordered
   for(vector<DiffInfo>::iterator I=available_patches.begin();
       available_patches.empty() == false &&
	  I != available_patches.end() &&
	  I->sha1 != local_sha1;
       ++I)
   {
      available_patches.erase(I);
   }

   // error checking and falling back if no patch was found
   if(available_patches.empty() == true)
   {
      Failed("No patches available", NULL);
      return false;
   }

   // queue the right diff
   Desc.URI = RealURI + ".diff/" + available_patches[0].file + ".gz";
   Desc.Description = Description + " " + available_patches[0].file + string(".pdiff");
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI + ".diff/" + available_patches[0].file);

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

   string FinalFile;
   FinalFile = _config->FindDir("Dir::State::lists")+"partial/"+URItoFileName(RealURI);

   // success in downloading a diff, enter ApplyDiff state
   if(State == StateFetchDiff)
   {

      // rred excepts the patch as $FinalFile.ed
      Rename(DestFile,FinalFile+".ed");

      if(Debug)
	 std::clog << "Sending to rred method: " << FinalFile << std::endl;

      State = StateApplyDiff;
      Local = true;
      Desc.URI = "rred:" + FinalFile;
      QueueURI(Desc);
      ActiveSubprocess = "rred";
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      Mode = "rred";
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
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
                              ServerSha1, available_patches);
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

   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(Target->URI);

   Debug = _config->FindB("Debug::pkgAcquire::Diffs",false);

   RealURI = Target->URI;
   Desc.Owner = this;
   Description = Target->Description;
   Desc.ShortDesc = Target->ShortDesc;

   Desc.URI = RealURI + ".diff/" + patch.file + ".gz";
   Desc.Description = Description + " " + patch.file + string(".pdiff");
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI + ".diff/" + patch.file);

   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs: " << Desc.URI << std::endl;

   QueueURI(Desc);
}
									/*}}}*/
void pkgAcqIndexMergeDiffs::Failed(string Message,pkgAcquire::MethodConfig * /*Cnf*/)/*{{{*/
{
   if(Debug)
      std::clog << "pkgAcqIndexMergeDiffs failed: " << Desc.URI << " with " << Message << std::endl;
   Complete = false;
   Status = StatDone;
   Dequeue();

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

   string const FinalFile = _config->FindDir("Dir::State::lists") + "partial/" + URItoFileName(RealURI);

   if (State == StateFetchDiff)
   {
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
      ActiveSubprocess = "rred";
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      Mode = "rred";
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
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
            std::string PartialFile = _config->FindDir("Dir::State::lists");
            PartialFile += "partial/" + URItoFileName(RealURI);
	    std::string patch = PartialFile + ".ed." + (*I)->patch.file + ".gz";
            std::cerr << patch << std::endl;
	    unlink(patch.c_str());
      }

      // all set and done
      Complete = true;
      if(Debug)
	 std::clog << "allDone: " << DestFile << "\n" << std::endl;
   }
}
									/*}}}*/
// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The package file is added to the queue and a second class is 
   instantiated to fetch the revision file */   
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,
			 string URI,string URIDesc,string ShortDesc,
			 HashStringList const  &ExpectedHash)
   : pkgAcqBaseIndex(Owner, 0, NULL, ExpectedHash, NULL), RealURI(URI)
{
   AutoSelectCompression();
   Init(URI, URIDesc, ShortDesc);

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgIndex with TransactionManager "
                << TransactionManager << std::endl;
}
									/*}}}*/
// AcqIndex::AcqIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
pkgAcqIndex::pkgAcqIndex(pkgAcquire *Owner,
                         pkgAcqMetaBase *TransactionManager,
                         IndexTarget const *Target,
			 HashStringList const &ExpectedHash, 
                         indexRecords *MetaIndexParser)
   : pkgAcqBaseIndex(Owner, TransactionManager, Target, ExpectedHash, 
                     MetaIndexParser), RealURI(Target->URI)
{
   // autoselect the compression method
   AutoSelectCompression();
   Init(Target->URI, Target->Description, Target->ShortDesc);

   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "New pkgIndex with TransactionManager "
                << TransactionManager << std::endl;
}
									/*}}}*/
// AcqIndex::AutoSelectCompression - Select compression			/*{{{*/
// ---------------------------------------------------------------------
void pkgAcqIndex::AutoSelectCompression()
{
   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   CompressionExtension = "";
   if (ExpectedHashes.usable())
   {
      for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
	 if (*t == "uncompressed" || MetaIndexParser->Exists(string(Target->MetaKey).append(".").append(*t)) == true)
	    CompressionExtension.append(*t).append(" ");
   }
   else
   {
      for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
	 CompressionExtension.append(*t).append(" ");
   }
   if (CompressionExtension.empty() == false)
      CompressionExtension.erase(CompressionExtension.end()-1);
}
// AcqIndex::Init - defered Constructor					/*{{{*/
// ---------------------------------------------------------------------
void pkgAcqIndex::Init(string const &URI, string const &URIDesc, 
                       string const &ShortDesc)
{
   Decompression = false;
   Erase = false;

   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(URI);

   std::string const comprExt = CompressionExtension.substr(0, CompressionExtension.find(' '));
   if (comprExt == "uncompressed")
   {
      Desc.URI = URI;
      if(Target)
         MetaKey = string(Target->MetaKey);
   }
   else
   {
      Desc.URI = URI + '.' + comprExt;
      DestFile = DestFile + '.' + comprExt;
      if(Target)
         MetaKey = string(Target->MetaKey) + '.' + comprExt;
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
// ---------------------------------------------------------------------
/* */
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
// pkgAcqIndex::Failed - getting the indexfile failed    		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqIndex::Failed(string Message,pkgAcquire::MethodConfig *Cnf)	/*{{{*/
{
   size_t const nextExt = CompressionExtension.find(' ');
   if (nextExt != std::string::npos)
   {
      CompressionExtension = CompressionExtension.substr(nextExt+1);
      Init(RealURI, Desc.Description, Desc.ShortDesc);
      return;
   }

   // on decompression failure, remove bad versions in partial/
   if (Decompression && Erase) {
      string s = _config->FindDir("Dir::State::lists") + "partial/";
      s.append(URItoFileName(RealURI));
      unlink(s.c_str());
   }

   Item::Failed(Message,Cnf);

   /// cancel the entire transaction
   TransactionManager->AbortTransaction();
}
									/*}}}*/
// pkgAcqIndex::GetFinalFilename - Return the full final file path      /*{{{*/
// ---------------------------------------------------------------------
/* */
std::string pkgAcqIndex::GetFinalFilename() const
{
   std::string const compExt = CompressionExtension.substr(0, CompressionExtension.find(' '));
   std::string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(RealURI);
   if (_config->FindB("Acquire::GzipIndexes",false) == true)
      FinalFile += '.' + compExt;
   return FinalFile;
}
                                                                       /*}}}*/
// AcqIndex::ReverifyAfterIMS - Reverify index after an ims-hit        /*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqIndex::ReverifyAfterIMS()
{
   std::string const compExt = CompressionExtension.substr(0, CompressionExtension.find(' '));

   // update destfile to *not* include the compression extension when doing
   // a reverify (as its uncompressed on disk already)
   DestFile =  _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI);

   // adjust DestFile if its compressed on disk
   if (_config->FindB("Acquire::GzipIndexes",false) == true)
      DestFile += '.' + compExt;

   // copy FinalFile into partial/ so that we check the hash again
   string FinalFile = GetFinalFilename();
   Decompression = true;
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
void pkgAcqIndex::Done(string Message, unsigned long long Size,
                       HashStringList const &Hashes,
		       pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message,Size,Hashes,Cfg);
   std::string const compExt = CompressionExtension.substr(0, CompressionExtension.find(' '));

   if (Decompression == true)
   {
      if (ExpectedHashes.usable() && ExpectedHashes != Hashes)
      {
         Desc.URI = RealURI;
	 RenameOnError(HashSumMismatch);
	 printHashSumComparision(RealURI, ExpectedHashes, Hashes);
         Failed(Message, Cfg);
         return;
      }

      // FIXME: this can go away once we only ever download stuff that
      //        has a valid hash and we never do GET based probing
      //
      /* Always verify the index file for correctness (all indexes must
       * have a Package field) (LP: #346386) (Closes: #627642) 
       */
      FileFd fd(DestFile, FileFd::ReadOnly, FileFd::Extension);
      // Only test for correctness if the content of the file is not empty
      // (empty is ok)
      if (fd.Size() > 0)
      {
         pkgTagSection sec;
         pkgTagFile tag(&fd);
         
         // all our current indexes have a field 'Package' in each section
         if (_error->PendingError() == true || tag.Step(sec) == false || sec.Exists("Package") == false)
         {
            RenameOnError(InvalidFormat);
            Failed(Message, Cfg);
            return;
         }
      }
       
      // FIXME: can we void the "Erase" bool here as its very non-local?
      std::string CompressedFile = _config->FindDir("Dir::State::lists") + "partial/";
      CompressedFile += URItoFileName(RealURI);
      if(_config->FindB("Acquire::GzipIndexes",false) == false)
         CompressedFile += '.' + compExt;

      // Remove the compressed version.
      if (Erase == true)
	 unlink(CompressedFile.c_str());

      // Done, queue for rename on transaction finished
      TransactionManager->TransactionStageCopy(this, DestFile, GetFinalFilename());

      return;
   }
   
   // FIXME: use the same method to find 
   // check the compressed hash too
   if(MetaKey != "" && Hashes.size() > 0)
   {
      indexRecords::checkSum *Record = MetaIndexParser->Lookup(MetaKey);
      if(Record && Record->Hashes.usable() && Hashes != Record->Hashes)
      {
         RenameOnError(HashSumMismatch);
         printHashSumComparision(RealURI, Record->Hashes, Hashes);
         Failed(Message, Cfg);
         return;
      }
   }

   Erase = false;
   Complete = true;
   
   // Handle the unzipd case
   string FileName = LookupTag(Message,"Alt-Filename");
   if (FileName.empty() == false)
   {
      Decompression = true;
      Local = true;
      DestFile += ".decomp";
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      ActiveSubprocess = "copy";
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      Mode = "copy";
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
      return;
   }

   FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
   }

   if (FileName == DestFile)
      Erase = true;
   else
      Local = true;

   // do not reverify cdrom sources as apt-cdrom may rewrite the Packages
   // file when its doing the indexcopy
   if (RealURI.substr(0,6) == "cdrom:" &&
       StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      return;

   // The files timestamp matches, reverify by copy into partial/
   if (StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
   {
      Erase = false;
      ReverifyAfterIMS();
#if 0 // ???
      // set destfile to the final destfile
      if(_config->FindB("Acquire::GzipIndexes",false) == false)
      {
         DestFile = _config->FindDir("Dir::State::lists") + "partial/";
         DestFile += URItoFileName(RealURI);
      }

      ReverifyAfterIMS(FileName);
#endif
      return;
   }
   string decompProg;

   // If we enable compressed indexes, queue for hash verification
   if (_config->FindB("Acquire::GzipIndexes",false))
   {
      DestFile = _config->FindDir("Dir::State::lists");
      DestFile += URItoFileName(RealURI) + '.' + compExt;

      Decompression = true;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);

      return;
    }

   // get the binary name for your used compression type
   decompProg = _config->Find(string("Acquire::CompressionTypes::").append(compExt),"");
   if(decompProg.empty() == false);
   else if(compExt == "uncompressed")
      decompProg = "copy";
   else {
      _error->Error("Unsupported extension: %s", compExt.c_str());
      return;
   }

   Decompression = true;
   DestFile += ".decomp";
   Desc.URI = decompProg + ":" + FileName;
   QueueURI(Desc);

   ActiveSubprocess = decompProg;
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
// AcqIndexTrans::pkgAcqIndexTrans - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* The Translation file is added to the queue */
pkgAcqIndexTrans::pkgAcqIndexTrans(pkgAcquire *Owner,
			    string URI,string URIDesc,string ShortDesc) 
  : pkgAcqIndex(Owner, URI, URIDesc, ShortDesc, HashStringList())
{
}
									/*}}}*/
pkgAcqIndexTrans::pkgAcqIndexTrans(pkgAcquire *Owner, 
                                   pkgAcqMetaBase *TransactionManager, 
                                   IndexTarget const * const Target,
                                   HashStringList const &ExpectedHashes, 
                                   indexRecords *MetaIndexParser)
   : pkgAcqIndex(Owner, TransactionManager, Target, ExpectedHashes, MetaIndexParser)
{
   // load the filesize
   indexRecords::checkSum *Record = MetaIndexParser->Lookup(string(Target->MetaKey));
   if(Record)
      FileSize = Record->Size;
}
									/*}}}*/
// AcqIndexTrans::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
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
// ---------------------------------------------------------------------
/* */
void pkgAcqIndexTrans::Failed(string Message,pkgAcquire::MethodConfig *Cnf)
{
   size_t const nextExt = CompressionExtension.find(' ');
   if (nextExt != std::string::npos)
   {
      CompressionExtension = CompressionExtension.substr(nextExt+1);
      Init(RealURI, Desc.Description, Desc.ShortDesc);
      Status = StatIdle;
      return;
   }

   // FIXME: this is used often (e.g. in pkgAcqIndexTrans) so refactor
   if (Cnf->LocalOnly == true || 
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == false)
   {      
      // Ignore this
      Status = StatDone;
      Complete = false;
      Dequeue();
      return;
   }

   Item::Failed(Message,Cnf);
}
									/*}}}*/

void pkgAcqMetaBase::Add(Item *I)
{
   Transaction.push_back(I);
}

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

      // kill files in partial
      string PartialFile = _config->FindDir("Dir::State::lists");
      PartialFile += "partial/";
      PartialFile += flNotDir((*I)->DestFile);
      if(FileExists(PartialFile))
         Rename(PartialFile, PartialFile + ".FAILED");
   }
}
									/*}}}*/
bool pkgAcqMetaBase::TransactionHasError()
{
   for (pkgAcquire::ItemIterator I = Transaction.begin();
        I != Transaction.end(); ++I)
      if((*I)->Status != pkgAcquire::Item::StatDone &&
         (*I)->Status != pkgAcquire::Item::StatIdle)
         return true;

   return false;
}
// Acquire::CommitTransaction - Commit a transaction			/*{{{*/
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
            std::clog << "mv " 
                      << (*I)->PartialFile << " -> " 
                      <<  (*I)->DestFile << " " 
                      << (*I)->DescURI()
                      << std::endl;
         Rename((*I)->PartialFile, (*I)->DestFile);
         chmod((*I)->DestFile.c_str(),0644);
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
}

void pkgAcqMetaBase::TransactionStageCopy(Item *I,
                                          const std::string &From,
                                          const std::string &To)
{
   I->PartialFile = From;
   I->DestFile = To;
}

void pkgAcqMetaBase::TransactionStageRemoval(Item *I,
                                             const std::string &FinalFile)
{
   I->PartialFile = "";
   I->DestFile = FinalFile; 
}


                                                                       /*{{{*/
bool pkgAcqMetaBase::GenerateAuthWarning(const std::string &RealURI,
                                         const std::string &Message)
{
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


pkgAcqMetaSig::pkgAcqMetaSig(pkgAcquire *Owner,          		/*{{{*/
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
   DestFile += URItoFileName(URI);

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
/* The only header we use is the last-modified header. */
string pkgAcqMetaSig::Custom600Headers() const
{
   string FinalFile = _config->FindDir("Dir::State::lists");
   FinalFile += URItoFileName(RealURI);

   struct stat Buf;
   if (stat(FinalFile.c_str(),&Buf) != 0)
      return "\nIndex-File: true";

   return "\nIndex-File: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}

void pkgAcqMetaSig::Done(string Message,unsigned long long Size, HashStringList const &Hashes,
			 pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message, Size, Hashes, Cfg);

   string FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
      return;
   }

   if (FileName != DestFile)
   {
      // We have to copy it into place
      Local = true;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      return;
   }

   if(StringToBool(LookupTag(Message,"IMS-Hit"),false) == true)
      IMSHit = true;

   // adjust paths if its a ims-hit
   if(IMSHit)
   {
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
         
      TransactionManager->TransactionStageCopy(this, FinalFile, FinalFile);
   }

   // queue for verify
   if(AuthPass == false)
   {
      AuthPass = true;
      Desc.URI = "gpgv:" + DestFile;
      DestFile = MetaIndexFile;
      QueueURI(Desc);
      return;
   }

   // queue to copy the file in place if it was not a ims hit, on ims
   // hit the file is already at the right place
   if(IMSHit == false)
   {
      PartialFile = _config->FindDir("Dir::State::lists") + "partial/";
      PartialFile += URItoFileName(RealURI);
      
      std::string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);

      TransactionManager->TransactionStageCopy(this, PartialFile, FinalFile);
   }

   // we parse the MetaIndexFile here because at this point we can
   // trust the data
   if(AuthPass == true)
   {
      // load indexes and queue further downloads
      MetaIndexParser->Load(MetaIndexFile);
      QueueIndexes(true);
   }

   Complete = true;
}
									/*}}}*/
void pkgAcqMetaSig::Failed(string Message,pkgAcquire::MethodConfig *Cnf)/*{{{*/
{
   string Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);

   // FIXME: duplicated code from pkgAcqMetaIndex
   if (AuthPass == true)
   {
      bool Stop = GenerateAuthWarning(RealURI, Message);
      if(Stop)
         return;
   }

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
         Status = pkgAcquire::Item::StatError;
         TransactionManager->AbortTransaction();
         return;
      }
   }

   // this ensures that any file in the lists/ dir is removed by the
   // transaction
   DestFile =  _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI);
   TransactionManager->TransactionStageRemoval(this, DestFile);

   // only allow going further if the users explicitely wants it
   if(_config->FindB("Acquire::AllowInsecureRepositories") == true)
   {
      // we parse the indexes here because at this point the user wanted
      // a repository that may potentially harm him
      MetaIndexParser->Load(MetaIndexFile);
      QueueIndexes(true);
   } 
   else 
   {
      _error->Warning("Use --allow-insecure-repositories to force the update");
   }

   // FIXME: this is used often (e.g. in pkgAcqIndexTrans) so refactor
   if (Cnf->LocalOnly == true || 
       StringToBool(LookupTag(Message,"Transient-Failure"),false) == false)
   {      
      // Ignore this
      Status = StatDone;
      Complete = false;
      Dequeue();
      return;
   }
   Item::Failed(Message,Cnf);
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
// pkgAcqMetaIndex::Init - Delayed constructor                         /*{{{*/
void pkgAcqMetaIndex::Init(std::string URIDesc, std::string ShortDesc)
{
   DestFile = _config->FindDir("Dir::State::lists") + "partial/";
   DestFile += URItoFileName(RealURI);

   // Create the item
   Desc.Description = URIDesc;
   Desc.Owner = this;
   Desc.ShortDesc = ShortDesc;
   Desc.URI = RealURI;

   // we expect more item
   ExpectedAdditionalItems = IndexTargets->size();
   QueueURI(Desc);
}
// pkgAcqMetaIndex::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
/* The only header we use is the last-modified header. */
string pkgAcqMetaIndex::Custom600Headers() const
{
   string Final = _config->FindDir("Dir::State::lists");
   Final += URItoFileName(RealURI);
   
   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
      return "\nIndex-File: true";
   
   return "\nIndex-File: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
void pkgAcqMetaIndex::Done(string Message,unsigned long long Size,HashStringList const &Hashes,	/*{{{*/
			   pkgAcquire::MethodConfig *Cfg)
{
   Item::Done(Message,Size,Hashes,Cfg);

   // MetaIndexes are done in two passes: one to download the
   // metaindex with an appropriate method, and a second to verify it
   // with the gpgv method

   if (AuthPass == true)
   {
      AuthDone(Message);

      // all cool, move Release file into place
      Complete = true;
   }
   else
   {
      RetrievalDone(Message);
      if (!Complete)
         // Still more retrieving to do
         return;

      if (SigFile != "")
      {
         // There was a signature file, so pass it to gpgv for
         // verification
         if (_config->FindB("Debug::pkgAcquire::Auth", false))
            std::cerr << "Metaindex acquired, queueing gpg verification ("
                      << SigFile << "," << DestFile << ")\n";
         AuthPass = true;
         Desc.URI = "gpgv:" + SigFile;
         QueueURI(Desc);
	 ActiveSubprocess = "gpgv";
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	 Mode = "gpgv";
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
	 return;
      }
   }

   if (Complete == true)
   {
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      if (SigFile == DestFile)
	 SigFile = FinalFile;

      // queue for copy in place
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
   }
}
									/*}}}*/
void pkgAcqMetaIndex::RetrievalDone(string Message)			/*{{{*/
{
   // We have just finished downloading a Release file (it is not
   // verified yet)

   string FileName = LookupTag(Message,"Filename");
   if (FileName.empty() == true)
   {
      Status = StatError;
      ErrorText = "Method gave a blank filename";
      return;
   }

   if (FileName != DestFile)
   {
      Local = true;
      Desc.URI = "copy:" + FileName;
      QueueURI(Desc);
      return;
   }

   // make sure to verify against the right file on I-M-S hit
   IMSHit = StringToBool(LookupTag(Message,"IMS-Hit"),false);
   if(IMSHit)
   {
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      if (SigFile == DestFile)
      {
	 SigFile = FinalFile;
#if 0
	 // constructor of pkgAcqMetaClearSig moved it out of the way,
	 // now move it back in on IMS hit for the 'old' file
	 string const OldClearSig = DestFile + ".reverify";
	 if (RealFileExists(OldClearSig) == true)
	    Rename(OldClearSig, FinalFile);
#endif
      }
      DestFile = FinalFile;
   }

   // queue a signature
   if(SigFile != DestFile)
      new pkgAcqMetaSig(Owner, TransactionManager, 
                        MetaIndexSigURI, MetaIndexSigURIDesc,
                        MetaIndexSigShortDesc, DestFile, IndexTargets, 
                        MetaIndexParser);

   Complete = true;
}
									/*}}}*/
void pkgAcqMetaIndex::AuthDone(string Message)				/*{{{*/
{
   // At this point, the gpgv method has succeeded, so there is a
   // valid signature from a key in the trusted keyring.  We
   // perform additional verification of its contents, and use them
   // to verify the indexes we are about to download

   if (!MetaIndexParser->Load(DestFile))
   {
      Status = StatAuthError;
      ErrorText = MetaIndexParser->ErrorText;
      return;
   }

   if (!VerifyVendor(Message))
   {
      return;
   }

   if (_config->FindB("Debug::pkgAcquire::Auth", false))
      std::cerr << "Signature verification succeeded: "
                << DestFile << std::endl;

// we ensure this by other means
#if 0 
   // do not trust any previously unverified content that we may have
   string LastGoodSigFile = _config->FindDir("Dir::State::lists").append("partial/").append(URItoFileName(RealURI));
   if (DestFile != SigFile)
      LastGoodSigFile.append(".gpg");
   LastGoodSigFile.append(".reverify");
   if(IMSHit == false && RealFileExists(LastGoodSigFile) == false)
   {
      for (vector <struct IndexTarget*>::const_iterator Target = IndexTargets->begin();
           Target != IndexTargets->end();
           ++Target)
      {
         // remove old indexes
         std::string index = _config->FindDir("Dir::State::lists") +
            URItoFileName((*Target)->URI);
         unlink(index.c_str());
         // and also old gzipindexes
         std::vector<std::string> types = APT::Configuration::getCompressionTypes();
         for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
         {
            index += '.' + (*t);
            unlink(index.c_str());
         }
      }
   }
#endif

   // Download further indexes with verification 
   //
   // it would be really nice if we could simply do
   //    if (IMSHit == false) QueueIndexes(true)
   // and skip the download if the Release file has not changed
   // - but right now the list cleaner will needs to be tricked
   //   to not delete all our packages/source indexes in this case
   QueueIndexes(true);

#if 0
   // is it a clearsigned MetaIndex file?
   if (DestFile == SigFile)
      return;

   // Done, move signature file into position
   string VerifiedSigFile = _config->FindDir("Dir::State::lists") +
      URItoFileName(RealURI) + ".gpg";
   Rename(SigFile,VerifiedSigFile);
   chmod(VerifiedSigFile.c_str(),0644);
#endif
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
bool pkgAcqMetaIndex::VerifyVendor(string Message)			/*{{{*/
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
// pkgAcqMetaIndex::Failed - no Release file present or no signature file present	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMetaIndex::Failed(string Message,
                             pkgAcquire::MethodConfig * /*Cnf*/)
{
   string Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);

   if (AuthPass == true)
   {
      bool Stop = GenerateAuthWarning(RealURI, Message);
      if(Stop)
         return;
   }

   /* Always move the meta index, even if gpgv failed. This ensures
    * that PackageFile objects are correctly filled in */
   if (FileExists(DestFile)) 
   {
      string FinalFile = _config->FindDir("Dir::State::lists");
      FinalFile += URItoFileName(RealURI);
      /* InRelease files become Release files, otherwise
       * they would be considered as trusted later on */
      if (SigFile == DestFile) {
	 RealURI = RealURI.replace(RealURI.rfind("InRelease"), 9,
	                               "Release");
	 FinalFile = FinalFile.replace(FinalFile.rfind("InRelease"), 9,
	                               "Release");
	 SigFile = FinalFile;
      }

      // Done, queue for rename on transaction finished
      TransactionManager->TransactionStageCopy(this, DestFile, FinalFile);
   }

   _error->Warning(_("The data from '%s' is not signed. Packages "
                     "from that repository can not be authenticated."),
                   URIDesc.c_str());

   // No Release file was present, or verification failed, so fall
   // back to queueing Packages files without verification
   // only allow going further if the users explicitely wants it
   if(_config->FindB("Acquire::AllowInsecureRepositories") == true)
   {
      QueueIndexes(false);
   } else {
      // warn if the repository is unsinged
      _error->Warning("Use --allow-insecure-repositories to force the update");
   } 
}
									/*}}}*/

void pkgAcqMetaIndex::Finished()
{
   if(_config->FindB("Debug::Acquire::Transaction", false) == true)
      std::clog << "Finished: " << DestFile <<std::endl;
   if(TransactionManager != NULL &&
      TransactionManager->TransactionHasError() == false)
      TransactionManager->CommitTransaction();
}


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
   SigFile = DestFile;

   // index targets + (worst case:) Release/Release.gpg
   ExpectedAdditionalItems = IndexTargets->size() + 2;

#if 0
   // keep the old InRelease around in case of transistent network errors
   string const Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);
   if (RealFileExists(Final) == true)
   {
      string const LastGoodSig = DestFile + ".reverify";
      Rename(Final,LastGoodSig);
   }
#endif
}
									/*}}}*/
pkgAcqMetaClearSig::~pkgAcqMetaClearSig()				/*{{{*/
{
#if 0
   // if the file was never queued undo file-changes done in the constructor
   if (QueueCounter == 1 && Status == StatIdle && FileSize == 0 && Complete == false)
   {
      string const Final = _config->FindDir("Dir::State::lists") + URItoFileName(RealURI);
      string const LastGoodSig = DestFile + ".reverify";
      if (RealFileExists(Final) == false && RealFileExists(LastGoodSig) == true)
	 Rename(LastGoodSig, Final);
   }
#endif
}
									/*}}}*/
// pkgAcqMetaClearSig::Custom600Headers - Insert custom request headers	/*{{{*/
// ---------------------------------------------------------------------
// FIXME: this can go away once the InRelease file is used widely
string pkgAcqMetaClearSig::Custom600Headers() const
{
   string Final = _config->FindDir("Dir::State::lists");
   Final += URItoFileName(RealURI);

   struct stat Buf;
   if (stat(Final.c_str(),&Buf) != 0)
   {
      if (stat(Final.c_str(),&Buf) != 0)
	 return "\nIndex-File: true\nFail-Ignore: true\n";
   }

   return "\nIndex-File: true\nFail-Ignore: true\nLast-Modified: " + TimeRFC1123(Buf.st_mtime);
}
									/*}}}*/
// pkgAcqMetaClearSig::Done - We got a file                     	/*{{{*/
// ---------------------------------------------------------------------
void pkgAcqMetaClearSig::Done(std::string Message,unsigned long long Size, 
                              HashStringList const &Hashes,
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
   pkgAcqMetaIndex::Done(Message, Size, Hashes, Cnf);
}
									/*}}}*/
void pkgAcqMetaClearSig::Failed(string Message,pkgAcquire::MethodConfig *Cnf) /*{{{*/
{
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

      new pkgAcqMetaIndex(Owner, TransactionManager,
			MetaIndexURI, MetaIndexURIDesc, MetaIndexShortDesc,
			MetaSigURI, MetaSigURIDesc, MetaSigShortDesc,
			IndexTargets, MetaIndexParser);
      if (Cnf->LocalOnly == true ||
	  StringToBool(LookupTag(Message, "Transient-Failure"), false) == false)
	 Dequeue();
   }
   else
      pkgAcqMetaIndex::Failed(Message, Cnf);
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

   Complete = true;

   // Reference filename
   if (FileName != DestFile)
   {
      StoreFilename = DestFile = FileName;
      Local = true;
      return;
   }
   
   // Done, move it into position
   string FinalFile = _config->FindDir("Dir::Cache::Archives");
   FinalFile += flNotDir(StoreFilename);
   Rename(DestFile,FinalFile);
   
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
	 PartialSize = Buf.st_size;
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
