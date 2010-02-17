// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Provide access methods to various configuration settings,
   setup defaults and returns validate settings.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>

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
// GetLanguages - Return Vector of Language Codes			/*{{{*/
// ---------------------------------------------------------------------
/* return a vector of language codes in the prefered order.
   the special word "environment" will be replaced with the long and the short
   code of the local settings and it will be insured that this will not add
   duplicates. So in an german local the setting "environment, de_DE, en, de"
   will result in "de_DE, de, en".
   The special word "none" is the stopcode for the not-All code vector */
std::vector<std::string> const Configuration::getLanguages(bool const &All,
				bool const &Cached, char const ** const Locale) {
	using std::string;

	// The detection is boring and has a lot of cornercases,
	// so we cache the results to calculated it only once.
	std::vector<string> static allCodes;
	std::vector<string> static codes;

	// we have something in the cache
	if (codes.empty() == false || allCodes.empty() == false) {
		if (Cached == true) {
			if(All == true && allCodes.empty() == false)
				return allCodes;
			else
				return codes;
		} else {
			allCodes.clear();
			codes.clear();
		}
	}

	// get the environment language codes: LC_MESSAGES (and later LANGUAGE)
	// we extract both, a long and a short code and then we will
	// check if we actually need both (rare) or if the short is enough
	string const envMsg = string(Locale == 0 ? std::setlocale(LC_MESSAGES, NULL) : *Locale);
	size_t const lenShort = (envMsg.find('_') != string::npos) ? envMsg.find('_') : 2;
	size_t const lenLong = (envMsg.find_first_of(".@") != string::npos) ? envMsg.find_first_of(".@") : (lenShort + 3);

	string envLong = envMsg.substr(0,lenLong);
	string const envShort = envLong.substr(0,lenShort);
	bool envLongIncluded = true;

	// first cornercase: LANG=C, so we use only "en" Translation
	if (envLong == "C") {
		codes.push_back("en");
		allCodes = codes;
		return codes;
	}

	// to save the servers from unneeded queries, we only try also long codes
	// for languages it is realistic to have a long code translation file…
	// TODO: Improve translation acquire system to drop them dynamic
	char const *needLong[] = { "cs", "en", "pt", "sv", "zh", NULL };
	if (envLong != envShort) {
		for (char const **l = needLong; *l != NULL; l++)
			if (envShort.compare(*l) == 0) {
				envLongIncluded = false;
				break;
			}
	}

	// we don't add the long code, but we allow the user to do so
	if (envLongIncluded == true)
		envLong.clear();

	// FIXME: Remove support for the old APT::Acquire::Translation
	// it was undocumented and so it should be not very widthly used
	string const oldAcquire = _config->Find("APT::Acquire::Translation","");
	if (oldAcquire.empty() == false && oldAcquire != "environment") {
		if (oldAcquire != "none")
			codes.push_back(oldAcquire);
		allCodes = codes;
		return codes;
	}

	// It is very likely we will need to environment codes later,
	// so let us generate them now from LC_MESSAGES and LANGUAGE
	std::vector<string> environment;
	// take care of LC_MESSAGES
	if (envLongIncluded == false)
		environment.push_back(envLong);
	environment.push_back(envShort);
	// take care of LANGUAGE
	string envLang = Locale == 0 ? getenv("LANGUAGE") : *(Locale+1);
	if (envLang.empty() == false) {
		std::vector<string> env = ExplodeString(envLang,':');
		short addedLangs = 0; // add a maximum of 3 fallbacks from the environment
		for (std::vector<string>::const_iterator e = env.begin();
		     e != env.end() && addedLangs < 3; ++e) {
			if (unlikely(e->empty() == true) || *e == "en")
				continue;
			if (*e == envLong || *e == envShort)
				continue;
			if (std::find(environment.begin(), environment.end(), *e) != environment.end())
				continue;
			if (e->find('_') != string::npos) {
				// Drop LongCodes here - ShortCodes are also included
				string const shorty = e->substr(0, e->find('_'));
				char const **n = needLong;
				for (; *n != NULL; ++n)
					if (shorty == *n)
						break;
				if (*n == NULL)
					continue;
			}
			++addedLangs;
			environment.push_back(*e);
		}
	}

	// Support settings like Acquire::Translation=none on the command line to
	// override the configuration settings vector of languages.
	string const forceLang = _config->Find("Acquire::Languages","");
	if (forceLang.empty() == false) {
		if (forceLang == "environment") {
			codes = environment;
		} else if (forceLang != "none")
			codes.push_back(forceLang);
		allCodes = codes;
		return codes;
	}

	std::vector<string> const lang = _config->FindVector("Acquire::Languages");
	// the default setting -> "environment, en"
	if (lang.empty() == true) {
		codes = environment;
		if (envShort != "en")
			codes.push_back("en");
		allCodes = codes;
		return codes;
	}

	// the configs define the order, so add the environment
	// then needed and ensure the codes are not listed twice.
	bool noneSeen = false;
	for (std::vector<string>::const_iterator l = lang.begin();
	     l != lang.end(); l++) {
		if (*l == "environment") {
			for (std::vector<string>::const_iterator e = environment.begin();
			     e != environment.end(); ++e) {
				if (std::find(allCodes.begin(), allCodes.end(), *e) != allCodes.end())
					continue;
				if (noneSeen == false)
					codes.push_back(*e);
				allCodes.push_back(*e);
			}
			continue;
		} else if (*l == "none") {
			noneSeen = true;
			continue;
		} else if (std::find(allCodes.begin(), allCodes.end(), *l) != allCodes.end())
			continue;

		if (noneSeen == false)
			codes.push_back(*l);
		allCodes.push_back(*l);
	}
	if (All == true)
		return allCodes;
	else
		return codes;
}
									/*}}}*/
}
