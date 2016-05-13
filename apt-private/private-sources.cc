#include <config.h>

#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <apt-private/private-output.h>
#include <apt-private/private-sources.h>
#include <apt-private/private-utils.h>

#include <stddef.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include <apti18n.h>

/* Interface discussion with donkult (for the future):
  apt [add-{archive,release,component}|edit|change-release|disable]-sources 
 and be clever and work out stuff from the Release file
*/

// EditSource - EditSourcesList         			        /*{{{*/
// ---------------------------------------------------------------------
bool EditSources(CommandLine &CmdL)
{
   bool res;
   pkgSourceList sl;

   std::string sourceslist;
   if (CmdL.FileList[1] != NULL)
   {
      sourceslist = _config->FindDir("Dir::Etc::sourceparts") + CmdL.FileList[1];
      if (!APT::String::Endswith(sourceslist, ".list"))
         sourceslist += ".list";
   } else {
      sourceslist = _config->FindFile("Dir::Etc::sourcelist");
   }
   HashString before;
   if (FileExists(sourceslist))
       before.FromFile(sourceslist);

   int lockfd = GetLock(sourceslist);
   if (lockfd < 0)
      return false;

   do {
      EditFileInSensibleEditor(sourceslist);
      _error->PushToStack();
      res = sl.Read(sourceslist);
      if (!res) {
	 std::string outs;
	 strprintf(outs, _("Failed to parse %s. Edit again? "), sourceslist.c_str());
         // FIXME: should we add a "restore previous" option here?
         res = !YnPrompt(outs.c_str(), true);
      }
      _error->RevertToStack();
   } while (res == false);
   close(lockfd);

   if (FileExists(sourceslist) && !before.VerifyFile(sourceslist)) {
      ioprintf(
         std::cout, _("Your '%s' file changed, please run 'apt-get update'."),
         sourceslist.c_str());
   }

   return true;
}
									/*}}}*/
