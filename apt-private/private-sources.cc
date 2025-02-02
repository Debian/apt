#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-output.h>
#include <apt-private/private-sources.h>
#include <apt-private/private-utils.h>

#include <cstddef>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <apti18n.h>

using std::operator""sv;

/* Interface discussion with donkult (for the future):
  apt [add-{archive,release,component}|edit|change-release|disable]-sources
 and be clever and work out stuff from the Release file
*/

// EditSource - EditSourcesList						/*{{{*/
class APT_HIDDEN ScopedGetLock {
public:
   int fd;
   explicit ScopedGetLock(std::string const &filename) : fd(GetLock(filename)) {}
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
         std::cout, _("Your '%s' file changed, please run 'apt-get update'.\n"),
         sourceslist.c_str());
   }
   return res;
}
									/*}}}*/

template <typename T, typename V>
static auto contains(T const &container, V const &value) -> bool
{
   return std::find(container.begin(), container.end(), value) != container.end();
}
static auto merge(std::vector<std::string> &a, std::vector<std::string> const &b)
{
   for (auto const &e : b)
      if (not contains(a, e))
	 a.push_back(e);
}

static bool Modernize(std::string const &filename) /*{{{*/
{
   auto isMain = filename == _config->FindFile("Dir::Etc::SourceList");
   auto simulate = _config->FindB("APT::Get::Simulate");

   std::cerr << "Modernizing " << filename << "...\n";
   pkgSourceList list;
   if (not list.Read(filename))
      return false;

   struct Entry
   {
      std::set<std::string> types;
      std::vector<std::string> uris;
      std::vector<std::string> suites;
      std::vector<std::string> components;
      std::string signedBy;
      std::map<std::string, std::string> options;
      std::string origin;
      bool merged = false;

      auto Merge(Entry &other) -> bool
      {
	 auto howManyDifferent = (types != other.types) + (uris != other.uris) + (suites != other.suites) + (components != other.components);
	 if (howManyDifferent > 1)
	    return false;
	 if (options != other.options)
	    return false;
	 if (origin != other.origin)
	    return false;
	 if (signedBy.empty() && not other.signedBy.empty() && (uris != other.uris || suites != other.suites))
	    return false;
	 if (not signedBy.empty() && not other.signedBy.empty() && signedBy != other.signedBy)
	    return false;

	 types.insert(other.types.begin(), other.types.end());
	 merge(uris, other.uris);
	 merge(suites, other.suites);
	 merge(components, other.components);
	 if (signedBy.empty())
	    signedBy = other.signedBy;

	 other.merged = true;
	 return true;
      }
   };
   std::vector<Entry> entries;
   for (auto const &meta : list)
   {
      Entry e;

      e.uris.push_back(meta->GetURI());
      e.suites.push_back(meta->GetDist());
      for (auto const &t : meta->GetIndexTargets())
      {
	 e.types.insert(t.Option(IndexTarget::TARGET_OF));
	 if (not contains(e.components, t.Option(IndexTarget::COMPONENT)))
	    e.components.push_back(t.Option(IndexTarget::COMPONENT));
      }

      if (meta->IsTrustedSet())
	 e.options["Trusted"] = "yes";

      std::string err;
      e.signedBy = meta->GetSignedBy();
      meta->Load(&err);
      if (e.signedBy.empty() && not meta->GetOrigin().empty())
      {
	 std::string dir = _config->FindDir("Dir") + std::string{"usr/share/keyrings/"};
	 std::string keyring = std::regex_replace(meta->GetOrigin(), std::regex(" "), "-") + "-archive-keyring.gpg";
	 std::transform(keyring.begin(), keyring.end(), keyring.begin(), tolower);
	 if (FileExists(dir + keyring))
	    e.signedBy = dir + keyring;
      }
      if (auto k = _config->FindDir("Dir::Etc::trustedparts") + flNotDir(std::regex_replace(filename, std::regex("\\.list$"), ".gpg")); FileExists(k))
	 e.signedBy = k;
      if (auto k = _config->FindDir("Dir::Etc::trustedparts") + flNotDir(std::regex_replace(filename, std::regex("\\.list$"), ".asc")); FileExists(k))
	 e.signedBy = k;

      if (isMain && not meta->GetOrigin().empty())
      {
	 constexpr auto bad = "\\|{}[]<>\"^~_=!@#$%^&*"sv;
	 e.origin = meta->GetOrigin();
	 std::transform(e.origin.begin(), e.origin.end(), e.origin.begin(), tolower);
	 std::transform(e.origin.begin(), e.origin.end(), e.origin.begin(), [](char c) -> char
			{ return isspace(c) ? '-' : c; });
	 std::transform(e.origin.begin(), e.origin.end(), e.origin.begin(), [bad](char c) -> char
			{ return bad.find(c) != bad.npos ? '-' : c; });
	 std::replace(e.origin.begin(), e.origin.end(), '/', '-');
      }

      entries.push_back(std::move(e));
   }

   for (bool merged = false; merged;)
   {
      merged = false;
      for (auto it = entries.begin(); it != entries.end(); ++it)
      {
	 for (auto it2 = it + 1; it2 != entries.end(); ++it2)
	    if (not it2->merged)
	       merged |= it->Merge(*it2);
      }
   }

   std::map<std::string, std::ofstream> streams;
   for (auto const &e : entries)
   {
      std::string outname;
      if (not isMain)
	 outname = std::regex_replace(filename, std::regex("\\.list$"), ".sources");
      else if (e.origin.empty())
	 outname = _config->FindDir("Dir::Etc::SourceParts") + "moved-from-main.sources";
      else
	 outname = _config->FindDir("Dir::Etc::SourceParts") + (e.origin) + ".sources";

      if (auto it = streams.find(outname); it == streams.end())
      {
	 if (not simulate)
	    std::cerr << "- Writing " << outname << "\n";
	 streams[outname].open(simulate ? "/dev/stdout" : outname, std::ios::app);
      }
      auto &out = streams[outname];
      if (not out)
	 _error->Warning("Cannot open %s for writing.", outname.c_str());
      if (e.merged)
	 continue;

      if (out.tellp() != 0)
	 out << "\n";

      if (simulate)
	 out << "# Would write to: " << outname << "\n";
      if (isMain)
	 out << "# Modernized from " << filename << "\n";
      out << "Types:";
      for (auto const &t : e.types)
	 out << " " << t;
      out << "\n";
      out << "URIs: " << APT::String::Join(e.uris, " ") << "\n";
      out << "Suites: " << APT::String::Join(e.suites, " ") << "\n";
      out << "Components: " << APT::String::Join(e.components, " ") << "\n";
      out << "Signed-By: " << e.signedBy << "\n";
      for (auto const &[key, value] : e.options)
	 out << key << ": " << value << "\n";
      if (e.signedBy.empty())
	 _error->Warning("Could not determine Signed-By for URIs: %s, Suites: %s", APT::String::Join(e.uris, " ").c_str(), APT::String::Join(e.suites, " ").c_str());
   }

   if (not simulate && rename(filename.c_str(), (filename + ".bak").c_str()) != 0)
      _error->WarningE("rename", "Could not rename %s", filename.c_str());

   _error->DumpErrors();
   std::cerr << "\n";
   return true;
}
									/*}}}*/
bool ModernizeSources(CommandLine &) /*{{{*/
{
   auto main = _config->FindFile("Dir::Etc::SourceList");
   auto parts = _config->FindDir("Dir::Etc::SourceParts");
   std::vector<std::string> files;
   if (FileExists(main))
      files.push_back(main);
   for (auto const &I : GetListOfFilesInDir(parts, std::vector<std::string>{"list", "sources"}, true))
      if (APT::String::Endswith(I, ".list"))
	 files.push_back(I);
   if (files.empty())
   {
      std::cout << "All sources are modern.\n";
      return true;
   }

   // TRANSLATOR: "No" answer printed for a yes/no question if --assume-no is set
   auto no = _("N");

   std::cout << "The following files need modernizing:\n";
   for (auto const &I : files)
      std::cout << "  - " << I << "\n";
   std::cout << "\n"
             << "Modernizing will replace .list files with the new .sources format,\n"
             << "add Signed-By values where they can be determined automatically,\n"
             << "and save the old files into .list.bak files.\n"
             << "\n"
             << "This command supports the 'signed-by' and 'trusted' options. If you\n"
             << "have specified other options inside [] brackets, please transfer them\n"
             << "manually to the output files; see sources.list(5) for a mapping.\n"
             << "\n"
             << "For a simulation, respond " << no << " in the following prompt.\n";

   std::string prompt;
   strprintf(prompt, _("Rewrite %zu sources?"), files.size());
   if (YnPrompt(prompt.c_str(), true) == false)
   {
      _config->Set("APT::Get::Simulate", true);
      std::cout << "Simulating only..." << std::endl;
      // Make it visible.
      usleep(100 * 1000);
   }

   bool good = true;
   for (auto const &I : files)
      good = good && Modernize(I);

   return good;
}
									/*}}}*/
