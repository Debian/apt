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

/**
 * @enum class Kind
 * @brief Represents the possible actions of APT transactions in history.
 */
enum class Kind
{
   Install,
   Reinstall,
   Upgrade,
   Downgrade,
   InstallEnd = Downgrade,
   Remove,
   Purge,
};

/**
 * Get the string parsing specific string representation 
 * of a @ref Kind.
 *
 * @param kind An action kind.
 * @return A string representation.
 *
 * @note This string representation is not localized.
 */
std::string KindToString(const Kind &kind);

/**
 * @struct Change
 * @brief Represents a package change.
 */
struct Change
{
   Kind kind;
   std::string package;
   std::string currentVersion;
   std::string candidateVersion;
   bool automatic = false; ///< If the change was automatically performed.

   private:
   void *d; // pointer for future extension;
};

static inline bool IsRemoval(const Kind &kind) { return kind > Kind::InstallEnd; }

/**
 * @struct Entry
 * @brief Represents an entry in the APT history log.
 */
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

/**
 * Parse a tag section as history log entry.
 *
 * @param section A tag section.
 * @return A history log entry.
 */
APT_PUBLIC Entry ParseSection(const pkgTagSection &section);

/**
 * Parse a file descriptor to a history buffer.
 *
 * @param fd A file descriptor.
 * @param buf A history buffer.
 * @return true if successful, false otherwise.
 *
 * @note Caller is responsible for closing the file descriptor.
 */
APT_PUBLIC bool ParseFile(FileFd &fd, HistoryBuffer &buf);

// ParseLogDir - Parse the apt history log directory to the buffer.
/**
 * Parse the APT history log directory to a buffer.
 *
 * @param buf A history buffer.
 * @return true if successful, false otherwise.
 */
APT_PUBLIC bool ParseLogDir(HistoryBuffer &buf);

} // namespace APT::History

#endif
