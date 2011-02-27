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
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>

#include <sys/types.h>
#include <dirent.h>

#include <algorithm>
#include <string>
#include <vector>
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
	_config->CndSet("Acquire::CompressionTypes::xz","xz");
	_config->CndSet("Acquire::CompressionTypes::lzma","lzma");
	_config->CndSet("Acquire::CompressionTypes::gz","gzip");

	setDefaultConfigurationForCompressors();

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

	// add the special "uncompressed" type
	if (std::find(types.begin(), types.end(), "uncompressed") == types.end())
	{
		string const uncompr = _config->FindFile("Dir::Bin::uncompressed", "");
		if (uncompr.empty() == true || FileExists(uncompr) == true)
			types.push_back("uncompressed");
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

	// Include all Language codes we have a Translation file for in /var/lib/apt/lists
	// so they will be all included in the Cache.
	std::vector<string> builtin;
	DIR *D = opendir(_config->FindDir("Dir::State::lists").c_str());
	if (D != 0) {
		builtin.push_back("none");
		for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D)) {
			string const name = Ent->d_name;
			size_t const foundDash = name.rfind("-");
			size_t const foundUnderscore = name.rfind("_");
			if (foundDash == string::npos || foundUnderscore == string::npos ||
			    foundDash <= foundUnderscore ||
			    name.substr(foundUnderscore+1, foundDash-(foundUnderscore+1)) != "Translation")
				continue;
			string const c = name.substr(foundDash+1);
			if (unlikely(c.empty() == true) || c == "en")
				continue;
			// Skip unusual files, like backups or that alike
			string::const_iterator s = c.begin();
			for (;s != c.end(); ++s) {
				if (isalpha(*s) == 0)
					break;
			}
			if (s != c.end())
				continue;
			if (std::find(builtin.begin(), builtin.end(), c) != builtin.end())
				continue;
			builtin.push_back(c);
		}
	}
	closedir(D);

	// FIXME: Remove support for the old APT::Acquire::Translation
	// it was undocumented and so it should be not very widthly used
	string const oldAcquire = _config->Find("APT::Acquire::Translation","");
	if (oldAcquire.empty() == false && oldAcquire != "environment") {
		// TRANSLATORS: the two %s are APT configuration options
		_error->Notice("Option '%s' is deprecated. Please use '%s' instead, see 'man 5 apt.conf' for details.",
				"APT::Acquire::Translation", "Acquire::Languages");
		if (oldAcquire != "none")
			codes.push_back(oldAcquire);
		codes.push_back("en");
		allCodes = codes;
		for (std::vector<string>::const_iterator b = builtin.begin();
		     b != builtin.end(); ++b)
			if (std::find(allCodes.begin(), allCodes.end(), *b) == allCodes.end())
				allCodes.push_back(*b);
		if (All == true)
			return allCodes;
		else
			return codes;
	}

	// get the environment language codes: LC_MESSAGES (and later LANGUAGE)
	// we extract both, a long and a short code and then we will
	// check if we actually need both (rare) or if the short is enough
	string const envMsg = string(Locale == 0 ? std::setlocale(LC_MESSAGES, NULL) : *Locale);
	size_t const lenShort = (envMsg.find('_') != string::npos) ? envMsg.find('_') : 2;
	size_t const lenLong = (envMsg.find_first_of(".@") != string::npos) ? envMsg.find_first_of(".@") : (lenShort + 3);

	string const envLong = envMsg.substr(0,lenLong);
	string const envShort = envLong.substr(0,lenShort);

	// It is very likely we will need the environment codes later,
	// so let us generate them now from LC_MESSAGES and LANGUAGE
	std::vector<string> environment;
	if (envShort != "C") {
		// take care of LC_MESSAGES
		if (envLong != envShort)
			environment.push_back(envLong);
		environment.push_back(envShort);
		// take care of LANGUAGE
		const char *language_env = getenv("LANGUAGE") == 0 ? "" : getenv("LANGUAGE");
		string envLang = Locale == 0 ? language_env : *(Locale+1);
		if (envLang.empty() == false) {
			std::vector<string> env = VectorizeString(envLang,':');
			short addedLangs = 0; // add a maximum of 3 fallbacks from the environment
			for (std::vector<string>::const_iterator e = env.begin();
			     e != env.end() && addedLangs < 3; ++e) {
				if (unlikely(e->empty() == true) || *e == "en")
					continue;
				if (*e == envLong || *e == envShort)
					continue;
				if (std::find(environment.begin(), environment.end(), *e) != environment.end())
					continue;
				++addedLangs;
				environment.push_back(*e);
			}
		}
	} else {
		environment.push_back("en");
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
		for (std::vector<string>::const_iterator b = builtin.begin();
		     b != builtin.end(); ++b)
			if (std::find(allCodes.begin(), allCodes.end(), *b) == allCodes.end())
				allCodes.push_back(*b);
		if (All == true)
			return allCodes;
		else
			return codes;
	}

	// cornercase: LANG=C, so we use only "en" Translation
	if (envShort == "C") {
		allCodes = codes = environment;
		allCodes.insert(allCodes.end(), builtin.begin(), builtin.end());
		if (All == true)
			return allCodes;
		else
			return codes;
	}

	std::vector<string> const lang = _config->FindVector("Acquire::Languages");
	// the default setting -> "environment, en"
	if (lang.empty() == true) {
		codes = environment;
		if (envShort != "en")
			codes.push_back("en");
		allCodes = codes;
		for (std::vector<string>::const_iterator b = builtin.begin();
		     b != builtin.end(); ++b)
			if (std::find(allCodes.begin(), allCodes.end(), *b) == allCodes.end())
				allCodes.push_back(*b);
		if (All == true)
			return allCodes;
		else
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

	for (std::vector<string>::const_iterator b = builtin.begin();
	     b != builtin.end(); ++b)
		if (std::find(allCodes.begin(), allCodes.end(), *b) == allCodes.end())
			allCodes.push_back(*b);

	if (All == true)
		return allCodes;
	else
		return codes;
}
									/*}}}*/
// getArchitectures - Return Vector of prefered Architectures		/*{{{*/
std::vector<std::string> const Configuration::getArchitectures(bool const &Cached) {
	using std::string;

	std::vector<string> static archs;
	if (likely(Cached == true) && archs.empty() == false)
		return archs;

	archs = _config->FindVector("APT::Architectures");
	string const arch = _config->Find("APT::Architecture");
	if (unlikely(arch.empty() == true))
		return archs;

	if (archs.empty() == true ||
	    std::find(archs.begin(), archs.end(), arch) == archs.end())
		archs.push_back(arch);

	// erase duplicates and empty strings
	for (std::vector<string>::reverse_iterator a = archs.rbegin();
	     a != archs.rend(); ++a) {
		if (a->empty() == true || std::find(a + 1, archs.rend(), *a) != archs.rend())
			archs.erase(a.base()-1);
		if (a == archs.rend())
			break;
	}

	return archs;
}
									/*}}}*/
// checkArchitecture - are we interested in the given Architecture?	/*{{{*/
bool const Configuration::checkArchitecture(std::string const &Arch) {
	if (Arch == "all")
		return true;
	std::vector<std::string> const archs = getArchitectures(true);
	return (std::find(archs.begin(), archs.end(), Arch) != archs.end());
}
									/*}}}*/
// setDefaultConfigurationForCompressors				/*{{{*/
void Configuration::setDefaultConfigurationForCompressors() {
	// Set default application paths to check for optional compression types
	_config->CndSet("Dir::Bin::lzma", "/usr/bin/lzma");
	_config->CndSet("Dir::Bin::xz", "/usr/bin/xz");
	_config->CndSet("Dir::Bin::bzip2", "/bin/bzip2");
}
									/*}}}*/
// getCompressors - Return Vector of usbale compressors			/*{{{*/
// ---------------------------------------------------------------------
/* return a vector of compressors used by apt-ftparchive in the
   multicompress functionality or to detect data.tar files */
std::vector<APT::Configuration::Compressor>
const Configuration::getCompressors(bool const Cached) {
	static std::vector<APT::Configuration::Compressor> compressors;
	if (compressors.empty() == false) {
		if (Cached == true)
			return compressors;
		else
			compressors.clear();
	}

	setDefaultConfigurationForCompressors();

	compressors.push_back(Compressor(".", "", "", "", "", 1));
	if (_config->Exists("Dir::Bin::gzip") == false || FileExists(_config->FindFile("Dir::Bin::gzip")) == true)
		compressors.push_back(Compressor("gzip",".gz","gzip","-9n","-d",2));
	if (_config->Exists("Dir::Bin::bzip2") == false || FileExists(_config->FindFile("Dir::Bin::bzip2")) == true)
		compressors.push_back(Compressor("bzip2",".bz2","bzip2","-9","-d",3));
	if (_config->Exists("Dir::Bin::lzma") == false || FileExists(_config->FindFile("Dir::Bin::lzma")) == true)
		compressors.push_back(Compressor("lzma",".lzma","lzma","-9","-d",4));
	if (_config->Exists("Dir::Bin::xz") == false || FileExists(_config->FindFile("Dir::Bin::xz")) == true)
		compressors.push_back(Compressor("xz",".xz","xz","-6","-d",5));

	std::vector<std::string> const comp = _config->FindVector("APT::Compressor");
	for (std::vector<std::string>::const_iterator c = comp.begin();
	     c != comp.end(); ++c) {
		if (*c == "." || *c == "gzip" || *c == "bzip2" || *c == "lzma" || *c == "xz")
			continue;
		compressors.push_back(Compressor(c->c_str(), std::string(".").append(*c).c_str(), c->c_str(), "-9", "-d", 100));
	}

	return compressors;
}
									/*}}}*/
// getCompressorExtensions - supported data.tar extensions		/*{{{*/
// ---------------------------------------------------------------------
/* */
std::vector<std::string> const Configuration::getCompressorExtensions() {
	std::vector<APT::Configuration::Compressor> const compressors = getCompressors();
	std::vector<std::string> ext;
	for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressors.begin();
	     c != compressors.end(); ++c)
		if (c->Extension.empty() == false && c->Extension != ".")
			ext.push_back(c->Extension);
	return ext;
}
									/*}}}*/
// Compressor constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
Configuration::Compressor::Compressor(char const *name, char const *extension,
				      char const *binary,
				      char const *compressArg, char const *uncompressArg,
				      unsigned short const cost) {
	std::string const config = string("APT:Compressor::").append(name).append("::");
	Name = _config->Find(std::string(config).append("Name"), name);
	Extension = _config->Find(std::string(config).append("Extension"), extension);
	Binary = _config->Find(std::string(config).append("Binary"), binary);
	Cost = _config->FindI(std::string(config).append("Cost"), cost);
	std::string const compConf = std::string(config).append("CompressArg");
	if (_config->Exists(compConf) == true)
		CompressArgs = _config->FindVector(compConf);
	else if (compressArg != NULL)
		CompressArgs.push_back(compressArg);
	std::string const uncompConf = std::string(config).append("UncompressArg");
	if (_config->Exists(uncompConf) == true)
		UncompressArgs = _config->FindVector(uncompConf);
	else if (uncompressArg != NULL)
		UncompressArgs.push_back(uncompressArg);
}
									/*}}}*/
}
