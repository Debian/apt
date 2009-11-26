#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

#include <assert.h>
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
	std::vector<std::string> vec = APT::Configuration::getLanguages(false, false, "de_DE.UTF-8");
	assert(vec.size() == 2);
	assert(vec[0] == "de");
	assert(vec[1] == "en");

	// Special: Check if the cache is actually in use
		vec = APT::Configuration::getLanguages(false, true, "en_GB.UTF-8");
		assert(vec.size() == 2);
		assert(vec[0] == "de");
		assert(vec[1] == "en");

	vec = APT::Configuration::getLanguages(false, false, "en_GB.UTF-8");
	assert(vec.size() == 2);
	assert(vec[0] == "en_GB");
	assert(vec[1] == "en");

	vec = APT::Configuration::getLanguages(false, false, "pt_PR.UTF-8");
	assert(vec.size() == 3);
	assert(vec[0] == "pt_PR");
	assert(vec[1] == "pt");
	assert(vec[2] == "en");

	vec = APT::Configuration::getLanguages(false, false, "ast_DE.UTF-8"); // bogus, but syntactical correct
	assert(vec.size() == 2);
	assert(vec[0] == "ast");
	assert(vec[1] == "en");

	vec = APT::Configuration::getLanguages(false, false, "C");
	assert(vec.size() == 1);
	assert(vec[0] == "en");

	_config->Set("Acquire::Languages::1", "environment");
	_config->Set("Acquire::Languages::2", "en");
	vec = APT::Configuration::getLanguages(false, false, "de_DE.UTF-8");
	assert(vec.size() == 2);
	assert(vec[0] == "de");
	assert(vec[1] == "en");

	_config->Set("Acquire::Languages::3", "de");
	vec = APT::Configuration::getLanguages(false, false, "de_DE.UTF-8");
	assert(vec.size() == 2);
	assert(vec[0] == "de");
	assert(vec[1] == "en");

	_config->Set("Acquire::Languages::1", "none");
	vec = APT::Configuration::getLanguages(false, false, "de_DE.UTF-8");
	assert(vec.size() == 0);
	vec = APT::Configuration::getLanguages(true, false, "de_DE.UTF-8");
	assert(vec[0] == "en");
	assert(vec[1] == "de");

	_config->Set("Acquire::Languages::1", "fr");
	_config->Set("Acquire::Languages", "de_DE");
	vec = APT::Configuration::getLanguages(false, false, "de_DE.UTF-8");
	assert(vec.size() == 1);
	assert(vec[0] == "de_DE");

	_config->Set("Acquire::Languages", "none");
	vec = APT::Configuration::getLanguages(true, false, "de_DE.UTF-8");
	assert(vec.size() == 0);

	_config->Set("Acquire::Languages", "");
	//FIXME: Remove support for this deprecated setting
		_config->Set("APT::Acquire::Translation", "ast_DE");
		vec = APT::Configuration::getLanguages(true, false, "de_DE.UTF-8");
		assert(vec.size() == 1);
		assert(vec[0] == "ast_DE");
		_config->Set("APT::Acquire::Translation", "none");
		vec = APT::Configuration::getLanguages(true, false, "de_DE.UTF-8");
		assert(vec.size() == 0);

	return 0;
}
