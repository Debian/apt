// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>

#include <apt-private/private-cmndline.h>

#include <vector>
#include <stdarg.h>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

APT_SENTINEL static bool strcmp_match_in_list(char const * const Cmd, ...)		/*{{{*/
{
   va_list args;
   bool found = false;
   va_start(args, Cmd);
   char const * Match = NULL;
   while ((Match = va_arg(args, char const *)) != NULL)
   {
      if (strcmp(Cmd, Match) != 0)
	 continue;
      found = true;
      break;
   }
   va_end(args);
   return found;
}
									/*}}}*/
#define addArg(w,x,y,z) Args.push_back(CommandLine::MakeArgs(w,x,y,z))
#define CmdMatches(...) strcmp_match_in_list(Cmd, __VA_ARGS__, NULL)
static bool addArgumentsAPTCache(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("depends", "rdepends", "xvcg", "dotty"))
   {
      addArg('i', "important", "APT::Cache::Important", 0);
      addArg(0, "installed", "APT::Cache::Installed", 0);
      addArg(0, "pre-depends", "APT::Cache::ShowPre-Depends", 0);
      addArg(0, "depends", "APT::Cache::ShowDepends", 0);
      addArg(0, "recommends", "APT::Cache::ShowRecommends", 0);
      addArg(0, "suggests", "APT::Cache::ShowSuggests", 0);
      addArg(0, "replaces", "APT::Cache::ShowReplaces", 0);
      addArg(0, "breaks", "APT::Cache::ShowBreaks", 0);
      addArg(0, "conflicts", "APT::Cache::ShowConflicts", 0);
      addArg(0, "enhances", "APT::Cache::ShowEnhances", 0);
      addArg(0, "recurse", "APT::Cache::RecurseDepends", 0);
   }
   else if (CmdMatches("search"))
   {
      addArg('n', "names-only", "APT::Cache::NamesOnly", 0);
      addArg('f', "full", "APT::Cache::ShowFull", 0);
   }
   else if (CmdMatches("show"))
   {
      addArg('a', "all-versions", "APT::Cache::AllVersions", 0);
   }
   else if (CmdMatches("pkgnames"))
   {
      addArg(0, "all-names", "APT::Cache::AllNames", 0);
   }
   else if (CmdMatches("unmet"))
   {
      addArg('i', "important", "APT::Cache::Important", 0);
   }
   else if (CmdMatches("gencaches", "showsrc", "showpkg", "stats", "dump",
	    "dumpavail", "showauto", "policy", "madison"))
      ;
   else
      return false;

   // FIXME: move to the correct command(s)
   addArg('g', "generate", "APT::Cache::Generate", 0);
   addArg('t', "target-release", "APT::Default-Release", CommandLine::HasArg);
   addArg('t', "default-release", "APT::Default-Release", CommandLine::HasArg);

   addArg('p', "pkg-cache", "Dir::Cache::pkgcache", CommandLine::HasArg);
   addArg('s', "src-cache", "Dir::Cache::srcpkgcache", CommandLine::HasArg);
   return true;
}
									/*}}}*/
static bool addArgumentsAPTCDROM(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("add", "ident") == false)
      return false;

   // FIXME: move to the correct command(s)
   addArg(0, "auto-detect", "Acquire::cdrom::AutoDetect", CommandLine::Boolean);
   addArg('d', "cdrom", "Acquire::cdrom::mount", CommandLine::HasArg);
   addArg('r', "rename", "APT::CDROM::Rename", 0);
   addArg('m', "no-mount", "APT::CDROM::NoMount", 0);
   addArg('f', "fast", "APT::CDROM::Fast", 0);
   addArg('n', "just-print", "APT::CDROM::NoAct", 0);
   addArg('n', "recon", "APT::CDROM::NoAct", 0);
   addArg('n', "no-act", "APT::CDROM::NoAct", 0);
   addArg('a', "thorough", "APT::CDROM::Thorough", 0);
   return true;
}
									/*}}}*/
static bool addArgumentsAPTConfig(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("dump"))
   {
      addArg(0,"empty","APT::Config::Dump::EmptyValue",CommandLine::Boolean);
      addArg(0,"format","APT::Config::Dump::Format",CommandLine::HasArg);
   }
   else if (CmdMatches("shell"))
      ;
   else
      return false;

   return true;
}
									/*}}}*/
static bool addArgumentsAPTGet(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("install", "remove", "purge", "upgrade", "dist-upgrade",
	    "dselect-upgrade", "autoremove", "full-upgrade"))
   {
      addArg(0, "show-progress", "DpkgPM::Progress", 0);
      addArg('f', "fix-broken", "APT::Get::Fix-Broken", 0);
      addArg(0, "purge", "APT::Get::Purge", 0);
      addArg('V',"verbose-versions","APT::Get::Show-Versions",0);
      addArg(0, "auto-remove", "APT::Get::AutomaticRemove", 0);
      addArg(0, "reinstall", "APT::Get::ReInstall", 0);
      addArg(0, "solver", "APT::Solver", CommandLine::HasArg);
      if (CmdMatches("upgrade"))
      {
         addArg(0, "new-pkgs", "APT::Get::Upgrade-Allow-New", 
                CommandLine::Boolean);
      }
   }
   else if (CmdMatches("update"))
   {
      addArg(0, "list-cleanup", "APT::Get::List-Cleanup", 0);
   }
   else if (CmdMatches("source"))
   {
      addArg('b', "compile", "APT::Get::Compile", 0);
      addArg('b', "build", "APT::Get::Compile", 0);
      addArg('P', "build-profiles", "APT::Build-Profiles", CommandLine::HasArg);
      addArg(0, "diff-only", "APT::Get::Diff-Only", 0);
      addArg(0, "debian-only", "APT::Get::Diff-Only", 0);
      addArg(0, "tar-only", "APT::Get::Tar-Only", 0);
      addArg(0, "dsc-only", "APT::Get::Dsc-Only", 0);
   }
   else if (CmdMatches("build-dep"))
   {
      addArg('a', "host-architecture", "APT::Get::Host-Architecture", CommandLine::HasArg);
      addArg('P', "build-profiles", "APT::Build-Profiles", CommandLine::HasArg);
      addArg(0, "purge", "APT::Get::Purge", 0);
      addArg(0, "solver", "APT::Solver", CommandLine::HasArg);
      // this has no effect *but* sbuild is using it (see LP: #1255806)
      // once sbuild is fixed, this option can be removed
      addArg('f', "fix-broken", "APT::Get::Fix-Broken", 0);
   }
   else if (CmdMatches("clean", "autoclean", "check", "download", "changelog") ||
	    CmdMatches("markauto", "unmarkauto")) // deprecated commands
      ;
   else if (CmdMatches("moo"))
      addArg(0, "color", "APT::Moo::Color", 0);

   if (CmdMatches("install", "remove", "purge", "upgrade", "dist-upgrade",
	    "deselect-upgrade", "autoremove", "clean", "autoclean", "check",
	    "build-dep", "full-upgrade"))
   {
      addArg('s', "simulate", "APT::Get::Simulate", 0);
      addArg('s', "just-print", "APT::Get::Simulate", 0);
      addArg('s', "recon", "APT::Get::Simulate", 0);
      addArg('s', "dry-run", "APT::Get::Simulate", 0);
      addArg('s', "no-act", "APT::Get::Simulate", 0);
   }

   // FIXME: move to the correct command(s)
   addArg('d',"download-only","APT::Get::Download-Only",0);
   addArg('y',"yes","APT::Get::Assume-Yes",0);
   addArg('y',"assume-yes","APT::Get::Assume-Yes",0);
   addArg(0,"assume-no","APT::Get::Assume-No",0);
   addArg('u',"show-upgraded","APT::Get::Show-Upgraded",0);
   addArg('m',"ignore-missing","APT::Get::Fix-Missing",0);
   addArg('t',"target-release","APT::Default-Release",CommandLine::HasArg);
   addArg('t',"default-release","APT::Default-Release",CommandLine::HasArg);
   addArg(0,"download","APT::Get::Download",0);
   addArg(0,"fix-missing","APT::Get::Fix-Missing",0);
   addArg(0,"ignore-hold","APT::Ignore-Hold",0);
   addArg(0,"upgrade","APT::Get::upgrade",0);
   addArg(0,"only-upgrade","APT::Get::Only-Upgrade",0);
   addArg(0,"force-yes","APT::Get::force-yes",0);
   addArg(0,"print-uris","APT::Get::Print-URIs",0);
   addArg(0,"trivial-only","APT::Get::Trivial-Only",0);
   addArg(0,"remove","APT::Get::Remove",0);
   addArg(0,"only-source","APT::Get::Only-Source",0);
   addArg(0,"arch-only","APT::Get::Arch-Only",0);
   addArg(0,"allow-unauthenticated","APT::Get::AllowUnauthenticated",0);
   addArg(0,"install-recommends","APT::Install-Recommends",CommandLine::Boolean);
   addArg(0,"install-suggests","APT::Install-Suggests",CommandLine::Boolean);
   addArg(0,"fix-policy","APT::Get::Fix-Policy-Broken",0);

   return true;
}
									/*}}}*/
static bool addArgumentsAPTMark(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("auto", "manual", "hold", "unhold", "showauto",
	    "showmanual", "showhold", "showholds", "install",
	    "markauto", "unmarkauto"))
      ;
   else
      return false;

   addArg('v',"verbose","APT::MarkAuto::Verbose",0);
   addArg('s',"simulate","APT::Mark::Simulate",0);
   addArg('s',"just-print","APT::Mark::Simulate",0);
   addArg('s',"recon","APT::Mark::Simulate",0);
   addArg('s',"dry-run","APT::Mark::Simulate",0);
   addArg('s',"no-act","APT::Mark::Simulate",0);
   addArg('f',"file","Dir::State::extended_states",CommandLine::HasArg);

   return true;
}
									/*}}}*/
static bool addArgumentsAPT(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("list"))
   {
      addArg(0,"installed","APT::Cmd::Installed",0);
      addArg(0,"upgradable","APT::Cmd::Upgradable",0);
      addArg(0,"manual-installed","APT::Cmd::Manual-Installed",0);
      addArg('v', "verbose", "APT::Cmd::List-Include-Summary", 0);
      addArg('a', "all-versions", "APT::Cmd::All-Versions", 0);
   }
   else if (CmdMatches("show"))
   {
      addArg('a', "all-versions", "APT::Cache::AllVersions", 0);
   }
   else if (addArgumentsAPTGet(Args, Cmd) || addArgumentsAPTCache(Args, Cmd))
   {
       // we have no (supported) command-name overlaps so far, so we call
       // specifics in order until we find one which adds arguments
   }
   else
      return false;

   return true;
}
									/*}}}*/
std::vector<CommandLine::Args> getCommandArgs(char const * const Program, char const * const Cmd)/*{{{*/
{
   std::vector<CommandLine::Args> Args;
   Args.reserve(50);
   if (Program == NULL || Cmd == NULL)
      ; // FIXME: Invalid command supplied
   else if (strcmp(Cmd, "help") == 0)
      ; // no options for help so no need to implement it in each
   else if (strcmp(Program, "apt-get") == 0)
      addArgumentsAPTGet(Args, Cmd);
   else if (strcmp(Program, "apt-cache") == 0)
      addArgumentsAPTCache(Args, Cmd);
   else if (strcmp(Program, "apt-cdrom") == 0)
      addArgumentsAPTCDROM(Args, Cmd);
   else if (strcmp(Program, "apt-config") == 0)
      addArgumentsAPTConfig(Args, Cmd);
   else if (strcmp(Program, "apt-mark") == 0)
      addArgumentsAPTMark(Args, Cmd);
   else if (strcmp(Program, "apt") == 0)
      addArgumentsAPT(Args, Cmd);

   // options without a command
   addArg('h', "help", "help", 0);
   addArg('v', "version", "version", 0);
   // general options
   addArg('q', "quiet", "quiet", CommandLine::IntLevel);
   addArg('q', "silent", "quiet", CommandLine::IntLevel);
   addArg('c', "config-file", 0, CommandLine::ConfigFile);
   addArg('o', "option", 0, CommandLine::ArbItem);
   addArg(0, NULL, NULL, 0);

   return Args;
}
									/*}}}*/
#undef CmdMatches
#undef addArg
