// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   cover around the internal solver to be able to run it like an external

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
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

#include <config.h>
#include <apti18n.h>

#include <unistd.h>
#include <cstdio>
									/*}}}*/

// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp(CommandLine &CmdL) {
	ioprintf(std::cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
		 COMMON_ARCH,__DATE__,__TIME__);

	std::cout <<
		_("Usage: apt-internal-resolver\n"
		"\n"
		"apt-internal-resolver is an interface to use the current internal\n"
		"like an external resolver for the APT family for debugging or alike\n"
		"\n"
		"Options:\n"
		"  -h  This help text.\n"
		"  -q  Loggable output - no progress indicator\n"
		"  -c=? Read this configuration file\n"
		"  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
		"apt.conf(5) manual pages for more information and options.\n"
		"                       This APT has Super Cow Powers.\n");
	return true;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
	CommandLine::Args Args[] = {
		{'h',"help","help",0},
		{'v',"version","version",0},
		{'q',"quiet","quiet",CommandLine::IntLevel},
		{'q',"silent","quiet",CommandLine::IntLevel},
		{'c',"config-file",0,CommandLine::ConfigFile},
		{'o',"option",0,CommandLine::ArbItem},
		{0,0,0,0}};

	CommandLine CmdL(Args,_config);
	if (pkgInitConfig(*_config) == false ||
	    CmdL.Parse(argc,argv) == false) {
		_error->DumpErrors();
		return 2;
	}

	// See if the help should be shown
	if (_config->FindB("help") == true ||
	    _config->FindB("version") == true) {
		ShowHelp(CmdL);
		return 1;
	}

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

	_config->Set("APT::Solver", "internal");
	_config->Set("edsp::scenario", "stdin");
	int input = STDIN_FILENO;
	FILE* output = stdout;
	SetNonBlock(input, false);

	EDSP::WriteProgress(0, "Start up solver…", output);

	if (pkgInitSystem(*_config,_system) == false) {
		std::cerr << "System could not be initialized!" << std::endl;
		return 1;
	}

	EDSP::WriteProgress(1, "Read request…", output);

	if (WaitFd(input, false, 5) == false)
		std::cerr << "WAIT timed out in the resolver" << std::endl;

	std::list<std::string> install, remove;
	bool upgrade, distUpgrade, autoRemove;
	if (EDSP::ReadRequest(input, install, remove, upgrade, distUpgrade, autoRemove) == false) {
		std::cerr << "Parsing the request failed!" << std::endl;
		return 2;
	}

	EDSP::WriteProgress(5, "Read scenario…", output);

	pkgCacheFile CacheFile;
	CacheFile.Open(NULL, false);

	EDSP::WriteProgress(50, "Apply request on scenario…", output);

	if (EDSP::ApplyRequest(install, remove, CacheFile) == false) {
		std::cerr << "Failed to apply request to depcache!" << std::endl;
		return 3;
	}

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

	if (upgrade == true) {
		if (pkgAllUpgrade(CacheFile) == false) {
			EDSP::WriteError("ERR_UNSOLVABLE_UPGRADE", "An upgrade error occured", output);
			return 0;
		}
	} else if (distUpgrade == true) {
		if (pkgDistUpgrade(CacheFile) == false) {
			EDSP::WriteError("ERR_UNSOLVABLE_DIST_UPGRADE", "An dist-upgrade error occured", output);
			return 0;
		}
	} else if (Fix.Resolve() == false) {
		EDSP::WriteError("ERR_UNSOLVABLE", "An error occured", output);
		return 0;
	}

	EDSP::WriteProgress(95, "Write solution…", output);

	if (EDSP::WriteSolution(CacheFile, output) == false) {
		std::cerr << "Failed to output the solution!" << std::endl;
		return 4;
	}

	EDSP::WriteProgress(100, "Done", output);

	bool const Errors = _error->PendingError();
	if (_config->FindI("quiet",0) > 0)
		_error->DumpErrors(std::cerr);
	else
		_error->DumpErrors(std::cerr, GlobalError::DEBUG);
	return Errors == true ? 100 : 0;
}
									/*}}}*/
