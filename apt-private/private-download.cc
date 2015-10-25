// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-output.h>
#include <apt-private/private-download.h>
#include <apt-private/acqprogress.h>

#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <errno.h>

#include <apti18n.h>
									/*}}}*/

// CheckAuth - check if each download comes form a trusted source	/*{{{*/
bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser)
{
   std::vector<std::string> UntrustedList;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd(); ++I)
      if (!(*I)->IsTrusted())
	 UntrustedList.push_back((*I)->ShortDesc());

   if (UntrustedList.empty())
      return true;

   return AuthPrompt(UntrustedList, PromptUser);
}
									/*}}}*/
bool AuthPrompt(std::vector<std::string> const &UntrustedList, bool const PromptUser)/*{{{*/
{
   ShowList(c2out,_("WARNING: The following packages cannot be authenticated!"), UntrustedList,
	 [](std::string const&) { return true; },
	 [](std::string const&str) { return str; },
	 [](std::string const&) { return ""; });

   if (_config->FindB("APT::Get::AllowUnauthenticated",false) == true)
   {
      c2out << _("Authentication warning overridden.\n");
      return true;
   }

   if (PromptUser == false)
      return _error->Error(_("Some packages could not be authenticated"));

   if (_config->FindI("quiet",0) < 2
       && _config->FindB("APT::Get::Assume-Yes",false) == false)
   {
      c2out << _("Install these packages without verification?") << std::flush;
      if (!YnPrompt(false))
         return _error->Error(_("Some packages could not be authenticated"));

      return true;
   }
   else if (_config->FindB("APT::Get::Force-Yes",false) == true) {
      _error->Warning(_("--force-yes is deprecated, use one of the options starting with --allow instead."));
      return true;
   }

   return _error->Error(_("There were unauthenticated packages and -y was used without --allow-unauthenticated"));
}
									/*}}}*/
bool AcquireRun(pkgAcquire &Fetcher, int const PulseInterval, bool * const Failure, bool * const TransientNetworkFailure)/*{{{*/
{
   pkgAcquire::RunResult res;
   if(PulseInterval > 0)
      res = Fetcher.Run(PulseInterval);
   else
      res = Fetcher.Run();

   if (res == pkgAcquire::Failed)
      return false;

   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin();
	I != Fetcher.ItemsEnd(); ++I)
   {

      if ((*I)->Status == pkgAcquire::Item::StatDone &&
	    (*I)->Complete == true)
	 continue;

      if (TransientNetworkFailure != NULL && (*I)->Status == pkgAcquire::Item::StatIdle)
      {
	 *TransientNetworkFailure = true;
	 continue;
      }

      ::URI uri((*I)->DescURI());
      uri.User.clear();
      uri.Password.clear();
      std::string descUri = std::string(uri);
      _error->Error(_("Failed to fetch %s  %s\n"), descUri.c_str(),
	    (*I)->ErrorText.c_str());

      if (Failure != NULL)
	 *Failure = true;
   }

   return true;
}
									/*}}}*/
bool CheckFreeSpaceBeforeDownload(std::string const &Dir, unsigned long long FetchBytes)/*{{{*/
{
   uint32_t const RAMFS_MAGIC = 0x858458f6;
   /* Check for enough free space, but only if we are actually going to
      download */
   if (_config->FindB("APT::Get::Print-URIs", false) == true ||
       _config->FindB("APT::Get::Download", true) == false)
      return true;

   struct statvfs Buf;
   if (statvfs(Dir.c_str(),&Buf) != 0) {
      if (errno == EOVERFLOW)
	 return _error->WarningE("statvfs",_("Couldn't determine free space in %s"),
	       Dir.c_str());
      else
	 return _error->Errno("statvfs",_("Couldn't determine free space in %s"),
	       Dir.c_str());
   }
   else
   {
      unsigned long long const FreeBlocks = _config->Find("APT::Sandbox::User").empty() ? Buf.f_bfree : Buf.f_bavail;
      if (FreeBlocks < (FetchBytes / Buf.f_bsize))
      {
	 struct statfs Stat;
	 if (statfs(Dir.c_str(),&Stat) != 0
#if HAVE_STRUCT_STATFS_F_TYPE
	       || Stat.f_type != RAMFS_MAGIC
#endif
	    )
	    return _error->Error(_("You don't have enough free space in %s."),
		  Dir.c_str());
      }
   }
   return true;
}
									/*}}}*/

aptAcquireWithTextStatus::aptAcquireWithTextStatus() : pkgAcquire::pkgAcquire(),
   Stat(std::cout, ScreenWidth, _config->FindI("quiet",0))
{
   SetLog(&Stat);
}
