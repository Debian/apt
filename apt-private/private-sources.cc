
#include <apt-pkg/hashes.h>
#include <apti18n.h>

#include "private-output.h"
#include "private-sources.h"
#include "private-utils.h"

// EditSource - EditSourcesList         			        /*{{{*/
// ---------------------------------------------------------------------
bool EditSources(CommandLine &CmdL)
{
   bool res;
   pkgSourceList sl;
   std::string outs;

   // FIXME: suport CmdL.FileList to specify sources.list.d files
   std::string sourceslist = _config->FindFile("Dir::Etc::sourcelist");

   HashString before;
   before.FromFile(sourceslist);

   do {
      EditFileInSensibleEditor(sourceslist);
      _error->PushToStack();
      res = sl.Read(sourceslist);
      if (!res) {
         strprintf(outs, _("Failed to parse %s. Edit again? "),
                   sourceslist.c_str());
         std::cout << outs;
         res = !YnPrompt(true);
      }
      _error->RevertToStack();
   } while (res == false);

   if (!before.VerifyFile(sourceslist)) {
      strprintf(
         outs, _("Your '%s' file changed, please run 'apt-get update'."),
         sourceslist.c_str());
      std::cout << outs << std::endl;
   }

   return true;
}
									/*}}}*/
