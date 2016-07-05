// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-get.cc,v 1.156 2004/08/28 01:05:16 mdz Exp $
/* ######################################################################
   
   apt-get - Cover for dpkg
   
   This is an allout cover for dpkg implementing a safer front end. It is
   based largely on libapt-pkg.

   The syntax is different, 
      apt-get [opt] command [things]
   Where command is:
      update - Resyncronize the package files from their sources
      upgrade - Smart-Download the newest versions of all packages
      dselect-upgrade - Follows dselect's changes to the Status: field
                       and installes new and removes old packages
      dist-upgrade - Powerful upgrader designed to handle the issues with
                    a new distribution.
      install - Download and install a given package (by name, not by .deb)
      check - Update the package cache and check for broken packages
      clean - Erase the .debs downloaded to /var/cache/apt/archives and
              the partial dir too

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/clean.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/init.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/sptr.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-download.h>
#include <apt-private/private-install.h>
#include <apt-private/private-main.h>
#include <apt-private/private-moo.h>
#include <apt-private/private-output.h>
#include <apt-private/private-update.h>
#include <apt-private/private-upgrade.h>
#include <apt-private/private-utils.h>
#include <apt-private/private-source.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

using namespace std;

/* mark packages as automatically/manually installed.			{{{*/
static bool DoMarkAuto(CommandLine &CmdL)
{
   bool Action = true;
   int AutoMarkChanged = 0;
   OpTextProgress progress;
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;

   if (strcasecmp(CmdL.FileList[0],"markauto") == 0)
      Action = true;
   else if (strcasecmp(CmdL.FileList[0],"unmarkauto") == 0)
      Action = false;

   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      const char *S = *I;
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
      if (Pkg.end() == true) {
         return _error->Error(_("Couldn't find package %s"),S);
      }
      else
      {
         if (!Action)
            ioprintf(c1out,_("%s set to manually installed.\n"), Pkg.Name());
         else
            ioprintf(c1out,_("%s set to automatically installed.\n"),
                      Pkg.Name());

         Cache->MarkAuto(Pkg,Action);
         AutoMarkChanged++;
      }
   }

   _error->Notice(_("This command is deprecated. Please use 'apt-mark auto' and 'apt-mark manual' instead."));

   if (AutoMarkChanged && ! _config->FindB("APT::Get::Simulate",false))
      return Cache->writeStateFile(NULL);
   return false;
}
									/*}}}*/
// DoDSelectUpgrade - Do an upgrade by following dselects selections	/*{{{*/
// ---------------------------------------------------------------------
/* Follows dselect's selections */
static bool DoDSelectUpgrade(CommandLine &)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;
   
   pkgDepCache::ActionGroup group(Cache);

   // Install everything with the install flag set
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; ++I)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,false);
   }

   /* Now install their deps too, if we do this above then order of
      the status file is significant for | groups */
   for (I = Cache->PkgBegin();I.end() != true; ++I)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,true);
   }
   
   // Apply erasures now, they override everything else.
   for (I = Cache->PkgBegin();I.end() != true; ++I)
   {
      // Remove packages 
      if (I->SelectedState == pkgCache::State::DeInstall ||
	  I->SelectedState == pkgCache::State::Purge)
	 Cache->MarkDelete(I,I->SelectedState == pkgCache::State::Purge);
   }

   /* Resolve any problems that dselect created, allupgrade cannot handle
      such things. We do so quite aggressively too.. */
   if (Cache->BrokenCount() != 0)
   {      
      pkgProblemResolver Fix(Cache);

      // Hold back held packages.
      if (_config->FindB("APT::Ignore-Hold",false) == false)
      {
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); I.end() == false; ++I)
	 {
	    if (I->SelectedState == pkgCache::State::Hold)
	    {
	       Fix.Protect(I);
	       Cache->MarkKeep(I);
	    }
	 }
      }
   
      if (Fix.Resolve() == false)
      {
	 ShowBroken(c1out,Cache,false);
	 return _error->Error(_("Internal error, problem resolver broke stuff"));
      }
   }

   // Now upgrade everything
   if (APT::Upgrade::Upgrade(Cache, APT::Upgrade::FORBID_REMOVE_PACKAGES | APT::Upgrade::FORBID_INSTALL_NEW_PACKAGES) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, problem resolver broke stuff"));
   }
   
   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoCheck - Perform the check operation				/*{{{*/
// ---------------------------------------------------------------------
/* Opening automatically checks the system, this command is mostly used
   for debugging */
static bool DoCheck(CommandLine &)
{
   CacheFile Cache;
   Cache.Open();
   Cache.CheckDeps();
   
   return true;
}
									/*}}}*/
// DoIndexTargets - Lists all IndexTargets				/*{{{*/
static std::string format_key(std::string key)
{
   // deb822 is case-insensitive, but the human eye prefers candy
   std::transform(key.begin(), key.end(), key.begin(), ::tolower);
   key[0] = ::toupper(key[0]);
   size_t found = key.find("_uri");
   if (found != std::string::npos)
      key.replace(found, 4, "-URI");
   while ((found = key.find('_')) != std::string::npos)
   {
      key[found] = '-';
      key[found + 1] = ::toupper(key[found + 1]);
   }
   return key;
}
static bool DoIndexTargets(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgSourceList *SrcList = CacheFile.GetSourceList();
   pkgCache *Cache = CacheFile.GetPkgCache();

   if (SrcList == nullptr || Cache == nullptr)
      return false;

   std::string const Format = _config->Find("APT::Get::IndexTargets::Format");
   bool const ReleaseInfo = _config->FindB("APT::Get::IndexTargets::ReleaseInfo", true);
   bool Filtered = CmdL.FileSize() > 1;
   for (pkgSourceList::const_iterator S = SrcList->begin(); S != SrcList->end(); ++S)
   {
      std::vector<IndexTarget> const targets = (*S)->GetIndexTargets();
      std::map<std::string, string> AddOptions;
      if (ReleaseInfo)
      {
	 AddOptions.insert(std::make_pair("TRUSTED", ((*S)->IsTrusted() ? "yes" : "no")));
	 pkgCache::RlsFileIterator const RlsFile = (*S)->FindInCache(*Cache, false);
	 if (RlsFile.end())
	    continue;
#define APT_RELEASE(X,Y) if (RlsFile.Y() != NULL) AddOptions.insert(std::make_pair(X, RlsFile.Y()))
	 APT_RELEASE("CODENAME", Codename);
	 APT_RELEASE("SUITE", Archive);
	 APT_RELEASE("VERSION", Version);
	 APT_RELEASE("ORIGIN", Origin);
	 APT_RELEASE("LABEL", Label);
#undef APT_RELEASE
      }

      for (std::vector<IndexTarget>::const_iterator T = targets.begin(); T != targets.end(); ++T)
      {
	 std::string filename = T->Option(ReleaseInfo ? IndexTarget::EXISTING_FILENAME : IndexTarget::FILENAME);
	 if (filename.empty())
	    continue;

	 std::ostringstream stanza;
	 if (Filtered || Format.empty())
	 {
	    stanza << "MetaKey: " << T->MetaKey << "\n"
	       << "ShortDesc: " << T->ShortDesc << "\n"
	       << "Description: " << T->Description << "\n"
	       << "URI: " << T->URI << "\n"
	       << "Filename: " << filename << "\n"
	       << "Optional: " << (T->IsOptional ? "yes" : "no") << "\n"
	       << "KeepCompressed: " << (T->KeepCompressed ? "yes" : "no") << "\n";
	    for (std::map<std::string,std::string>::const_iterator O = AddOptions.begin(); O != AddOptions.end(); ++O)
	       stanza << format_key(O->first) << ": " << O->second << "\n";
	    for (std::map<std::string,std::string>::const_iterator O = T->Options.begin(); O != T->Options.end(); ++O)
	    {
	       if (O->first == "PDIFFS")
		  stanza << "PDiffs: " << O->second << "\n";
	       else if (O->first == "COMPRESSIONTYPES")
		  stanza << "CompressionTypes: " << O->second << "\n";
	       else if (O->first == "KEEPCOMPRESSEDAS")
		  stanza << "KeepCompressedAs: " << O->second << "\n";
	       else if (O->first == "DEFAULTENABLED")
		  stanza << "DefaultEnabled: " << O->second << "\n";
	       else
		  stanza << format_key(O->first) << ": " << O->second << "\n";
	    }
	    stanza << "\n";

	    if (Filtered)
	    {
	       // that is a bit crude, but good enough for now
	       bool found = true;
	       std::string haystack = std::string("\n") + stanza.str() + "\n";
	       std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
	       size_t const filesize = CmdL.FileSize() - 1;
	       for (size_t i = 0; i != filesize; ++i)
	       {
		  std::string needle = std::string("\n") + CmdL.FileList[i + 1] + "\n";
		  std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
		  if (haystack.find(needle) != std::string::npos)
		     continue;
		  found = false;
		  break;
	       }
	       if (found == false)
		  continue;
	    }
	 }

	 if (Format.empty())
	    cout << stanza.str();
	 else
	 {
	    std::string out = SubstVar(Format, "$(FILENAME)", filename);
	    out = T->Format(out);
	    for (std::map<std::string,std::string>::const_iterator O = AddOptions.begin(); O != AddOptions.end(); ++O)
	       out = SubstVar(out, std::string("$(") + O->first + ")", O->second);
	    cout << out << std::endl;
	 }
      }
   }

   return true;
}
									/*}}}*/
static bool ShowHelp(CommandLine &)					/*{{{*/
{
   if (_config->FindB("version") == true)
   {
      cout << _("Supported modules:") << endl;

      for (unsigned I = 0; I != pkgVersioningSystem::GlobalListLen; I++)
      {
	 pkgVersioningSystem *VS = pkgVersioningSystem::GlobalList[I];
	 if (_system != 0 && _system->VS == VS)
	    cout << '*';
	 else
	    cout << ' ';
	 cout << "Ver: " << VS->Label << endl;

	 /* Print out all the packaging systems that will work with 
	    this VS */
	 for (unsigned J = 0; J != pkgSystem::GlobalListLen; J++)
	 {
	    pkgSystem *Sys = pkgSystem::GlobalList[J];
	    if (_system == Sys)
	       cout << '*';
	    else
	       cout << ' ';
	    if (Sys->VS->TestCompatibility(*VS) == true)
	       cout << "Pkg:  " << Sys->Label << " (Priority " << Sys->Score(*_config) << ")" << endl;
	 }
      }

      for (unsigned I = 0; I != pkgSourceList::Type::GlobalListLen; I++)
      {
	 pkgSourceList::Type *Type = pkgSourceList::Type::GlobalList[I];
	 cout << " S.L: '" << Type->Name << "' " << Type->Label << endl;
      }

      for (unsigned I = 0; I != pkgIndexFile::Type::GlobalListLen; I++)
      {
	 pkgIndexFile::Type *Type = pkgIndexFile::Type::GlobalList[I];
	 cout << " Idx: " << Type->Label << endl;
      }

      return true;
   }

   std::cout <<
      _("Usage: apt-get [options] command\n"
	    "       apt-get [options] install|remove pkg1 [pkg2 ...]\n"
	    "       apt-get [options] source pkg1 [pkg2 ...]\n"
	    "\n"
	    "apt-get is a command line interface for retrieval of packages\n"
	    "and information about them from authenticated sources and\n"
	    "for installation, upgrade and removal of packages together\n"
	    "with their dependencies.\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {"update", &DoUpdate, _("Retrieve new lists of packages")},
      {"upgrade", &DoUpgrade, _("Perform an upgrade")},
      {"install", &DoInstall, _("Install new packages (pkg is libc6 not libc6.deb)")},
      {"remove", &DoInstall, _("Remove packages")},
      {"purge", &DoInstall, _("Remove packages and config files")},
      {"autoremove", &DoInstall, _("Remove automatically all unused packages")},
      {"auto-remove", &DoInstall, nullptr},
      {"markauto", &DoMarkAuto, nullptr},
      {"unmarkauto", &DoMarkAuto, nullptr},
      {"dist-upgrade", &DoDistUpgrade, _("Distribution upgrade, see apt-get(8)")},
      {"full-upgrade", &DoDistUpgrade, nullptr},
      {"dselect-upgrade", &DoDSelectUpgrade, _("Follow dselect selections")},
      {"build-dep", &DoBuildDep, _("Configure build-dependencies for source packages")},
      {"clean", &DoClean, _("Erase downloaded archive files")},
      {"autoclean", &DoAutoClean, _("Erase old downloaded archive files")},
      {"auto-clean", &DoAutoClean, nullptr},
      {"check", &DoCheck, _("Verify that there are no broken dependencies")},
      {"source", &DoSource, _("Download source archives")},
      {"download", &DoDownload, _("Download the binary package into the current directory")},
      {"changelog", &DoChangelog, _("Download and display the changelog for the given package")},
      {"indextargets", &DoIndexTargets, nullptr},
      {"moo", &DoMoo, nullptr},
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   // Parse the command line and initialize the package library
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_GET, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);

   InitSignals();
   InitOutput();

   CheckIfSimulateMode(CmdL);

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/
