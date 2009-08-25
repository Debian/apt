// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Provide access methods to various configuration settings,
   setup defaults and returns validate settings.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

#include <vector>
#include <string>
									/*}}}*/
namespace APT {
// getCompressionTypes - Return Vector of usbale compressiontypes	/*{{{*/
// ---------------------------------------------------------------------
/* return a vector of compression types in the prefered order. */
std::vector<std::string>
const Configuration::getCompressionTypes(bool const &Cached) {
	static std::vector<std::string> types;
	if (types.empty() == false) {
		if (Cached == true)
			return types;
		else
			types.clear();
	}

	// Set default application paths to check for optional compression types
	_config->CndSet("Dir::Bin::lzma", "/usr/bin/lzma");
	_config->CndSet("Dir::Bin::bzip2", "/bin/bzip2");

	::Configuration::Item const *Opts = _config->Tree("Acquire::CompressionTypes");
	if (Opts != 0)
		Opts = Opts->Child;

	// at first, move over the options to setup at least the default options
	bool foundLzma=false, foundBzip2=false, foundGzip=false;
	for (; Opts != 0; Opts = Opts->Next) {
		if (Opts->Value == "lzma")
			foundLzma = true;
		else if (Opts->Value == "bz2")
			foundBzip2 = true;
		else if (Opts->Value == "gz")
			foundGzip = true;
	}

	// setup the defaults now
	if (!foundBzip2)
		_config->Set("Acquire::CompressionTypes::bz2","bzip2");
	if (!foundLzma)
		_config->Set("Acquire::CompressionTypes::lzma","lzma");
	if (!foundGzip)
		_config->Set("Acquire::CompressionTypes::gz","gzip");

	// move again over the option tree to finially calculate our result
	::Configuration::Item const *Types = _config->Tree("Acquire::CompressionTypes");
	if (Types != 0)
		Types = Types->Child;

	for (; Types != 0; Types = Types->Next) {
		string const appsetting = string("Dir::Bin::").append(Types->Value);
		// ignore compression types we have no app ready to use
		if (appsetting.empty() == false && _config->Exists(appsetting) == true) {
			std::string const app = _config->FindFile(appsetting.c_str(), "");
			if (app.empty() == false && FileExists(app) == false)
				continue;
		}
		types.push_back(Types->Tag);
	}

	return types;
}
									/*}}}*/
}
