// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Provide access methods to various configuration settings,
   setup defaults and returns validate settings.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <string>
#include <vector>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

									/*}}}*/
namespace APT {
// setDefaultConfigurationForCompressors				/*{{{*/
static void setDefaultConfigurationForCompressors() {
	// Set default application paths to check for optional compression types
	_config->CndSet("Dir::Bin::gzip", "/bin/gzip");
	_config->CndSet("Dir::Bin::bzip2", "/bin/bzip2");
	_config->CndSet("Dir::Bin::xz", "/usr/bin/xz");
	_config->CndSet("Dir::Bin::lz4", "/usr/bin/lz4");
	_config->CndSet("Dir::Bin::zstd", "/usr/bin/zstd");
	if (FileExists(_config->Find("Dir::Bin::xz")) == true) {
		_config->Set("Dir::Bin::lzma", _config->Find("Dir::Bin::xz"));
		_config->Set("APT::Compressor::lzma::Binary", "xz");
		if (_config->Exists("APT::Compressor::lzma::CompressArg") == false) {
			_config->Set("APT::Compressor::lzma::CompressArg::", "--format=lzma");
			_config->Set("APT::Compressor::lzma::CompressArg::", "-6");
		}
		if (_config->Exists("APT::Compressor::lzma::UncompressArg") == false) {
			_config->Set("APT::Compressor::lzma::UncompressArg::", "--format=lzma");
			_config->Set("APT::Compressor::lzma::UncompressArg::", "-d");
		}
	} else {
		_config->CndSet("Dir::Bin::lzma", "/usr/bin/lzma");
		if (_config->Exists("APT::Compressor::lzma::CompressArg") == false) {
			_config->Set("APT::Compressor::lzma::CompressArg::", "--suffix=");
			_config->Set("APT::Compressor::lzma::CompressArg::", "-6");
		}
		if (_config->Exists("APT::Compressor::lzma::UncompressArg") == false) {
			_config->Set("APT::Compressor::lzma::UncompressArg::", "--suffix=");
			_config->Set("APT::Compressor::lzma::UncompressArg::", "-d");
		}
	}
	// setup the defaults for the compressiontypes => method mapping
	_config->CndSet("Acquire::CompressionTypes::xz","xz");
	_config->CndSet("Acquire::CompressionTypes::bz2","bzip2");
	_config->CndSet("Acquire::CompressionTypes::lzma","lzma");
	_config->CndSet("Acquire::CompressionTypes::gz","gzip");
	_config->CndSet("Acquire::CompressionTypes::lz4","lz4");
	_config->CndSet("Acquire::CompressionTypes::zst", "zstd");
}
									/*}}}*/
// getCompressionTypes - Return Vector of usable compressiontypes	/*{{{*/
// ---------------------------------------------------------------------
/* return a vector of compression types in the preferred order. */
std::vector<std::string>
const Configuration::getCompressionTypes(bool const &Cached) {
	static std::vector<std::string> types;
	if (types.empty() == false) {
		if (Cached == true)
			return types;
		else
			types.clear();
	}

	std::vector<APT::Configuration::Compressor> const compressors = getCompressors();

	// load the order setting into our vector
	std::vector<std::string> const order = _config->FindVector("Acquire::CompressionTypes::Order");
	for (std::vector<std::string>::const_iterator o = order.begin();
	     o != order.end(); ++o) {
		if ((*o).empty() == true)
			continue;
		// ignore types we have no method ready to use
		std::string const method = std::string("Acquire::CompressionTypes::").append(*o);
		if (_config->Exists(method) == false)
			continue;
		// ignore types we have no app ready to use
		std::string const app = _config->Find(method);
		if (std::find_if(compressors.begin(), compressors.end(), [&app](APT::Configuration::Compressor const &c) {
				return c.Name == app;
			}) == compressors.end())
			continue;
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
		if (std::find_if(compressors.begin(), compressors.end(), [&Types](APT::Configuration::Compressor const &c) {
				return c.Name == Types->Value;
			}) == compressors.end())
			continue;
		types.push_back(Types->Tag);
	}

	// add the special "uncompressed" type
	if (std::find(types.begin(), types.end(), "uncompressed") == types.end())
	{
		std::string const uncompr = _config->Find("Dir::Bin::uncompressed", "");
		if (uncompr.empty() == true || FileExists(uncompr) == true)
			types.push_back("uncompressed");
	}

	return types;
}
									/*}}}*/
// GetLanguages - Return Vector of Language Codes			/*{{{*/
// ---------------------------------------------------------------------
/* return a vector of language codes in the preferred order.
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
	if (D != NULL) {
		builtin.push_back("none");
		for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D)) {
			string const name = SubstVar(Ent->d_name, "%5f", "_");
			size_t const foundDash = name.rfind("-");
			size_t const foundUnderscore = name.rfind("_", foundDash);
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
				if (isalpha(*s) == 0 && *s != '_')
					break;
			}
			if (s != c.end())
				continue;
			if (std::find(builtin.begin(), builtin.end(), c) != builtin.end())
				continue;
			builtin.push_back(c);
		}
		closedir(D);
	}

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
	string const envMsg = string(Locale == 0 ? ::setlocale(LC_MESSAGES, NULL) : *Locale);
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
		// cornercase: LANG=C, so we use only "en" Translation
		environment.push_back("en");
	}

	std::vector<string> const lang = _config->FindVector("Acquire::Languages", "environment,en");
	// the configs define the order, so add the environment
	// then needed and ensure the codes are not listed twice.
	bool noneSeen = false;
	for (std::vector<string>::const_iterator l = lang.begin();
	     l != lang.end(); ++l) {
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

	if (allCodes.empty() == false) {
		for (std::vector<string>::const_iterator b = builtin.begin();
		     b != builtin.end(); ++b)
			if (std::find(allCodes.begin(), allCodes.end(), *b) == allCodes.end())
				allCodes.push_back(*b);
	} else {
		// "none" was forced
		allCodes.push_back("none");
	}

	if (All == true)
		return allCodes;
	else
		return codes;
}
									/*}}}*/
// checkLanguage - are we interested in the given Language?		/*{{{*/
bool Configuration::checkLanguage(std::string Lang, bool const All) {
	// the empty Language is always interesting as it is the original
	if (Lang.empty() == true)
		return true;
	// filenames are encoded, so undo this
	Lang = SubstVar(Lang, "%5f", "_");
	std::vector<std::string> const langs = getLanguages(All, true);
	return (std::find(langs.begin(), langs.end(), Lang) != langs.end());
}
									/*}}}*/
// getArchitectures - Return Vector of preferred Architectures		/*{{{*/
std::vector<std::string> const Configuration::getArchitectures(bool const &Cached) {
	std::vector<std::string> static archs;
	if (likely(Cached == true) && archs.empty() == false)
		return archs;

	std::string const arch = _config->Find("APT::Architecture");
	archs = _config->FindVector("APT::Architectures");

	if (archs.empty() == true && _system != nullptr)
	   archs = _system->ArchitecturesSupported();

	if (archs.empty() == true ||
	    std::find(archs.begin(), archs.end(), arch) == archs.end())
		archs.insert(archs.begin(), arch);

	// erase duplicates, empty strings and very foreign architectures
	auto newend = std::remove_if(archs.begin(), archs.end(), [](auto const &a) { return a.empty(); });
	for (auto a = archs.begin(); a != newend; ++a)
		newend = std::remove(std::next(a), newend, *a);
	for (auto const &f : _config->FindVector("APT::BarbarianArchitectures"))
		newend = std::remove(archs.begin(), newend, f);
	archs.erase(newend, archs.end());
	return archs;
}
									/*}}}*/
// checkArchitecture - are we interested in the given Architecture?	/*{{{*/
bool Configuration::checkArchitecture(std::string const &Arch) {
	if (Arch == "all")
		return true;
	std::vector<std::string> const archs = getArchitectures(true);
	return (std::find(archs.begin(), archs.end(), Arch) != archs.end());
}
									/*}}}*/
// getCompressors - Return Vector of usealbe compressors		/*{{{*/
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

	std::vector<std::string> CompressorsDone;
# define APT_ADD_COMPRESSOR(NAME, EXT, BINARY, ARG, DEARG, COST) \
	{ CompressorsDone.push_back(NAME); compressors.emplace_back(NAME, EXT, BINARY, ARG, DEARG, COST); }
	APT_ADD_COMPRESSOR(".", "", "", nullptr, nullptr, 0)
	if (_config->Exists("Dir::Bin::zstd") == false || FileExists(_config->Find("Dir::Bin::zstd")) == true)
	   APT_ADD_COMPRESSOR("zstd", ".zst", "zstd", "-19", "-d", 60)
#ifdef HAVE_ZSTD
	else
	   APT_ADD_COMPRESSOR("zstd", ".zst", "false", nullptr, nullptr, 60)
#endif
	if (_config->Exists("Dir::Bin::lz4") == false || FileExists(_config->Find("Dir::Bin::lz4")) == true)
		APT_ADD_COMPRESSOR("lz4",".lz4","lz4","-1","-d",50)
#ifdef HAVE_LZ4
	else
		APT_ADD_COMPRESSOR("lz4",".lz4","false", nullptr, nullptr, 50)
#endif
	if (_config->Exists("Dir::Bin::gzip") == false || FileExists(_config->Find("Dir::Bin::gzip")) == true)
		APT_ADD_COMPRESSOR("gzip",".gz","gzip","-6n","-d",100)
#ifdef HAVE_ZLIB
	else
		APT_ADD_COMPRESSOR("gzip",".gz","false", nullptr, nullptr, 100)
#endif
	if (_config->Exists("Dir::Bin::xz") == false || FileExists(_config->Find("Dir::Bin::xz")) == true)
		APT_ADD_COMPRESSOR("xz",".xz","xz","-6","-d",200)
#ifdef HAVE_LZMA
	else
		APT_ADD_COMPRESSOR("xz",".xz","false", nullptr, nullptr, 200)
#endif
	if (_config->Exists("Dir::Bin::bzip2") == false || FileExists(_config->Find("Dir::Bin::bzip2")) == true)
		APT_ADD_COMPRESSOR("bzip2",".bz2","bzip2","-6","-d",300)
#ifdef HAVE_BZ2
	else
		APT_ADD_COMPRESSOR("bzip2",".bz2","false", nullptr, nullptr, 300)
#endif
	if (_config->Exists("Dir::Bin::lzma") == false || FileExists(_config->Find("Dir::Bin::lzma")) == true)
		APT_ADD_COMPRESSOR("lzma",".lzma","lzma","-6","-d",400)
#ifdef HAVE_LZMA
	else
		APT_ADD_COMPRESSOR("lzma",".lzma","false", nullptr, nullptr, 400)
#endif

	std::vector<std::string> const comp = _config->FindVector("APT::Compressor", "", true);
	for (auto const &c: comp)
	{
		if (c.empty() || std::find(CompressorsDone.begin(), CompressorsDone.end(), c) != CompressorsDone.end())
			continue;
		compressors.push_back(Compressor(c.c_str(), std::string(".").append(c).c_str(), c.c_str(), nullptr, nullptr, 1000));
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
	std::string const config = std::string("APT::Compressor::").append(name).append("::");
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
// getBuildProfiles - return a vector of enabled build profiles		/*{{{*/
std::vector<std::string> const Configuration::getBuildProfiles() {
	// order is: override value (~= commandline), environment variable, list (~= config file)
	std::string profiles_env = getenv("DEB_BUILD_PROFILES") == 0 ? "" : getenv("DEB_BUILD_PROFILES");
	if (profiles_env.empty() == false) {
		profiles_env = SubstVar(profiles_env, " ", ",");
		std::string const bp = _config->Find("APT::Build-Profiles");
		_config->Clear("APT::Build-Profiles");
		if (bp.empty() == false)
			_config->Set("APT::Build-Profiles", bp);
	}
	return _config->FindVector("APT::Build-Profiles", profiles_env);
}
std::string const Configuration::getBuildProfilesString() {
	std::vector<std::string> profiles = getBuildProfiles();
	if (profiles.empty() == true)
		return "";
	std::vector<std::string>::const_iterator p = profiles.begin();
	std::string list = *p;
	for (++p; p != profiles.end(); ++p)
	   list.append(",").append(*p);
	return list;
}
									/*}}}*/

// getMachineID - read /etc/machine-id into a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
std::string const Configuration::getMachineID()
{
   std::string id = _config->Find("APT::Machine-ID");

   if (id.empty())
   {
      std::string file = _config->FindFile("Dir::Etc::machine-id");

      if (file.empty())
      {
	 file = flCombine(_config->FindDir("Dir::Etc"), "../machine-id");
      }
      FileFd fd;
      _error->PushToStack();

      if (not OpenConfigurationFileFd(file, fd) || not fd.ReadLine(id))
      {
	 if (_config->FindB("Debug::Phasing", false))
	 {
	    _error->DumpErrors(std::clog);
	 }
      }

      _error->RevertToStack();
   }

   return id;
}
									/*}}}*/
// isChroot - whether we are inside a chroot			     	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Configuration::isChroot()
{
   static struct once
   {
      bool res = false;
      once()
      {
	 pid_t child = ExecFork();
	 if (child == 0)
	 {
	    auto binary = _config->FindFile("Dir::Bin::ischroot", "/usr/bin/ischroot");
	    const char *const Args[] = {
	       binary.c_str(),
	       "-t",
	       nullptr};
	    execvp(Args[0], const_cast<char **>(Args));
	    _exit(127);
	 }

	 res = ExecWait(child, "ischroot", true);
      }
   } once;

   return once.res;
}
									/*}}}*/
}
