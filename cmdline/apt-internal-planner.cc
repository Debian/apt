// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   cover around the internal solver to be able to run it like an external

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-main.h>
#include <apt-private/private-output.h>

#include <cstdio>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

static bool ShowHelp(CommandLine &)					/*{{{*/
{
	std::cout <<
		_("Usage: apt-internal-planner\n"
		"\n"
		"apt-internal-planner is an interface to use the current internal\n"
		"installation planner for the APT family like an external one,\n"
		"for debugging or the like.\n");
	return true;
}
									/*}}}*/
APT_NORETURN static void DIE(std::string const &message) {		/*{{{*/
	std::cerr << "ERROR: " << message << std::endl;
	_error->DumpErrors(std::cerr);
	exit(EXIT_FAILURE);
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {};
}
									/*}}}*/
class PMOutput: public pkgPackageManager				/*{{{*/
{
   FileFd &output;
   bool const Debug;

protected:
   virtual bool Install(PkgIterator Pkg,std::string) APT_OVERRIDE
   {
      //std::cerr << "INSTALL: " << APT::PrettyPkg(&Cache, Pkg) << std::endl;
      return EDSP::WriteSolutionStanza(output, "Unpack", Cache[Pkg].InstVerIter(Cache));
   }
   virtual bool Configure(PkgIterator Pkg) APT_OVERRIDE
   {
      //std::cerr << "CONFIGURE: " << APT::PrettyPkg(&Cache, Pkg) << " " << std::endl;
      return EDSP::WriteSolutionStanza(output, "Configure", Cache[Pkg].InstVerIter(Cache));
   }
   virtual bool Remove(PkgIterator Pkg,bool) APT_OVERRIDE
   {
      //std::cerr << "REMOVE: " << APT::PrettyPkg(&Cache, Pkg) << " " << std::endl;
      return EDSP::WriteSolutionStanza(output, "Remove", Pkg.CurrentVer());
   }
public:
   PMOutput(pkgDepCache *Cache, FileFd &file) : pkgPackageManager(Cache), output(file),
      Debug(_config->FindB("Debug::EDSP::WriteSolution", false))
   {}

   bool ApplyRequest(std::list<std::pair<std::string,EIPP::PKG_ACTION>> const &actions)
   {
      for (auto && a: actions)
      {
	 auto const Pkg = Cache.FindPkg(a.first);
	 if (unlikely(Pkg.end() == true))
	    continue;
	 switch (a.second)
	 {
	    case EIPP::PKG_ACTION::NOOP:
	       break;
	    case EIPP::PKG_ACTION::INSTALL:
	    case EIPP::PKG_ACTION::REINSTALL:
	       FileNames[Pkg->ID] = "EIPP";
	       break;
	    case EIPP::PKG_ACTION::REMOVE:
	       break;
	 }
      }
      return true;
   }
};
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
	// we really don't need anything
	DropPrivileges();

	CommandLine CmdL;
	ParseCommandLine(CmdL, APT_CMD::APT_INTERNAL_PLANNER, &_config, NULL, argc, argv, &ShowHelp, &GetCommands);

	// Deal with stdout not being a tty
	if (!isatty(STDOUT_FILENO) && _config->FindI("quiet", -1) == -1)
		_config->Set("quiet","1");

	if (_config->FindI("quiet", 0) < 1)
		_config->Set("Debug::EIPP::WriteSolution", true);

	_config->Set("APT::System", "Debian APT planner interface");
	_config->Set("APT::Planner", "internal");
	_config->Set("eipp::scenario", "/nonexistent/stdin");
	FileFd output;
	if (output.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly | FileFd::BufferedWrite, true) == false)
	   DIE("stdout couldn't be opened");
	int const input = STDIN_FILENO;
	SetNonBlock(input, false);

	EDSP::WriteProgress(0, "Start up planner…", output);

	if (pkgInitSystem(*_config,_system) == false)
		DIE("System could not be initialized!");

	EDSP::WriteProgress(1, "Read request…", output);

	if (WaitFd(input, false, 5) == false)
		DIE("WAIT timed out in the planner");

	std::list<std::pair<std::string,EIPP::PKG_ACTION>> actions;
	unsigned int flags;
	if (EIPP::ReadRequest(input, actions, flags) == false)
		DIE("Parsing the request failed!");
	_config->Set("APT::Immediate-Configure", (flags & EIPP::Request::NO_IMMEDIATE_CONFIGURATION) == 0);
	_config->Set("APT::Immediate-Configure-All", (flags & EIPP::Request::IMMEDIATE_CONFIGURATION_ALL) != 0);
	_config->Set("APT::Force-LoopBreak", (flags & EIPP::Request::ALLOW_TEMPORARY_REMOVE_OF_ESSENTIALS) != 0);

	EDSP::WriteProgress(5, "Read scenario…", output);

	pkgCacheFile CacheFile;
	if (CacheFile.Open(NULL, false) == false)
		DIE("Failed to open CacheFile!");

	EDSP::WriteProgress(50, "Apply request on scenario…", output);

	if (EIPP::ApplyRequest(actions, CacheFile) == false)
		DIE("Failed to apply request to depcache!");

	EDSP::WriteProgress(60, "Call orderinstall on current scenario…", output);

	//_config->Set("Debug::pkgOrderList", true);
	//_config->Set("Debug::pkgPackageManager", true);
	PMOutput PM(CacheFile, output);
	if (PM.ApplyRequest(actions) == false)
		DIE("Failed to apply request to packagemanager!");
	pkgPackageManager::OrderResult const Res = PM.DoInstallPreFork();
	std::ostringstream broken;
	switch (Res)
	{
	   case pkgPackageManager::Completed:
	      EDSP::WriteProgress(100, "Done", output);
	      break;
	   case pkgPackageManager::Incomplete:
	      broken << "Planner could only incompletely plan an installation order!" << std::endl;
	      _error->DumpErrors(broken, GlobalError::DEBUG);
	      EDSP::WriteError("pm-incomplete", broken.str(), output);
	      break;
	   case pkgPackageManager::Failed:
	      broken << "Planner failed to find an installation order!" << std::endl;
	      _error->DumpErrors(broken, GlobalError::DEBUG);
	      EDSP::WriteError("pm-failed", broken.str(), output);
	      break;
	}

	return DispatchCommandLine(CmdL, {});
}
									/*}}}*/
