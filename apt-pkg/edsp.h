// -*- mode: cpp; mode: fold -*-
/** Description \file edsp.h						{{{
   ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   with the notable exception of reading a scenario for conversion into
   a Cache as this is handled by edsp interface for listparser and friends
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSP_H
#define PKGLIB_EDSP_H

#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <cstdio>

#include <list>
#include <string>
#include <vector>


class pkgDepCache;
class OpProgress;

namespace EDSP								/*{{{*/
{
	namespace Request
	{
	   enum Flags
	   {
	      AUTOREMOVE = (1 << 0), /*!< removal of unneeded packages should be performed */
	      UPGRADE_ALL = (1 << 1), /*!< upgrade all installed packages, like 'apt-get full-upgrade' without forbid flags */
	      FORBID_NEW_INSTALL = (1 << 2), /*!< forbid the resolver to install new packages */
	      FORBID_REMOVE = (1 << 3), /*!< forbid the resolver to remove packages */
	   };
	}
	/** \brief creates the EDSP request stanza
	 *
	 *  In the EDSP protocol the first thing send to the resolver is a stanza
	 *  encoding the request. This method will write this stanza by looking at
	 *  the given Cache and requests the installation of all packages which were
	 *  marked for installation in it (equally for remove).
	 *
	 *  \param Cache in which the request is encoded
	 *  \param output is written to this "file"
	 *  \param flags effecting the request documented in #EDSP::Request::Flags
	 *  \param Progress is an instance to report progress to
	 *
	 *  \return true if request was composed successfully, otherwise false
	 */
	APT_PUBLIC bool WriteRequest(pkgDepCache &Cache, FileFd &output,
				 unsigned int const flags = 0,
				OpProgress *Progress = NULL);

	/** \brief creates the scenario representing the package universe
	 *
	 *  After the request all known information about a package are send
	 *  to the solver. The output looks similar to a Packages or status file
	 *
	 *  All packages and version included in this Cache are send, even if
	 *  it doesn't make sense from an APT resolver point of view like versions
	 *  with a negative pin to enable the solver to propose even that as a
	 *  solution or at least to be able to give a hint what can be done to
	 *  satisfy a request.
	 *
	 *  \param Cache is the known package universe
	 *  \param output is written to this "file"
	 *  \param Progress is an instance to report progress to
	 *
	 *  \return true if universe was composed successfully, otherwise false
	 */
	APT_PUBLIC bool WriteScenario(pkgDepCache &Cache, FileFd &output, OpProgress *Progress = NULL);

	/** \brief creates a limited scenario representing the package universe
	 *
	 *  This method works similar to #WriteScenario as it works in the same
	 *  way but doesn't send the complete universe to the solver but only
	 *  packages included in the pkgset which will have only dependencies
	 *  on packages which are in the given set. All other dependencies will
	 *  be removed, so that this method can be used to create testcases
	 *
	 *  \param Cache is the known package universe
	 *  \param output is written to this "file"
	 *  \param pkgset is a set of packages the universe should be limited to
	 *  \param Progress is an instance to report progress to
	 *
	 *  \return true if universe was composed successfully, otherwise false
	 */
	APT_PUBLIC bool WriteLimitedScenario(pkgDepCache &Cache, FileFd &output,
					 std::vector<bool> const &pkgset,
					 OpProgress *Progress = NULL);

	/** \brief waits and acts on the information returned from the solver
	 *
	 *  This method takes care of interpreting whatever the solver sends
	 *  through the standard output like a solution, progress or an error.
	 *  The main thread should hand his control over to this method to
	 *  wait for the solver to finish the given task. The file descriptor
	 *  used as input is completely consumed and closed by the method.
	 *
	 *  \param input file descriptor with the response from the solver
	 *  \param Cache the solution should be applied on if any
	 *  \param Progress is an instance to report progress to
	 *
	 *  \return true if a solution is found and applied correctly, otherwise false
	 */
	APT_PUBLIC bool ReadResponse(int const input, pkgDepCache &Cache, OpProgress *Progress = NULL);

	/** \brief search and read the request stanza for action later
	 *
	 *  This method while ignore the input up to the point it finds the
	 *  Request: line as an indicator for the Request stanza.
	 *  The request is stored in the parameters install and remove then,
	 *  as the cache isn't build yet as the scenario follows the request.
	 *
	 *  \param input file descriptor with the edsp input for the solver
	 *  \param[out] install is a list which gets populated with requested installs
	 *  \param[out] remove is a list which gets populated with requested removals
	 *  \param[out] upgrade is true if it is a request like apt-get upgrade
	 *  \param[out] distUpgrade is true if it is a request like apt-get dist-upgrade
	 *  \param[out] autoRemove is true if removal of unneeded packages should be performed
	 *
	 *  \return true if the request could be found and worked on, otherwise false
	 */
	APT_PUBLIC bool ReadRequest(int const input, std::list<std::string> &install,
			std::list<std::string> &remove, unsigned int &flags);

	/** \brief takes the request lists and applies it on the cache
	 *
	 *  The lists as created by #ReadRequest will be used to find the
	 *  packages in question and mark them for install/remove.
	 *  No solving is done and no auto-install/-remove.
	 *
	 *  \param install is a list of packages to mark for installation
	 *  \param remove is a list of packages to mark for removal
	 *  \param Cache is there the markers should be set
	 *
	 *  \return false if the request couldn't be applied, true otherwise
	 */
	APT_PUBLIC bool ApplyRequest(std::list<std::string> const &install,
				 std::list<std::string> const &remove,
				 pkgDepCache &Cache);

	/** \brief formats a solution stanza for the given version
	 *
	 *  EDSP uses a simple format for reporting solutions:
	 *  A single required field name with an ID as value.
	 *  Additional fields might appear as debug aids.
	 *
	 *  \param output to write the stanza forming the solution to
	 *  \param Type of the stanza, used as field name
	 *  \param Ver this stanza applies to
	 *
	 *  \return true if stanza could be written, otherwise false
	 */
	APT_PUBLIC bool WriteSolutionStanza(FileFd &output, char const * const Type, pkgCache::VerIterator const &Ver);

	/** \brief sends a progress report
	 *
	 *  \param percent of the solving completed
	 *  \param message the solver wants the user to see
	 *  \param output the front-end listens for progress report
	 */
	APT_PUBLIC bool WriteProgress(unsigned short const percent, const char* const message, FileFd &output);

	/** \brief sends an error report
	 *
	 *  Solvers are expected to execute successfully even if
	 *  they were unable to calculate a solution for a given task.
	 *  Obviously they can't send a solution through, so this
	 *  methods deals with formatting an error message correctly
	 *  so that the front-ends can receive and display it.
	 *
	 *  The first line of the message should be a short description
	 *  of the error so it can be used for dialog titles or alike
	 *
	 *  \param uuid of this error message
	 *  \param message is free form text to describe the error
	 *  \param output the front-end listens for error messages
	 */
	APT_PUBLIC bool WriteError(char const * const uuid, std::string const &message, FileFd &output);


	/** \brief executes the given solver and returns the pipe ends
	 *
	 *  The given solver is executed if it can be found in one of the
	 *  configured directories and setup for it is performed.
	 *
	 *  \param solver to execute
	 *  \param[out] solver_in will be the stdin of the solver
	 *  \param[out] solver_out will be the stdout of the solver
	 *
	 *  \return PID of the started solver or 0 if failure occurred
	 */
	APT_PUBLIC pid_t ExecuteSolver(const char* const solver, int * const solver_in, int * const solver_out, bool /*overload*/);

	/** \brief call an external resolver to handle the request
	 *
	 *  This method wraps all the methods above to call an external solver
	 *
	 *  \param solver to execute
	 *  \param Cache with the problem and as universe to work in
	 *  \param flags effecting the request documented in #EDSP::Request::Flags
	 *  \param Progress is an instance to report progress to
	 *
	 *  \return true if the solver has successfully solved the problem,
	 *  otherwise false
	 */
	APT_PUBLIC bool ResolveExternal(const char* const solver, pkgDepCache &Cache,
				    unsigned int const flags = 0,
				    OpProgress *Progress = NULL);
}
									/*}}}*/
class pkgPackageManager;
namespace EIPP								/*{{{*/
{
   namespace Request
   {
      enum Flags
      {
	 IMMEDIATE_CONFIGURATION_ALL = (1 << 0), /*!< try to keep the least amount of packages unconfigured as possible at all times */
	 NO_IMMEDIATE_CONFIGURATION = (1 << 1), /*!< do not perform immediate configuration at all */
	 ALLOW_TEMPORARY_REMOVE_OF_ESSENTIALS = (1 << 2), /*!< just as the name suggests, very special case and dangerous! */
      };
   }

   APT_HIDDEN bool WriteRequest(pkgDepCache &Cache, FileFd &output,
	 unsigned int const flags, OpProgress * const Progress);
   APT_HIDDEN bool WriteScenario(pkgDepCache &Cache, FileFd &output,
	 OpProgress * const Progress);

   APT_HIDDEN bool OrderInstall(char const * const planner, pkgPackageManager * const PM,
	 unsigned int const version, OpProgress * const Progress);
   APT_HIDDEN bool ReadResponse(int const input, pkgPackageManager * const PM,
	 OpProgress * const Progress);

   enum class PKG_ACTION
   {
      NOOP,
      INSTALL,
      REINSTALL,
      REMOVE
   };
   APT_PUBLIC bool ReadRequest(int const input,
	 std::list<std::pair<std::string,PKG_ACTION>> &actions,
	 unsigned int &flags);
   APT_PUBLIC bool ApplyRequest(std::list<std::pair<std::string,PKG_ACTION>> &actions,
	 pkgDepCache &Cache);
}
									/*}}}*/
#endif
