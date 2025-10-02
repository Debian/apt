#ifndef APT_PRIVATE_HISTORY_H
#define APT_PRIVATE_HISTORY_H

#include <apt-pkg/cmndline.h>
#include <apt-pkg/history.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/tagfile.h>

APT_PUBLIC bool DoHistoryInfo(CommandLine &Cmd);
APT_PUBLIC bool DoHistoryList(CommandLine &Cmd);
APT_PUBLIC bool DoHistoryRedo(CommandLine &Cmd);
APT_PUBLIC bool DoHistoryRollback(CommandLine &Cmd);
APT_PUBLIC bool DoHistoryUndo(CommandLine &Cmd);

namespace APT::Internal
{
using namespace History;
/** 
 * Flatten the changes contained within a history entry.
 *
 * This function extracts and returns a flat list of all changes
 * from the given entry, preserving their order if applicable.
 *
 * @param entry A history entry containing a map of changes.
 * @return A vector containing all individual changes from the entry.
 */
std::vector<Change> FlattenChanges(const Entry &entry);

/** 
 * Invert a change.
 *
 * This function inverts a change's effective action and version
 * such that a change followed by its inverse has no effective change.
 *
 * @param change A transactional change.
 * @return The given change's inverse.
 */
Change InvertChange(const Change &change);

} // namespace APT::Internal
#endif
