
#include <apt-pkg/hashes.h>
#include <apti18n.h>

#include "private-output.h"
#include "private-sources.h"
#include "private-utils.h"

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
   std::string outs;

   std::string sourceslist;
   if (CmdL.FileList[1] != NULL)
      sourceslist = _config->FindDir("Dir::Etc::sourceparts") + CmdL.FileList[1] + ".list";
   else
      sourceslist = _config->FindFile("Dir::Etc::sourcelist");

   HashString before;
   if (FileExists(sourceslist))
       before.FromFile(sourceslist);

   do {
      EditFileInSensibleEditor(sourceslist);
      _error->PushToStack();
      res = sl.Read(sourceslist);
      if (!res) {
         _error->DumpErrors();
         strprintf(outs, _("Failed to parse %s. Edit again? "),
                   sourceslist.c_str());
         std::cout << outs;
         res = !YnPrompt(true);
      }
      _error->RevertToStack();
   } while (res == false);

   if (FileExists(sourceslist) && !before.VerifyFile(sourceslist)) {
      strprintf(
         outs, _("Your '%s' file changed, please run 'apt-get update'."),
         sourceslist.c_str());
      std::cout << outs << std::endl;
   }

   return true;
}
									/*}}}*/
