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
#include <algorithm>
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

	// setup the defaults for the compressiontypes => method mapping
	_config->CndSet("Acquire::CompressionTypes::bz2","bzip2");
	_config->CndSet("Acquire::CompressionTypes::lzma","lzma");
	_config->CndSet("Acquire::CompressionTypes::gz","gzip");

	// Set default application paths to check for optional compression types
	_config->CndSet("Dir::Bin::lzma", "/usr/bin/lzma");
	_config->CndSet("Dir::Bin::bzip2", "/bin/bzip2");

	// accept non-list order as override setting for config settings on commandline
	std::string const overrideOrder = _config->Find("Acquire::CompressionTypes::Order","");
	if (overrideOrder.empty() == false)
		types.push_back(overrideOrder);

	// load the order setting into our vector
	std::vector<std::string> const order = _config->FindVector("Acquire::CompressionTypes::Order");
	for (std::vector<std::string>::const_iterator o = order.begin();
	     o != order.end(); o++) {
		if ((*o).empty() == true)
			continue;
		// ignore types we have no method ready to use
		if (_config->Exists(string("Acquire::CompressionTypes::").append(*o)) == false)
			continue;
		// ignore types we have no app ready to use
		string const appsetting = string("Dir::Bin::").append(*o);
		if (_config->Exists(appsetting) == true) {
			std::string const app = _config->FindFile(appsetting.c_str(), "");
			if (app.empty() == false && FileExists(app) == false)
				continue;
		}
		types.push_back(*o);
	}

	// move again over the option tree to add all missing compression types
	::Configuration::Item const *Types = _config->Tree("Acquire::CompressionTypes");
	if (Types != 0)
		Types = Types->Child;

	for (; Types != 0; Types = Types->Next) {
		if (Types->Tag == "Order" || Types->Tag.empty() == true)
			continue;
		// ignore types we already have in the vector
		if (std::find(types.begin(),types.end(),Types->Tag) != types.end())
			continue;
		// ignore types we have no app ready to use
		string const appsetting = string("Dir::Bin::").append(Types->Value);
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
