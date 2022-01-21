// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-main.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iomanip>
#include <vector>

#include <apti18n.h>
									/*}}}*/

APT_NONNULL(1, 2)
static bool CmdMatches_fn(char const *const Cmd, char const *const Match)
{
   return strcmp(Cmd, Match) == 0;
}
template <typename... Tail>
APT_NONNULL(1, 2)
static bool CmdMatches_fn(char const *const Cmd, char const *const Match, Tail... MoreMatches)
{
   return CmdMatches_fn(Cmd, Match) || CmdMatches_fn(Cmd, MoreMatches...);
}
#define addArg(w, x, y, z) Args.emplace_back(CommandLine::MakeArgs(w, x, y, z))
#define CmdMatches(...) (Cmd != nullptr && CmdMatches_fn(Cmd, __VA_ARGS__))

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
      addArg(0, "implicit", "APT::Cache::ShowImplicit", 0);
   }
   else if (CmdMatches("search"))
   {
      addArg('n', "names-only", "APT::Cache::NamesOnly", 0);
      addArg('f', "full", "APT::Cache::ShowFull", 0);
   }
   else if (CmdMatches("show") | CmdMatches("info"))
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
   else if (CmdMatches("showsrc"))
   {
      addArg(0,"only-source","APT::Cache::Only-Source",0);
   }
   else if (CmdMatches("gencaches", "showpkg", "stats", "dump",
	    "dumpavail", "showauto", "policy", "madison"))
      ;
   else
      return false;

   bool const found_something = Args.empty() == false;

   // FIXME: move to the correct command(s)
   addArg('g', "generate", "APT::Cache::Generate", 0);
   addArg('t', "target-release", "APT::Default-Release", CommandLine::HasArg);
   addArg('t', "default-release", "APT::Default-Release", CommandLine::HasArg);

   addArg('p', "pkg-cache", "Dir::Cache::pkgcache", CommandLine::HasArg);
   addArg('s', "src-cache", "Dir::Cache::srcpkgcache", CommandLine::HasArg);
   addArg(0, "with-source", "APT::Sources::With::", CommandLine::HasArg);

   return found_something;
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
static bool addArgumentsAPTDumpSolver(std::vector<CommandLine::Args> &Args, char const * const)/*{{{*/
{
   addArg(0,"user","APT::Solver::RunAsUser",CommandLine::HasArg);
   return true;
}
									/*}}}*/
static bool addArgumentsAPTExtractTemplates(std::vector<CommandLine::Args> &Args, char const * const)/*{{{*/
{
   addArg('t',"tempdir","APT::ExtractTemplates::TempDir",CommandLine::HasArg);
   return true;
}
									/*}}}*/
static bool addArgumentsAPTFTPArchive(std::vector<CommandLine::Args> &Args, char const * const)/*{{{*/
{
   addArg(0,"md5","APT::FTPArchive::MD5",0);
   addArg(0,"sha1","APT::FTPArchive::SHA1",0);
   addArg(0,"sha256","APT::FTPArchive::SHA256",0);
   addArg(0,"sha512","APT::FTPArchive::SHA512",0);
   addArg('d',"db","APT::FTPArchive::DB",CommandLine::HasArg);
   addArg('s',"source-override","APT::FTPArchive::SourceOverride",CommandLine::HasArg);
   addArg(0,"delink","APT::FTPArchive::DeLinkAct",0);
   addArg(0,"readonly","APT::FTPArchive::ReadOnlyDB",0);
   addArg(0,"contents","APT::FTPArchive::Contents",0);
   addArg('a',"arch","APT::FTPArchive::Architecture",CommandLine::HasArg);
   return true;
}
									/*}}}*/
static bool addArgumentsAPTInternalPlanner(std::vector<CommandLine::Args> &, char const * const)/*{{{*/
{
   return true;
}
									/*}}}*/
static bool addArgumentsAPTInternalSolver(std::vector<CommandLine::Args> &, char const * const)/*{{{*/
{
   return true;
}
									/*}}}*/
static bool addArgumentsAPTHelper(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("cat-file"))
   {
      addArg('C', "compress", "Apt-Helper::Cat-File::Compress",CommandLine::HasArg);
   }
   return true;
}
									/*}}}*/
static bool addArgumentsAPTGet(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("install", "reinstall", "remove", "purge", "upgrade", "dist-upgrade",
	    "dselect-upgrade", "autoremove", "autopurge", "full-upgrade"))
   {
      addArg(0, "show-progress", "DpkgPM::Progress", 0);
      addArg('f', "fix-broken", "APT::Get::Fix-Broken", 0);
      addArg(0, "purge", "APT::Get::Purge", 0);
      addArg('V',"verbose-versions","APT::Get::Show-Versions",0);
      addArg(0, "autoremove", "APT::Get::AutomaticRemove", 0);
      addArg(0, "auto-remove", "APT::Get::AutomaticRemove", 0);
      addArg(0, "reinstall", "APT::Get::ReInstall", 0);
      addArg(0, "solver", "APT::Solver", CommandLine::HasArg);
      addArg(0, "planner", "APT::Planner", CommandLine::HasArg);
      if (CmdMatches("upgrade"))
      {
         addArg(0, "new-pkgs", "APT::Get::Upgrade-Allow-New", 
                CommandLine::Boolean);
      }
   }
   else if (CmdMatches("update"))
   {
      addArg(0, "list-cleanup", "APT::Get::List-Cleanup", 0);
      addArg(0, "allow-insecure-repositories", "Acquire::AllowInsecureRepositories", 0);
      addArg(0, "allow-weak-repositories", "Acquire::AllowWeakRepositories", 0);
      addArg(0, "allow-releaseinfo-change", "Acquire::AllowReleaseInfoChange", 0);
      addArg(0, "allow-releaseinfo-change-origin", "Acquire::AllowReleaseInfoChange::Origin", 0);
      addArg(0, "allow-releaseinfo-change-label", "Acquire::AllowReleaseInfoChange::Label", 0);
      addArg(0, "allow-releaseinfo-change-version", "Acquire::AllowReleaseInfoChange::Version", 0);
      addArg(0, "allow-releaseinfo-change-codename", "Acquire::AllowReleaseInfoChange::Codename", 0);
      addArg(0, "allow-releaseinfo-change-suite", "Acquire::AllowReleaseInfoChange::Suite", 0);
      addArg(0, "allow-releaseinfo-change-defaultpin", "Acquire::AllowReleaseInfoChange::DefaultPin", 0);
      addArg('e', "error-on", "APT::Update::Error-Mode", CommandLine::HasArg);
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
   else if (CmdMatches("build-dep") || CmdMatches("satisfy"))
   {
      addArg('a', "host-architecture", "APT::Get::Host-Architecture", CommandLine::HasArg);
      addArg('P', "build-profiles", "APT::Build-Profiles", CommandLine::HasArg);
      addArg(0, "purge", "APT::Get::Purge", 0);
      addArg(0, "solver", "APT::Solver", CommandLine::HasArg);
      if (CmdMatches("build-dep"))
      {
         addArg(0,"arch-only","APT::Get::Arch-Only",0);
         addArg(0,"indep-only","APT::Get::Indep-Only",0);
      }
      // this has no effect *but* sbuild is using it (see LP: #1255806)
      // once sbuild is fixed, this option can be removed
      addArg('f', "fix-broken", "APT::Get::Fix-Broken", 0);
   }
   else if (CmdMatches("indextargets"))
   {
      addArg(0,"format","APT::Get::IndexTargets::Format", CommandLine::HasArg);
      addArg(0,"release-info","APT::Get::IndexTargets::ReleaseInfo", 0);
   }
   else if (CmdMatches("clean", "autoclean", "auto-clean", "check", "download", "changelog") ||
	    CmdMatches("markauto", "unmarkauto")) // deprecated commands
      ;
   else if (CmdMatches("moo"))
      addArg(0, "color", "APT::Moo::Color", 0);

   if (CmdMatches("install", "reinstall", "remove", "purge", "upgrade", "dist-upgrade",
	    "dselect-upgrade", "autoremove", "auto-remove", "autopurge", "clean", "autoclean", "auto-clean", "check",
	    "build-dep", "satisfy", "full-upgrade", "source"))
   {
      addArg('s', "simulate", "APT::Get::Simulate", 0);
      addArg('s', "just-print", "APT::Get::Simulate", 0);
      addArg('s', "recon", "APT::Get::Simulate", 0);
      addArg('s', "dry-run", "APT::Get::Simulate", 0);
      addArg('s', "no-act", "APT::Get::Simulate", 0);
   }

   bool const found_something = Args.empty() == false;

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
   addArg(0,"allow-change-held-packages","APT::Get::allow-change-held-packages",CommandLine::Boolean);
   addArg(0,"allow-remove-essential","APT::Get::allow-remove-essential",CommandLine::Boolean);
   addArg(0,"allow-downgrades","APT::Get::allow-downgrades",CommandLine::Boolean);
   addArg(0,"force-yes","APT::Get::force-yes",0);
   addArg(0,"print-uris","APT::Get::Print-URIs",0);
   addArg(0,"trivial-only","APT::Get::Trivial-Only",0);
   addArg(0,"mark-auto","APT::Get::Mark-Auto",0);
   addArg(0,"remove","APT::Get::Remove",0);
   addArg(0,"only-source","APT::Get::Only-Source",0);
   addArg(0,"allow-unauthenticated","APT::Get::AllowUnauthenticated",0);
   addArg(0,"install-recommends","APT::Install-Recommends",CommandLine::Boolean);
   addArg(0,"install-suggests","APT::Install-Suggests",CommandLine::Boolean);
   addArg(0,"fix-policy","APT::Get::Fix-Policy-Broken",0);
   addArg(0, "with-source", "APT::Sources::With::", CommandLine::HasArg);

   return found_something;
}
									/*}}}*/
static bool addArgumentsAPTMark(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("auto", "manual", "hold", "unhold", "showauto",
	    "showmanual", "showhold", "showholds", "showheld",
	    "markauto", "unmarkauto", "minimize-manual"))
   {
      addArg('f',"file","Dir::State::extended_states",CommandLine::HasArg);
   }
   else if (CmdMatches("install", "reinstall", "remove", "deinstall", "purge",
	    "showinstall", "showinstalls", "showremove", "showremoves",
	    "showdeinstall", "showdeinstalls", "showpurge", "showpurges"))
      ;
   else
      return false;

   if (CmdMatches("markauto", "unmarkauto"))
   {
      addArg('v',"verbose","APT::MarkAuto::Verbose",0);
   }

   if (CmdMatches("minimize-manual"))
   {
      addArg('y',"yes","APT::Get::Assume-Yes",0);
      addArg('y',"assume-yes","APT::Get::Assume-Yes",0);
      addArg(0,"assume-no","APT::Get::Assume-No",0);
   }

   if (CmdMatches("minimize-manual") || (Cmd != nullptr && strncmp(Cmd, "show", strlen("show")) != 0))
   {
      addArg('s',"simulate","APT::Mark::Simulate",0);
      addArg('s',"just-print","APT::Mark::Simulate",0);
      addArg('s',"recon","APT::Mark::Simulate",0);
      addArg('s',"dry-run","APT::Mark::Simulate",0);
      addArg('s',"no-act","APT::Mark::Simulate",0);
   }
   addArg(0, "with-source", "APT::Sources::With::", CommandLine::HasArg);

   return true;
}
									/*}}}*/
static bool addArgumentsAPTSortPkgs(std::vector<CommandLine::Args> &Args, char const * const)/*{{{*/
{
   addArg('s',"source","APT::SortPkgs::Source",0);
   return true;
}
									/*}}}*/
static bool addArgumentsAPT(std::vector<CommandLine::Args> &Args, char const * const Cmd)/*{{{*/
{
   if (CmdMatches("list"))
   {
      addArg('i',"installed","APT::Cmd::Installed",0);
      addArg(0,"upgradeable","APT::Cmd::Upgradable",0);
      addArg('u',"upgradable","APT::Cmd::Upgradable",0);
      addArg(0,"manual-installed","APT::Cmd::Manual-Installed",0);
      addArg('v', "verbose", "APT::Cmd::List-Include-Summary", 0);
      addArg('a', "all-versions", "APT::Cmd::All-Versions", 0);
   }
   else if (CmdMatches("show") || CmdMatches("info"))
   {
      addArg('a', "all-versions", "APT::Cache::AllVersions", 0);
      addArg('f', "full", "APT::Cache::ShowFull", 0);
   }
   else if (addArgumentsAPTGet(Args, Cmd) || addArgumentsAPTCache(Args, Cmd))
   {
       // we have no (supported) command-name overlaps so far, so we call
       // specifics in order until we find one which adds arguments
   }
   else
      return false;

   addArg(0, "with-source", "APT::Sources::With::", CommandLine::HasArg);

   return true;
}
									/*}}}*/
static bool addArgumentsRred(std::vector<CommandLine::Args> &Args, char const * const /*Cmd*/)/*{{{*/
{
   addArg('t', nullptr, "Rred::T", 0);
   addArg('f', nullptr, "Rred::F", 0);
   addArg('C', "compress", "Rred::Compress",CommandLine::HasArg);
   return true;
}
									/*}}}*/
std::vector<CommandLine::Args> getCommandArgs(APT_CMD const Program, char const * const Cmd)/*{{{*/
{
   std::vector<CommandLine::Args> Args;
   Args.reserve(50);
   if (Cmd != nullptr && strcmp(Cmd, "help") == 0)
      ; // no options for help so no need to implement it in each
   else
      switch (Program)
      {
	 case APT_CMD::APT: addArgumentsAPT(Args, Cmd); break;
	 case APT_CMD::APT_GET: addArgumentsAPTGet(Args, Cmd); break;
	 case APT_CMD::APT_CACHE: addArgumentsAPTCache(Args, Cmd); break;
	 case APT_CMD::APT_CDROM: addArgumentsAPTCDROM(Args, Cmd); break;
	 case APT_CMD::APT_CONFIG: addArgumentsAPTConfig(Args, Cmd); break;
	 case APT_CMD::APT_DUMP_SOLVER: addArgumentsAPTDumpSolver(Args, Cmd); break;
	 case APT_CMD::APT_EXTRACTTEMPLATES: addArgumentsAPTExtractTemplates(Args, Cmd); break;
	 case APT_CMD::APT_FTPARCHIVE: addArgumentsAPTFTPArchive(Args, Cmd); break;
	 case APT_CMD::APT_HELPER: addArgumentsAPTHelper(Args, Cmd); break;
	 case APT_CMD::APT_INTERNAL_PLANNER: addArgumentsAPTInternalPlanner(Args, Cmd); break;
	 case APT_CMD::APT_INTERNAL_SOLVER: addArgumentsAPTInternalSolver(Args, Cmd); break;
	 case APT_CMD::APT_MARK: addArgumentsAPTMark(Args, Cmd); break;
	 case APT_CMD::APT_SORTPKG: addArgumentsAPTSortPkgs(Args, Cmd); break;
	 case APT_CMD::RRED: addArgumentsRred(Args, Cmd); break;
      }

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
#undef addArg
static void ShowHelpListCommands(std::vector<aptDispatchWithHelp> const &Cmds)/*{{{*/
{
   if (Cmds.empty() || Cmds[0].Match == nullptr)
      return;
   std::cout << std::endl << _("Most used commands:") << std::endl;
   for (auto const &c: Cmds)
   {
      if (c.Help == nullptr)
	 continue;
      std::cout << "  " << c.Match << " - " << c.Help << std::endl;
   }
}
									/*}}}*/
static bool ShowCommonHelp(APT_CMD const Binary, CommandLine &CmdL, std::vector<aptDispatchWithHelp> const &Cmds,/*{{{*/
      bool (*ShowHelp)(CommandLine &))
{
   std::cout << PACKAGE << " " << PACKAGE_VERSION << " (" << COMMON_ARCH << ")" << std::endl;
   if (_config->FindB("version") == true && Binary != APT_CMD::APT_GET)
      return true;
   if (ShowHelp(CmdL) == false)
      return false;
   if (_config->FindB("version") == true || Binary == APT_CMD::APT_FTPARCHIVE)
      return true;
   ShowHelpListCommands(Cmds);
   std::cout << std::endl;
   char const * cmd = nullptr;
   switch (Binary)
   {
      case APT_CMD::APT: cmd = "apt(8)"; break;
      case APT_CMD::APT_CACHE: cmd = "apt-cache(8)"; break;
      case APT_CMD::APT_CDROM: cmd = "apt-cdrom(8)"; break;
      case APT_CMD::APT_CONFIG: cmd = "apt-config(8)"; break;
      case APT_CMD::APT_DUMP_SOLVER: cmd = nullptr; break;
      case APT_CMD::APT_EXTRACTTEMPLATES: cmd = "apt-extracttemplates(1)"; break;
      case APT_CMD::APT_FTPARCHIVE: cmd = "apt-ftparchive(1)"; break;
      case APT_CMD::APT_GET: cmd = "apt-get(8)"; break;
      case APT_CMD::APT_HELPER: cmd = nullptr; break;
      case APT_CMD::APT_INTERNAL_PLANNER: cmd = nullptr; break;
      case APT_CMD::APT_INTERNAL_SOLVER: cmd = nullptr; break;
      case APT_CMD::APT_MARK: cmd = "apt-mark(8)"; break;
      case APT_CMD::APT_SORTPKG: cmd = "apt-sortpkgs(1)"; break;
      case APT_CMD::RRED: cmd = nullptr; break;
   }
   if (cmd != nullptr)
      ioprintf(std::cout, _("See %s for more information about the available commands."), cmd);
   if (Binary != APT_CMD::APT_DUMP_SOLVER && Binary != APT_CMD::APT_INTERNAL_SOLVER &&
	 Binary != APT_CMD::APT_INTERNAL_PLANNER && Binary != APT_CMD::RRED)
      std::cout << std::endl <<
	 _("Configuration options and syntax is detailed in apt.conf(5).\n"
	       "Information about how to configure sources can be found in sources.list(5).\n"
	       "Package and version choices can be expressed via apt_preferences(5).\n"
	       "Security details are available in apt-secure(8).\n");
   if (Binary == APT_CMD::APT_GET || Binary == APT_CMD::APT)
      std::cout << std::right << std::setw(70) << _("This APT has Super Cow Powers.") << std::endl;
   else if (Binary == APT_CMD::APT_HELPER || Binary == APT_CMD::APT_DUMP_SOLVER)
      std::cout << std::right << std::setw(70) << _("This APT helper has Super Meep Powers.") << std::endl;
   return true;
}
									/*}}}*/
static void BinarySpecificConfiguration(char const * const Binary)	/*{{{*/
{
   std::string const binary = flNotDir(Binary);
   if (binary == "apt" || binary == "apt-config")
   {
      if (getenv("NO_COLOR") == nullptr)
         _config->CndSet("Binary::apt::APT::Color", true);
      _config->CndSet("Binary::apt::APT::Cache::Show::Version", 2);
      _config->CndSet("Binary::apt::APT::Cache::AllVersions", false);
      _config->CndSet("Binary::apt::APT::Cache::ShowVirtuals", true);
      _config->CndSet("Binary::apt::APT::Cache::Search::Version", 2);
      _config->CndSet("Binary::apt::APT::Cache::ShowDependencyType", true);
      _config->CndSet("Binary::apt::APT::Cache::ShowVersion", true);
      _config->CndSet("Binary::apt::APT::Get::Upgrade-Allow-New", true);
      _config->CndSet("Binary::apt::APT::Cmd::Show-Update-Stats", true);
      _config->CndSet("Binary::apt::DPkg::Progress-Fancy", true);
      _config->CndSet("Binary::apt::APT::Keep-Downloaded-Packages", false);
      _config->CndSet("Binary::apt::APT::Get::Update::InteractiveReleaseInfoChanges", true);
      _config->CndSet("Binary::apt::APT::Cmd::Pattern-Only", true);

      if (isatty(STDIN_FILENO))
         _config->CndSet("Binary::apt::Dpkg::Lock::Timeout", -1);
      else
         _config->CndSet("Binary::apt::Dpkg::Lock::Timeout", 120);
   }

   _config->Set("Binary", binary);
}
									/*}}}*/
static void BinaryCommandSpecificConfiguration(char const * const Binary, char const * const Cmd)/*{{{*/
{
   std::string const binary = flNotDir(Binary);
   if ((binary == "apt" || binary == "apt-get") && CmdMatches("upgrade", "dist-upgrade", "full-upgrade"))
   {
      //FIXME: the option is documented to apply only for install/remove, so
      // we force it false for configuration files where users can be confused if
      // we support it anyhow, but allow it on the commandline to take effect
      // even through it isn't documented as a user who doesn't want it wouldn't
      // ask for it
      _config->Set("APT::Get::AutomaticRemove", "");
   }
}
#undef CmdMatches
									/*}}}*/
std::vector<CommandLine::Dispatch> ParseCommandLine(CommandLine &CmdL, APT_CMD const Binary,/*{{{*/
      Configuration * const * const Cnf, pkgSystem ** const Sys, int const argc, const char *argv[],
      bool (*ShowHelp)(CommandLine &), std::vector<aptDispatchWithHelp> (*GetCommands)(void))
{
   InitLocale(Binary);
   if (Cnf != NULL && pkgInitConfig(**Cnf) == false)
   {
      _error->DumpErrors();
      exit(100);
   }

   if (likely(argc != 0 && argv[0] != NULL))
      BinarySpecificConfiguration(argv[0]);

   std::vector<CommandLine::Dispatch> Cmds;
   std::vector<aptDispatchWithHelp> const CmdsWithHelp = GetCommands();
   if (CmdsWithHelp.empty() == false)
   {
      CommandLine::Dispatch const help = { "help", [](CommandLine &){return false;} };
      Cmds.push_back(std::move(help));
   }
   std::transform(CmdsWithHelp.begin(), CmdsWithHelp.end(), std::back_inserter(Cmds),
		  [](auto &&cmd) { return CommandLine::Dispatch{cmd.Match, cmd.Handler}; });

   char const * CmdCalled = nullptr;
   if (Cmds.empty() == false && Cmds[0].Handler != nullptr)
      CmdCalled = CommandLine::GetCommand(Cmds.data(), argc, argv);
   if (CmdCalled != nullptr)
      BinaryCommandSpecificConfiguration(argv[0], CmdCalled);
   std::string const conf = "Binary::" + _config->Find("Binary");
   _config->MoveSubTree(conf.c_str(), nullptr);

   // Args running out of scope invalidates the pointer stored in CmdL,
   // but we don't use the pointer after this function, so we ignore
   // this problem for now and figure something out if we have to.
   auto Args = getCommandArgs(Binary, CmdCalled);
   CmdL = CommandLine(Args.data(), _config);

   if (CmdL.Parse(argc,argv) == false ||
       (Sys != NULL && pkgInitSystem(*_config, *Sys) == false))
   {
      if (_config->FindB("version") == true)
	 ShowCommonHelp(Binary, CmdL, CmdsWithHelp, ShowHelp);

      _error->DumpErrors();
      exit(100);
   }

   if (_config->FindB("APT::Get::Force-Yes", false) == true)
   {
      _error->Warning(_("--force-yes is deprecated, use one of the options starting with --allow instead."));
   }

   // See if the help should be shown
   if (_config->FindB("help") == true || _config->FindB("version") == true ||
	 (CmdL.FileSize() > 0 && strcmp(CmdL.FileList[0], "help") == 0))
   {
      ShowCommonHelp(Binary, CmdL, CmdsWithHelp, ShowHelp);
      exit(0);
   }
   if (Cmds.empty() == false && CmdL.FileSize() == 0)
   {
      ShowCommonHelp(Binary, CmdL, CmdsWithHelp, ShowHelp);
      exit(1);
   }
   return Cmds;
}
									/*}}}*/
unsigned short DispatchCommandLine(CommandLine &CmdL, std::vector<CommandLine::Dispatch> const &Cmds)	/*{{{*/
{
   // Match the operation
   bool const returned = Cmds.empty() ? true : CmdL.DispatchArg(Cmds.data());

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   if (returned == false)
      return 100;
   return Errors == true ? 100 : 0;
}
									/*}}}*/
