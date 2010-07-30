#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>

#include <string>

#include "assert.h"

class CLT: public CommandLine {

	public:
	std::string static AsString(const char * const * const argv,
				    unsigned int const argc) {
		std::string const static conf = "Commandline::AsString";
		_config->Clear(conf);
		SaveInConfig(argc, argv);
		return _config->Find(conf);
	}
};

#define CMD(y,z) equals(CLT::AsString(argv, y), z);

int main() {
	{
		const char* const argv[] = {"apt-get", "install", "-sf"};
		CMD(3, "apt-get install -sf");
	}
	{
		const char* const argv[] = {"apt-cache", "-s", "apt", "-so", "Debug::test=Test"};
		CMD(5, "apt-cache -s apt -so Debug::test=Test");
	}
	{
		const char* const argv[] = {"apt-cache", "-s", "apt", "-so", "Debug::test=Das ist ein Test"};
		CMD(5, "apt-cache -s apt -so Debug::test=\"Das ist ein Test\"");
	}
	{
		const char* const argv[] = {"apt-cache", "-s", "apt", "--hallo", "test=1.0"};
		CMD(5, "apt-cache -s apt --hallo test=1.0");
	}
}
