#include <apt-pkg/fileutl.h>

#include "assert.h"
#include <string>
#include <vector>

#include <stdio.h>
#include <iostream>

// simple helper to quickly output a vector of strings
void dumpVector(std::vector<std::string> vec) {
	for (std::vector<std::string>::const_iterator v = vec.begin();
	     v != vec.end(); v++)
		std::cout << *v << std::endl;
}

#define P(x)	string(argv[1]).append("/").append(x)

int main(int argc,char *argv[])
{
	if (argc != 2) {
		std::cout << "One parameter expected - given " << argc << std::endl;
		return 100;
	}

	// Files with no extension
	std::vector<std::string> files = GetListOfFilesInDir(argv[1], "", true);
	equals(files.size(), 2);
	equals(files[0], P("01yet-anothernormalfile"));
	equals(files[1], P("anormalfile"));

	// Files with no extension - should be the same as above
	files = GetListOfFilesInDir(argv[1], "", true, true);
	equals(files.size(), 2);
	equals(files[0], P("01yet-anothernormalfile"));
	equals(files[1], P("anormalfile"));

	// Files with impossible extension
	files = GetListOfFilesInDir(argv[1], "impossible", true);
	equals(files.size(), 0);

	// Files with impossible or no extension
	files = GetListOfFilesInDir(argv[1], "impossible", true, true);
	equals(files.size(), 2);
	equals(files[0], P("01yet-anothernormalfile"));
	equals(files[1], P("anormalfile"));

	// Files with list extension - nothing more
	files = GetListOfFilesInDir(argv[1], "list", true);
	equals(files.size(), 4);
	equals(files[0], P("01yet-anotherapt.list"));
	equals(files[1], P("anormalapt.list"));
	equals(files[2], P("linkedfile.list"));
	equals(files[3], P("multi.dot.list"));

	// Files with conf or no extension
	files = GetListOfFilesInDir(argv[1], "conf", true, true);
	equals(files.size(), 5);
	equals(files[0], P("01yet-anotherapt.conf"));
	equals(files[1], P("01yet-anothernormalfile"));
	equals(files[2], P("anormalapt.conf"));
	equals(files[3], P("anormalfile"));
	equals(files[4], P("multi.dot.conf"));

	// Files with disabled extension - nothing more
	files = GetListOfFilesInDir(argv[1], "disabled", true);
	equals(files.size(), 3);
	equals(files[0], P("disabledfile.conf.disabled"));
	equals(files[1], P("disabledfile.disabled"));
	equals(files[2], P("disabledfile.list.disabled"));

	// Files with disabled or no extension
	files = GetListOfFilesInDir(argv[1], "disabled", true, true);
	equals(files.size(), 5);
	equals(files[0], P("01yet-anothernormalfile"));
	equals(files[1], P("anormalfile"));
	equals(files[2], P("disabledfile.conf.disabled"));
	equals(files[3], P("disabledfile.disabled"));
	equals(files[4], P("disabledfile.list.disabled"));

	return 0;
}
