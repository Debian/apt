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
	if (argc != 2) {
		std::cout << "One parameter expected - given " << argc << std::endl;
		return 100;
	}

	char const* env[2];
	env[0] = "de_DE.UTF-8";
	env[1] = "";

	std::vector<std::string> vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 3);
	equals(vec[0], "de_DE");
	equals(vec[1], "de");
	equals(vec[2], "en");

	// Special: Check if the cache is actually in use
		env[0] = "en_GB.UTF-8";
		vec = APT::Configuration::getLanguages(false, true, env);
		equals(vec.size(), 3);
		equals(vec[0], "de_DE");
		equals(vec[1], "de");
		equals(vec[2], "en");

	env[0] = "en_GB.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 2);
	equals(vec[0], "en_GB");
	equals(vec[1], "en");

	// esperanto
	env[0] = "eo.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 2);
	equals(vec[0], "eo");
	equals(vec[1], "en");

	env[0] = "tr_DE@euro";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 3);
	equals(vec[0], "tr_DE");
	equals(vec[1], "tr");
	equals(vec[2], "en");

	env[0] = "de_NO";
	env[1] = "de_NO:en_GB:nb_NO:nb:no_NO:no:nn_NO:nn:da:sv:en";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 6);
	equals(vec[0], "de_NO");
	equals(vec[1], "de");
	equals(vec[2], "en_GB");
	equals(vec[3], "nb_NO");
	equals(vec[4], "nb");
	equals(vec[5], "en");

	env[0] = "pt_PR.UTF-8";
	env[1] = "";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 3);
	equals(vec[0], "pt_PR");
	equals(vec[1], "pt");
	equals(vec[2], "en");

	env[0] = "ast_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env); // bogus, but syntactical correct
	equals(vec.size(), 3);
	equals(vec[0], "ast_DE");
	equals(vec[1], "ast");
	equals(vec[2], "en");

	env[0] = "C";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 1);
	equals(vec[0], "en");

	_config->Set("Acquire::Languages", "none");
	env[0] = "C";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 0);

	_config->Set("Acquire::Languages", "environment");
	env[0] = "C";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 1);
	equals(vec[0], "en");

	_config->Set("Acquire::Languages", "de");
	env[0] = "C";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 1);
	equals(vec[0], "de");

	_config->Set("Acquire::Languages", "fr");
	env[0] = "ast_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 1);
	equals(vec[0], "fr");
	_config->Set("Acquire::Languages", "");

	_config->Set("Acquire::Languages::1", "environment");
	_config->Set("Acquire::Languages::2", "en");
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 3);
	equals(vec[0], "de_DE");
	equals(vec[1], "de");
	equals(vec[2], "en");

	_config->Set("Acquire::Languages::3", "de");
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 3);
	equals(vec[0], "de_DE");
	equals(vec[1], "de");
	equals(vec[2], "en");

	_config->Set("Dir::State::lists", argv[1]);
	vec = APT::Configuration::getLanguages(true, false, env);
	equals(vec.size(), 6);
	equals(vec[0], "de_DE");
	equals(vec[1], "de");
	equals(vec[2], "en");
	equals(vec[3], "none");
	equals(vec[4], "pt");
	equals(vec[5], "tr");

	_config->Set("Dir::State::lists", "/non-existing-dir");
	_config->Set("Acquire::Languages::1", "none");
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 0);
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(true, false, env);
	equals(vec.size(), 2);
	equals(vec[0], "en");
	equals(vec[1], "de");

	_config->Set("Acquire::Languages::1", "fr");
	_config->Set("Acquire::Languages", "de_DE");
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(false, false, env);
	equals(vec.size(), 1);
	equals(vec[0], "de_DE");

	_config->Set("Acquire::Languages", "none");
	env[0] = "de_DE.UTF-8";
	vec = APT::Configuration::getLanguages(true, false, env);
	equals(vec.size(), 0);

	_config->Set("Acquire::Languages", "");
	//FIXME: Remove support for this deprecated setting
		_config->Set("APT::Acquire::Translation", "ast_DE");
		env[0] = "de_DE.UTF-8";
		vec = APT::Configuration::getLanguages(true, false, env);
		equals(vec.size(), 2);
		equals(vec[0], "ast_DE");
		equals(vec[1], "en");
		_config->Set("APT::Acquire::Translation", "none");
		env[0] = "de_DE.UTF-8";
		vec = APT::Configuration::getLanguages(true, false, env);
		equals(vec.size(), 1);
		equals(vec[0], "en");

	return 0;
}
