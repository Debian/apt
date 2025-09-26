// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   History - Parse history logs to structured data.

   ##################################################################### */
/*}}}*/
#ifndef APTPKG_HISTORY_H
#define APTPKG_HISTORY_H

#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/tagfile.h>

#include <map>
#include <string>
#include <vector>

namespace APT::History
{

enum class Kind
{
   Install,
   Reinstall,
   Upgrade,
   Downgrade,
   Remove,
   Purge,
};

// KindToString - Take a kind and return its
// string representation.
//
// NOTE: Non-localized English version
std::string KindToString(const Kind &kind);

struct Change
{
   Kind kind;
   std::string package;
   std::string currentVersion;
   std::string candidateVersion;
   bool automatic = false;

   private:
   void *d; // pointer for future extension;
};

struct Entry
{
   // Strings instead of string_view to avoid reference errors
   std::string startDate;
   std::string endDate;
   std::string cmdLine;
   std::string comment;
   std::string error;
   std::string requestingUser;
   std::map<Kind, std::vector<Change>> changeMap;

   private:
   void *d;
};

// History is defined as the collection of entries in the history log(s).
typedef std::vector<Entry> HistoryBuffer;

// ParseSection - Take a tag section and parse it as a
// history log entry.
APT_PUBLIC Entry ParseSection(const pkgTagSection &section);
// ParseFile - Take a file descriptor and parse it as a history
// log to the given buffer.
// NOTE: Caller is responsible for closing the file descriptor.
APT_PUBLIC bool ParseFile(FileFd &fd, HistoryBuffer &buf);
// ParseLogDir - Parse the apt history log directory to the buffer.
APT_PUBLIC bool ParseLogDir(HistoryBuffer &buf);
} // namespace APT::History

#endif
