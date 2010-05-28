#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

#include "assert.h"
#include <string>
#include <vector>

#include <iostream>

// simple helper to quickly output a vector of strings
void dumpVector(std::vector<std::string> vec) {
	for (std::vector<std::string>::const_iterator v = vec.begin();
	     v != vec.end(); v++)
		std::cout << *v << std::endl;
}

int main(int argc,char *argv[])
{
	std::vector<std::string> vec;

	_config->Set("APT::Architectures::1", "i386");
	_config->Set("APT::Architectures::2", "amd64");
	vec = APT::Configuration::getArchitectures(false);
	equals(vec.size(), 2);
	equals(vec[0], "i386");
	equals(vec[1], "amd64");

	_config->Set("APT::Architecture", "i386");
	vec = APT::Configuration::getArchitectures(false);
	equals(vec.size(), 2);
	equals(vec[0], "i386");
	equals(vec[1], "amd64");

	_config->Set("APT::Architectures::2", "");
	vec = APT::Configuration::getArchitectures(false);
	equals(vec.size(), 1);
	equals(vec[0], "i386");

	_config->Set("APT::Architecture", "armel");
	vec = APT::Configuration::getArchitectures(false);
	equals(vec.size(), 2);
	equals(vec[0], "i386");
	equals(vec[1], "armel");

	_config->Set("APT::Architectures::2", "amd64");
	_config->Set("APT::Architectures::3", "i386");
	_config->Set("APT::Architectures::4", "armel");
	_config->Set("APT::Architectures::5", "i386");
	_config->Set("APT::Architectures::6", "amd64");
	_config->Set("APT::Architectures::7", "armel");
	_config->Set("APT::Architectures::8", "armel");
	_config->Set("APT::Architectures::9", "amd64");
	_config->Set("APT::Architectures::10", "amd64");
	vec = APT::Configuration::getArchitectures(false);
	equals(vec.size(), 3);
	equals(vec[0], "i386");
	equals(vec[1], "amd64");
	equals(vec[2], "armel");

	return 0;
}
