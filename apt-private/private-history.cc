// Include files
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/history.h>
#include <apt-pkg/tagfile.h>

#include <apt-private/private-history.h>

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glob.h>
#include <apti18n.h> // for coloring
		     /*}}}*/

using namespace APT::History;

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
   kindGroup.pop_back();
   return kindGroup;
}

// PrintHistory - Take a history vector and print it.
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

bool DoHistoryList(CommandLine &Cmd)
{
   HistoryBuffer buf = {};

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
      try
      {
	 id = std::stoi(*(Cmd.FileList + i));
      }
      catch (const std::invalid_argument &e)
      {
	 return _error->Error(_("'%s' not a valid ID."), *(Cmd.FileList + i));
      }
      if (buf.size() <= id)
	 return _error->Error(_("Transaction ID '%ld' out of bounds."), id);
      PrintDetailedEntry(buf, id);
   }

   return true;
}
