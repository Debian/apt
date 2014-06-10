// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-output.h>
#include <apt-private/private-download.h>

#include <fstream>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

// CheckAuth - check if each download comes form a trusted source	/*{{{*/
bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser)
{
   std::string UntrustedList;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd(); ++I)
      if (!(*I)->IsTrusted())
          UntrustedList += std::string((*I)->ShortDesc()) + " ";

   if (UntrustedList == "")
      return true;

   return AuthPrompt(UntrustedList, PromptUser);
}

bool AuthPrompt(std::string UntrustedList, bool const PromptUser)
{
   ShowList(c2out,_("WARNING: The following packages cannot be authenticated!"),UntrustedList,"");

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
   else if (_config->FindB("APT::Get::Force-Yes",false) == true)
      return true;

   return _error->Error(_("There are problems and -y was used without --force-yes"));
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
