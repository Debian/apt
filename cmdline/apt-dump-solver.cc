// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   dummy solver to get quickly a scenario file out of APT

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/edsp.h>

#include <config.h>

#include <cstdio>
#include <iostream>
									/*}}}*/

// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp() {

	std::cout <<
		PACKAGE " " VERSION " for " COMMON_ARCH " compiled on " __DATE__ " " __TIME__ << std::endl <<
		"Usage: apt-dump-resolver\n"
		"\n"
		"apt-dump-resolver is a dummy solver who just dumps its input to the\n"
		"file /tmp/dump.edsp and exists with a proper EDSP error.\n"
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

	FILE* input = fdopen(STDIN_FILENO, "r");
	FILE* output = fopen("/tmp/dump.edsp", "w");
	char buffer[400];
	while (fgets(buffer, sizeof(buffer), input) != NULL)
		fputs(buffer, output);
	fclose(output);
	fclose(input);

	EDSP::WriteError("ERR_JUST_DUMPING", "I am too dumb, i can just dump!\nPlease use one of my friends instead!", stdout);
}
