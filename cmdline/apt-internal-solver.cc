// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   cover around the internal solver to be able to run it like an external

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/init.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <apt-private/private-output.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-main.h>

#include <string.h>
#include <iostream>
#include <sstream>
#include <list>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <stdlib.h>

#include <apti18n.h>
									/*}}}*/

static bool ShowHelp(CommandLine &)					/*{{{*/
{
	std::cout <<
		_("Usage: apt-internal-solver\n"
		"\n"
		"apt-internal-solver is an interface to use the current internal\n"
		"resolver for the APT family like an external one, for debugging or\n"
		"the like.\n");
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
int main(int argc,const char *argv[])					/*{{{*/
{
	InitLocale();

	// we really don't need anything
	DropPrivileges();

	CommandLine CmdL;
	ParseCommandLine(CmdL, APT_CMD::APT_INTERNAL_SOLVER, &_config, NULL, argc, argv, &ShowHelp, &GetCommands);

	if (CmdL.FileList[0] != 0 && strcmp(CmdL.FileList[0], "scenario") == 0)
	{
		if (pkgInitSystem(*_config,_system) == false) {
			std::cerr << "System could not be initialized!" << std::endl;
			return 1;
		}
		pkgCacheFile CacheFile;
		CacheFile.Open(NULL, false);
		APT::PackageSet pkgset = APT::PackageSet::FromCommandLine(CacheFile, CmdL.FileList + 1);
		FILE* output = stdout;
		if (pkgset.empty() == true)
			EDSP::WriteScenario(CacheFile, output);
		else
			EDSP::WriteLimitedScenario(CacheFile, output, pkgset);
		fclose(output);
		_error->DumpErrors(std::cerr);
		return 0;
	}

	// Deal with stdout not being a tty
	if (!isatty(STDOUT_FILENO) && _config->FindI("quiet", -1) == -1)
		_config->Set("quiet","1");

	if (_config->FindI("quiet", 0) < 1)
		_config->Set("Debug::EDSP::WriteSolution", true);

	_config->Set("APT::System", "Debian APT solver interface");
	_config->Set("APT::Solver", "internal");
	_config->Set("edsp::scenario", "/nonexistent/stdin");
	int input = STDIN_FILENO;
	FILE* output = stdout;
	SetNonBlock(input, false);

	EDSP::WriteProgress(0, "Start up solver…", output);

	if (pkgInitSystem(*_config,_system) == false)
		DIE("System could not be initialized!");

	EDSP::WriteProgress(1, "Read request…", output);

	if (WaitFd(input, false, 5) == false)
		DIE("WAIT timed out in the resolver");

	std::list<std::string> install, remove;
	bool upgrade, distUpgrade, autoRemove;
	if (EDSP::ReadRequest(input, install, remove, upgrade, distUpgrade, autoRemove) == false)
		DIE("Parsing the request failed!");

	EDSP::WriteProgress(5, "Read scenario…", output);

	pkgCacheFile CacheFile;
	if (CacheFile.Open(NULL, false) == false)
		DIE("Failed to open CacheFile!");

	EDSP::WriteProgress(50, "Apply request on scenario…", output);

	if (EDSP::ApplyRequest(install, remove, CacheFile) == false)
		DIE("Failed to apply request to depcache!");

	pkgProblemResolver Fix(CacheFile);
	for (std::list<std::string>::const_iterator i = remove.begin();
	     i != remove.end(); ++i) {
		pkgCache::PkgIterator P = CacheFile->FindPkg(*i);
		Fix.Clear(P);
		Fix.Protect(P);
		Fix.Remove(P);
	}

	for (std::list<std::string>::const_iterator i = install.begin();
	     i != install.end(); ++i) {
		pkgCache::PkgIterator P = CacheFile->FindPkg(*i);
		Fix.Clear(P);
		Fix.Protect(P);
	}

	for (std::list<std::string>::const_iterator i = install.begin();
	     i != install.end(); ++i)
		CacheFile->MarkInstall(CacheFile->FindPkg(*i), true);

	EDSP::WriteProgress(60, "Call problemresolver on current scenario…", output);

	std::string failure;
	if (upgrade == true) {
		if (APT::Upgrade::Upgrade(CacheFile, APT::Upgrade::FORBID_REMOVE_PACKAGES | APT::Upgrade::FORBID_INSTALL_NEW_PACKAGES) == false)
			failure = "ERR_UNSOLVABLE_UPGRADE";
	} else if (distUpgrade == true) {
		if (APT::Upgrade::Upgrade(CacheFile, APT::Upgrade::ALLOW_EVERYTHING) == false)
			failure = "ERR_UNSOLVABLE_DIST_UPGRADE";
	} else if (Fix.Resolve() == false)
		failure = "ERR_UNSOLVABLE";

	if (failure.empty() == false) {
		std::ostringstream broken;
		ShowBroken(broken, CacheFile, false);
		EDSP::WriteError(failure.c_str(), broken.str(), output);
		return 0;
	}

	EDSP::WriteProgress(95, "Write solution…", output);

	if (EDSP::WriteSolution(CacheFile, output) == false)
		DIE("Failed to output the solution!");

	EDSP::WriteProgress(100, "Done", output);

	return DispatchCommandLine(CmdL, {});
}
									/*}}}*/
