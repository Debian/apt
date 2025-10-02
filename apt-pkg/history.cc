// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of functions for parsing the history log
   ##################################################################### */
/*}}}*/
// Include Files							/*{{{*/

#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/history.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <cassert>
#include <cctype>

#include <glob.h>
#include <apti18n.h> // for coloring

namespace APT::History
{

static Change ParsePackageEvent(const std::string &event)
{
   Change change{};
   // Remove all spaces
   std::string trimEvent = event;
   trimEvent.erase(std::remove_if(trimEvent.begin(), trimEvent.end(),
				  [](unsigned char c)
				  { return std::isspace(c); }),
		   trimEvent.end());
   auto openParen = trimEvent.find('(');
   if (openParen == std::string::npos)
      return change;

   change.package = trimEvent.substr(0, openParen);

   auto closeParen = trimEvent.find(')', openParen);
   if (closeParen == std::string::npos)
      return change;

   std::string versionStr = trimEvent.substr(openParen + 1, closeParen - openParen - 1);
   auto values = VectorizeString(versionStr, ',');
   for (size_t i = 0; i < values.size(); ++i)
   {
      if (i == 0 && std::isdigit(values[0][0]))
	 change.currentVersion = std::move(values[i]);
      else if (i == 1 && std::isdigit(values[1][0]))
	 change.candidateVersion = std::move(values[i]);
      else if (values[i] == "automatic")
	 change.automatic = true;
      else
	 _error->Warning(_("Unknown flag: %s"), values[i].c_str());
   }
   return change;
}

static std::vector<std::string> SplitPackagesInContent(const std::string &content)
{
   std::vector<std::string> result;
   std::size_t start = 0;

   if (content.length() == 0)
      return result;

   // Split at "), " but keep the parenthesis.
   while (true)
   {
      std::size_t pos = content.find("), ", start);
      if (pos == std::string::npos)
      {
	 result.push_back(content.substr(start));
	 break;
      }
      result.push_back(content.substr(start, pos - start + 1));
      // Increment by 3 since 3 == len("), ")
      start = pos + 3;
   }
   return result;
}

std::string KindToString(const Kind &kind)
{
   switch (kind)
   {
   case Kind::Install:
      return "Install";
   case Kind::Reinstall:
      return "Reinstall";
   case Kind::Upgrade:
      return "Upgrade";
   case Kind::Downgrade:
      return "Downgrade";
   case Kind::Remove:
      return "Remove";
   case Kind::Purge:
      return "Purge";
   default:
      return "Undefined";
   }
}

Entry ParseSection(
   const pkgTagSection &section)
{
   Entry entry{};
   entry.startDate = section.Find("Start-Date");
   entry.endDate = section.Find("End-Date");
   entry.cmdLine = section.Find("Commandline");
   entry.requestingUser = section.Find("Requested-By");
   entry.comment = section.Find("Comment");
   entry.error = section.Find("Error");

   std::string content = "";
   const Kind kinds[] =
      {
	 Kind::Install,
	 Kind::Reinstall,
	 Kind::Downgrade,
	 Kind::Upgrade,
	 Kind::Remove,
	 Kind::Purge,
      };
   for (const auto &kind : kinds)
   {
      content = section.Find(KindToString(kind));
      if (content.empty())
	 continue;

      std::vector<std::string> package_events = SplitPackagesInContent(content);
      std::vector<Change> changes = {};
      for (auto event : package_events)
      {
	 Change change = ParsePackageEvent(event);
	 change.kind = kind;
	 changes.push_back(change);
      }
      // Changed packages should be in order
      std::sort(changes.begin(), changes.end(), [](const Change &a, const Change &b)
		{ return a.package < b.package; });
      entry.changeMap[kind] = changes;
   }

   return entry;
}

bool ParseFile(FileFd &fd, HistoryBuffer &buf)
{
   pkgTagFile file(&fd, FileFd::ReadOnly);
   pkgTagSection tmpSection;
   while (file.Step(tmpSection))
      buf.push_back(ParseSection(tmpSection));
   return true;
}

bool ParseLogDir(HistoryBuffer &buf)
{
   std::string files = _config->FindFile("Dir::Log::History") + "*";
   const char *pattern = files.data();
   glob_t result;

   int ret = glob(pattern, GLOB_TILDE, nullptr, &result);
   if (ret != 0)
      return _error->Error(_("Cannot find history files: %s"), files.c_str());

   for (size_t i = 0; i < result.gl_pathc; ++i)
   {
      FileFd fd;
      if (not fd.Open(result.gl_pathv[i], FileFd::ReadOnly, FileFd::Extension))
	 return _error->Error(_("Could not open file: %s"), result.gl_pathv[i]);
      if (not ParseFile(fd, buf))
	 return _error->Error(_("Could not parse file: %s"), result.gl_pathv[i]);
      if (not fd.Close())
	 return _error->Error(_("Could not close file: %s"), result.gl_pathv[i]);
   }

   // Sort entries by time
   std::sort(buf.begin(), buf.end(),
	     [](const Entry &a, const Entry &b)
	     {
		return a.startDate < b.startDate;
	     });

   return true;
}
} // namespace APT::History
