// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   dummy solver to get quickly a scenario file out of APT

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/edsp.h>

#include <string.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <sstream>

#include <config.h>
									/*}}}*/

// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool ShowHelp() {
	ioprintf(std::cout, "%s %s (%s)\n", PACKAGE, PACKAGE_VERSION, COMMON_ARCH);
	std::cout <<
		"Usage: apt-dump-solver\n"
		"\n"
		"apt-dump-solver is a dummy solver who just dumps its input to the\n"
		"file specified in the environment variable APT_EDSP_DUMP_FILENAME and\n"
		"exists with a proper EDSP error.\n"
		"\n"
		"                       This dump has lost Super Cow Powers.\n";
	return true;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
	if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1],"-h") == 0 ||
	    strcmp(argv[1],"-v") == 0 || strcmp(argv[1],"--version") == 0)) {
		ShowHelp();
		return 0;
	}

	// we really don't need anything
	DropPrivileges();
	char const * const filename = getenv("APT_EDSP_DUMP_FILENAME");
	if (filename == NULL || strlen(filename) == 0)
	{
	   EDSP::WriteError("ERR_NO_FILENAME", "You have to set the environment variable APT_EDSP_DUMP_FILENAME\n"
		 "to a valid filename to store the dump of EDSP solver input in.\n"
		 "For example with: export APT_EDSP_DUMP_FILENAME=/tmp/dump.edsp", stdout);
	   return 0;
	}

	RemoveFile(argv[0], filename);
	FileFd input, output;
	if (input.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false ||
	      output.Open(filename, FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive, 0600) == false ||
	      CopyFile(input, output) == false || input.Close() == false || output.Close() == false)
	{
	   std::ostringstream out;
	   out << "Writing EDSP solver input to file '" << filename << "' failed!\n";
	   _error->DumpErrors(out);
	   EDSP::WriteError("ERR_WRITE_ERROR", out.str(), stdout);
	   return 0;
	}

	EDSP::WriteError("ERR_JUST_DUMPING", "I am too dumb, i can just dump!\nPlease use one of my friends instead!", stdout);
	return 0;
}
