#include <config.h>

#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/cachefile.h>

#include <apt-private/private-output.h>
#include <apt-private/private-sources.h>
#include <apt-private/private-utils.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include <apti18n.h>

/* Interface discussion with donkult (for the future):
  apt [add-{archive,release,component}|edit|change-release|disable]-sources
 and be clever and work out stuff from the Release file
*/

// EditSource - EditSourcesList						/*{{{*/
class APT_HIDDEN ScopedGetLock {
public:
   int fd;
   ScopedGetLock(std::string const &filename) : fd(GetLock(filename)) {}
   ~ScopedGetLock() { close(fd); }
};
bool EditSources(CommandLine &CmdL)
{
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
   else
   {
      FileFd filefd;
      if (filefd.Open(sourceslist, FileFd::Create | FileFd::WriteOnly, FileFd::None, 0644) == false)
	 return false;
   }

   ScopedGetLock lock(sourceslist);
   if (lock.fd < 0)
      return false;

   bool res;
   bool file_changed = false;
   do {
      if (EditFileInSensibleEditor(sourceslist) == false)
	 return false;
      if (before.empty())
      {
	 struct stat St;
	 if (stat(sourceslist.c_str(), &St) == 0 && St.st_size == 0)
	       RemoveFile("edit-sources", sourceslist);
      }
      else if (FileExists(sourceslist) && !before.VerifyFile(sourceslist))
      {
	 file_changed = true;
	 pkgCacheFile::RemoveCaches();
      }
      pkgCacheFile CacheFile;
      res = CacheFile.BuildCaches(nullptr);
      if (res == false || _error->empty(GlobalError::WARNING) == false) {
	 std::string outs;
	 strprintf(outs, _("Failed to parse %s. Edit again? "), sourceslist.c_str());
         // FIXME: should we add a "restore previous" option here?
         if (YnPrompt(outs.c_str(), true) == false)
	 {
	    if (res == false && _error->PendingError() == false)
	    {
	       CacheFile.Close();
	       pkgCacheFile::RemoveCaches();
	       res = CacheFile.BuildCaches(nullptr);
	    }
	    break;
	 }
      }
   } while (res == false);

   if (res == true && file_changed == true)
   {
      ioprintf(
         std::cout, _("Your '%s' file changed, please run 'apt-get update'."),
         sourceslist.c_str());
   }
   return res;
}
									/*}}}*/
