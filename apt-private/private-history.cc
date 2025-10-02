// Include files
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/history.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/tagfile.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-history.h>
#include <apt-private/private-install.h>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <glob.h>
#include <apti18n.h> // for coloring
		     /*}}}*/

using namespace APT::History;

// =============================================================================
// Internal API extensions
// =============================================================================

//

static Kind ReflectKind(const Kind &kind)
{
   switch (kind)
   {
   case Kind::Install:
   case Kind::Reinstall:
      return Kind::Remove;
   case Kind::Upgrade:
      return Kind::Downgrade;
   case Kind::Downgrade:
      return Kind::Upgrade;
   case Kind::Remove:
   case Kind::Purge:
      return Kind::Install;
   default:
      assert(false);
      return kind;
   }
}

std::vector<Change> APT::Internal::FlattenChanges(const Entry &entry)
{
   std::vector<Change> flattened;

   size_t total_size = 0;
   for (const auto &[_, changes] : entry.changeMap)
      total_size += changes.size();
   flattened.reserve(total_size);

   for (const auto &[_, changes] : entry.changeMap)
      flattened.insert(flattened.end(), changes.begin(), changes.end());
   return flattened;
}

Change APT::Internal::InvertChange(const Change &change)
{
   Change inverse = change;
   inverse.kind = ReflectKind(change.kind);

   // tailor change after what action was performed
   switch (change.kind)
   {
   case Kind::Upgrade:
      std::swap(inverse.currentVersion, inverse.candidateVersion);
      break;
   case Kind::Downgrade:
      std::swap(inverse.currentVersion, inverse.candidateVersion);
      break;
   default:
      break;
   }

   return inverse;
}

// =============================================================================
// Output formatting
// =============================================================================

//

// ShortenCommand - Take a command and shorten it such that it adheres
// to the given maximum length.
static std::string ShortenCommand(const std::string &cmd, const std::size_t maxLen)
{
   std::string shortenedCmd = cmd;
   if (shortenedCmd.starts_with("apt "))
      shortenedCmd = shortenedCmd.substr(4);
   if (shortenedCmd.length() > maxLen - 3)
      return shortenedCmd.substr(0, maxLen - 4) + "...";
   return shortenedCmd;
}

static std::string LocalizeKindToString(const Kind &kind)
{
   switch (kind)
   {
   case Kind::Install:
      return _("Install");
   case Kind::Reinstall:
      return _("Reinstall");
   case Kind::Upgrade:
      return _("Upgrade");
   case Kind::Downgrade:
      return _("Downgrade");
   case Kind::Remove:
      return _("Remove");
   case Kind::Purge:
      return _("Purge");
   default:
      return _("Undefined");
   }
}

// GetKindString - Take a history entry and construct the string
// corresponding to the actions performed. Multpile actions are
// alphabetically grouped such as:
// Install, Remove, and Reinstall -> I,R,rI
//
// Shorthand legend:
// Install   -> I
// Reinstall -> rI
// Upgrade   -> U
// Downgrade -> D
// Remove    -> R
// Purge     -> P
//
static std::string GetKindString(const Entry &entry)
{
   // We want full output if there is only one action
   if (entry.changeMap.size() == 1)
      return LocalizeKindToString(entry.changeMap.begin()->first).data();

   std::string kindGroup = "";
   // add localization later
   for (const auto &[action, _] : entry.changeMap)
   {
      switch (action)
      {
      case Kind::Install:
	 kindGroup += "I";
	 break;
      case Kind::Reinstall:
	 kindGroup += "rI";
	 break;
      case Kind::Upgrade:
	 kindGroup += "U";
	 break;
      case Kind::Downgrade:
	 kindGroup += "D";
	 break;
      case Kind::Remove:
	 kindGroup += "R";
	 break;
      case Kind::Purge:
	 kindGroup += "P";
	 break;
      default:
	 kindGroup += "INVALID";
      }
      kindGroup += ",";
   }
   // remove trailing ","
   if (not kindGroup.empty())
      kindGroup.pop_back();
   return kindGroup;
}

// =============================================================================
// Output printing
// =============================================================================

//

static void PrintHistoryVector(const HistoryBuffer buf, int columnWidth)
{
   // Calculate the width of the ID column, esentially the number of digits
   int id = 0;
   size_t sizeFrac = buf.size();
   int idWidth = 2;
   while (sizeFrac)
   {
      sizeFrac /= 10;
      idWidth++;
   }

   // Print headers
   auto writeEntry = [](std::string entry, size_t width)
   {
      std::cout << std::left << std::setw(width) << entry;
   };
   writeEntry(_("ID"), idWidth);
   writeEntry(_("Command line"), columnWidth);
   // NOTE: if date format is different,
   // this width needs to change
   writeEntry(_("Date and Time"), 23); // width for datestring
   writeEntry(_("Action"), 10);	       // 9 chars longest action type
   writeEntry(_("Changes"), columnWidth);
   std::cout << "\n\n";

   // Each entry corresponds to a row
   for (auto entry : buf)
   {
      writeEntry(std::to_string(id), idWidth);
      writeEntry(ShortenCommand(entry.cmdLine, columnWidth), columnWidth);
      writeEntry(entry.startDate, 23);
      // If there are multiple actions, we want to group them
      writeEntry(GetKindString(entry), 10);

      // Count the number of packages changed
      size_t numChanges = 0;
      for (const auto &[_, changes] : entry.changeMap)
	 numChanges += changes.size();
      writeEntry(std::to_string(numChanges), columnWidth);
      std::cout << "\n";
      id++;
   }
}

// PrintChange - Take a change and print the event for that package.
// Example:
//  "package (0.1.0, 0.2.0)" and "Upgrade" -> "Upgrade package (0.1.0 -> 0.2.0)"
//  "package (0.1.0)" and "Install" -> "Install package (0.1.0)"
static void PrintChange(const Change &change)
{
   std::cout << "    " << LocalizeKindToString(change.kind) << " " << change.package;
   std::cout << " (" << change.currentVersion;
   if (not change.candidateVersion.empty())
      std::cout << " -> " << change.candidateVersion;
   if (change.automatic)
      std::cout << ", " << _("automatic");

   std::cout << ")";
}

// PrintDetailedEntry - Take a history buffer and print the detailed
// transaction details for the given transaction id.
static void PrintDetailedEntry(const HistoryBuffer &buf, const size_t id)
{
   Entry entry = buf[id];
   std::cout << _("Transaction ID") << ": " << id << "\n";
   std::cout << _("Start time") << ": " << entry.startDate << "\n";
   std::cout << _("End time") << ": " << entry.endDate << "\n";
   std::cout << _("Requested by") << ": " << entry.requestingUser << "\n";
   std::cout << _("Command line") << ": " << entry.cmdLine << "\n";
   if (not entry.error.empty())
   {
      std::cout << _config->Find("APT::Color::Red") << _("Error") << ": ";
      std::cout << entry.error << _config->Find("APT::Color::Neutral") << "\n";
   }
   if (not entry.comment.empty())
      std::cout << _("Comment") << ": " << entry.comment << "\n";

   // For each performed action, print what it did to each package
   std::cout << _("Packages changed") << ":" << "\n";
   for (const auto &[_, changes] : entry.changeMap)
   {
      for (const auto &change : changes)
      {
	 PrintChange(change);
	 std::cout << "\n";
      }
      std::cout << "\n";
   }
}

// =============================================================================
// Transaction helpers
// =============================================================================

//

class TransactionController
{
   public:
   TransactionController(CacheFile &cache) : Cache(cache),
					     Fix(Cache.GetDepCache()),
					     InstallAction(Cache, &Fix, false),
					     RemoveAction(Cache, &Fix),
					     HeldBackPackages()
   {
   }

   bool AppendChange(const Change &change)
   {
      auto pkg = Cache->FindPkg(change.package);
      if (pkg.end())
	 return _error->Error(_("Could not find package %s"), change.package.data());
      if (IsRemoval(change.kind))
      {
	 auto ver = pkg.CurrentVer();
	 if (ver == nullptr)
	 {
	    _error->Warning(_("Tried to remove %s, but it is not installed"), change.package.data());
	    return true;
	 }

	 RemoveAction(ver);
	 return true;
      }

      auto ver = pkg.VersionList();
      for (; not ver.end(); ++ver)
	 if (strcmp(ver.VerStr(), change.currentVersion.data()) == 0)
	    break;
      if (ver.end())
	 return _error->Error(_("Could not find given version %s of %s"), change.currentVersion.data(),
			      change.package.data());
      InstallAction(ver);
      return true;
   }

   bool CommitChanges()
   {
      InstallAction.doAutoInstall();
      OpTextProgress Progress(*_config);
      bool const resolver_fail = Fix.Resolve(true, &Progress);
      if (resolver_fail == false && Cache->BrokenCount() == 0)
	 return false;
      if (CheckNothingBroken(Cache) == false)
	 return false;
      return InstallPackages(Cache, HeldBackPackages, false, true);
   }

   private:
   // This must be a reference for lifetime safety
   CacheFile &Cache;
   pkgProblemResolver Fix;
   TryToInstall InstallAction;
   TryToRemove RemoveAction;
   APT::PackageVector HeldBackPackages;
};

static bool ParseId(const char *str, size_t &id, size_t max)
{
   try
   {
      id = std::stoi(str);
   }
   catch (const std::invalid_argument &e)
   {
      return _error->Error(_("'%s' not a valid ID."), str);
   }
   if (max < id)
      return _error->Error(_("Transaction ID '%ld' out of bounds."), id);

   return true;
}

// =============================================================================
// Entrypoints
// =============================================================================

//

bool DoHistoryUndo(CommandLine &Cmd)
{
   HistoryBuffer buf = {};
   if (not ParseLogDir(buf))
      return _error->Error(_("Could not read %s"),
			   _config->FindFile("Dir::Log::History").data());
   size_t id = 0;
   if (not ParseId(*(Cmd.FileList + 1), id, buf.size() - 1))
      return false;

   CacheFile Cache;
   if (Cache.BuildCaches(true) == false)
      return false;
   TransactionController Controller(Cache);

   for (const auto &change : APT::Internal::FlattenChanges(buf[id]))
      if (not Controller.AppendChange(APT::Internal::InvertChange(change)))
	 return false;

   return Controller.CommitChanges();
}

bool DoHistoryRollback(CommandLine &Cmd)
{
   HistoryBuffer buf = {};
   if (not ParseLogDir(buf))
      return _error->Error(_("Could not read %s"),
			   _config->FindFile("Dir::Log::History").data());

   size_t rollbackId = 0;
   if (not ParseId(*(Cmd.FileList + 1), rollbackId, buf.size()))
      return false;
   CacheFile Cache;
   if (Cache.BuildCaches(true) == false)
      return false;
   TransactionController Controller(Cache);

   // Map to keep the effective change of each package
   std::unordered_map<std::string, Change> effectiveChangeMap;
   // Plus 1 for zero indexing
   size_t numChanges = buf.size() - (rollbackId + 1);
   std::span<Entry> bufSpan(buf.end() - numChanges, numChanges);
   // Iterate in reverse to process changes LIFO
   for (auto it = bufSpan.rbegin(); it != bufSpan.rend(); ++it)
      for (const auto &change : APT::Internal::FlattenChanges(*it))
	 effectiveChangeMap[change.package] = APT::Internal::InvertChange(change);

   for (const auto &[_, change] : effectiveChangeMap)
   {
      if (not Controller.AppendChange(change))
	 return false;
   }

   return Controller.CommitChanges();
}

bool DoHistoryRedo(CommandLine &Cmd)
{
   HistoryBuffer buf = {};
   if (not ParseLogDir(buf))
      return _error->Error(_("Could not read %s"),
			   _config->FindFile("Dir::Log::History").data());

   size_t id = 0;
   if (not ParseId(*(Cmd.FileList + 1), id, buf.size() - 1))
      return false;

   CacheFile Cache;
   if (Cache.BuildCaches(true) == false)
      return false;
   TransactionController Controller(Cache);

   for (const auto &change : APT::Internal::FlattenChanges(buf[id]))
      if (not Controller.AppendChange(change))
	 return false;

   return Controller.CommitChanges();
}

bool DoHistoryList(CommandLine &Cmd)
{
   HistoryBuffer buf = {};

   if (Cmd.FileSize() != 1)
      return _error->Error("This command does not support any arguments");

   if (not ParseLogDir(buf))
      return _error->Error(_("Could not read: %s"),
			   _config->FindFile("Dir::Log::History").data());
   PrintHistoryVector(buf, 25);

   return true;
}

bool DoHistoryInfo(CommandLine &Cmd)
{
   HistoryBuffer buf = {};
   if (not ParseLogDir(buf))
      return _error->Error(_("Could not read: %s"),
			   _config->FindFile("Dir::Log::History").data());

   size_t id = 0;
   for (size_t i = 1; i < Cmd.FileSize(); i++)
   {
      if (not ParseId(*(Cmd.FileList + i), id, buf.size() - 1))
	 return false;
      PrintDetailedEntry(buf, id);
   }

   return true;
}
