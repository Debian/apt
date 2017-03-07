// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.cc,v 1.20 2003/02/09 20:31:05 doogie Exp $
/* ######################################################################

   Init - Initialize the package library
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/init.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/macros.h>

#include <string.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <apti18n.h>
									/*}}}*/

#define Stringfy_(x) # x
#define Stringfy(x)  Stringfy_(x)
const char *pkgVersion = PACKAGE_VERSION;
const char *pkgLibVersion = Stringfy(APT_PKG_MAJOR) "."
                            Stringfy(APT_PKG_MINOR) "." 
                            Stringfy(APT_PKG_RELEASE);
namespace APT {
   APT_HIDDEN extern std::unordered_map<std::string, std::vector<std::string>> ArchToTupleMap;
}

// Splits by whitespace. There may be continuous spans of whitespace - they
// will be considered as one.
static std::vector<std::string> split(std::string const & s)
{
   std::vector<std::string> vec;
   std::istringstream iss(s);
   iss.imbue(std::locale::classic());
   for(std::string current_s; iss >> current_s; )
      vec.push_back(current_s);
   return vec;
}


// pkgInitArchTupleMap - Initialize the architecture tuple map				/*{{{*/
// ---------------------------------------------------------------------
/* This initializes */
static bool pkgInitArchTupleMap()
{
   auto tuplepath = _config->FindFile("Dir::dpkg::tupletable", DPKG_DATADIR "/tupletable");
   auto tripletpath = _config->FindFile("Dir::dpkg::triplettable", DPKG_DATADIR "/triplettable");
   auto cpupath = _config->FindFile("Dir::dpkg::cputable", DPKG_DATADIR "/cputable");

   // Load a list of CPUs
   std::vector<std::string> cpus;
   std::ifstream cputable(cpupath);
   for (std::string cpuline; std::getline(cputable, cpuline); )
   {
      if (cpuline[0] == '#' || cpuline[0] == '\0')
         continue;
      auto cpurow = split(cpuline);
      auto cpu = APT::String::Strip(cpurow.at(0));

      cpus.push_back(cpu);
   }
   if (!cputable.eof())
      return _error->Error("Error reading the CPU table");

   // Load the architecture tuple
   std::ifstream tupletable;
   if (FileExists(tuplepath))
      tupletable.open(tuplepath);
   else if (FileExists(tripletpath))
      tupletable.open(tripletpath);
   else
      return _error->Error("Cannot find dpkg tuplet or triplet table");

   APT::ArchToTupleMap.clear();
   for (std::string tupleline; std::getline(tupletable, tupleline); )
   {
      if (tupleline[0] == '#' || tupleline[0] == '\0')
         continue;

      std::vector<std::string> tuplerow = split(tupleline);

      auto tuple = APT::String::Strip(tuplerow.at(0));
      auto arch = APT::String::Strip(tuplerow.at(1));

      if (tuple.find("<cpu>") == tuple.npos && arch.find("<cpu>") == arch.npos)
      {
         APT::ArchToTupleMap.insert({arch, VectorizeString(tuple, '-')});
      }
      else
      {
         for (auto && cpu : cpus)
         {
            auto mytuple = SubstVar(tuple, std::string("<cpu>"), cpu);
            auto myarch = SubstVar(arch, std::string("<cpu>"), cpu);

            APT::ArchToTupleMap.insert({myarch, VectorizeString(mytuple, '-')});
         }
      }
   }
   if (!tupletable.eof())
      return _error->Error("Error reading the Tuple table");

   return true;
}
									/*}}}*/

    
// pkgInitConfig - Initialize the configuration class			/*{{{*/
// ---------------------------------------------------------------------
/* Directories are specified in such a way that the FindDir function will
   understand them. That is, if they don't start with a / then their parent
   is prepended, this allows a fair degree of flexability. */
bool pkgInitConfig(Configuration &Cnf)
{
   // General APT things
   Cnf.CndSet("APT::Architecture", COMMON_ARCH);
   if (Cnf.Exists("APT::Build-Essential") == false)
      Cnf.Set("APT::Build-Essential::", "build-essential");
   Cnf.CndSet("APT::Install-Recommends", true);
   Cnf.CndSet("APT::Install-Suggests", false);
   Cnf.CndSet("Dir","/");
   
   // State
   Cnf.CndSet("Dir::State", STATE_DIR + 1);
   Cnf.CndSet("Dir::State::lists","lists/");
   Cnf.CndSet("Dir::State::cdroms","cdroms.list");
   Cnf.CndSet("Dir::State::mirrors","mirrors/");

   // Cache
   Cnf.CndSet("Dir::Cache", CACHE_DIR + 1);
   Cnf.CndSet("Dir::Cache::archives","archives/");
   Cnf.CndSet("Dir::Cache::srcpkgcache","srcpkgcache.bin");
   Cnf.CndSet("Dir::Cache::pkgcache","pkgcache.bin");

   // Configuration
   Cnf.CndSet("Dir::Etc", CONF_DIR + 1);
   Cnf.CndSet("Dir::Etc::sourcelist","sources.list");
   Cnf.CndSet("Dir::Etc::sourceparts","sources.list.d");
   Cnf.CndSet("Dir::Etc::main","apt.conf");
   Cnf.CndSet("Dir::Etc::netrc", "auth.conf");
   Cnf.CndSet("Dir::Etc::parts","apt.conf.d");
   Cnf.CndSet("Dir::Etc::preferences","preferences");
   Cnf.CndSet("Dir::Etc::preferencesparts","preferences.d");
   Cnf.CndSet("Dir::Etc::trusted", "trusted.gpg");
   Cnf.CndSet("Dir::Etc::trustedparts","trusted.gpg.d");
   Cnf.CndSet("Dir::Bin::methods", LIBEXEC_DIR "/methods");
   Cnf.CndSet("Dir::Bin::solvers::",LIBEXEC_DIR  "/solvers");
   Cnf.CndSet("Dir::Bin::planners::",LIBEXEC_DIR  "/planners");
   Cnf.CndSet("Dir::Media::MountPath","/media/apt");

   // State
   Cnf.CndSet("Dir::Log", LOG_DIR + 1);
   Cnf.CndSet("Dir::Log::Terminal","term.log");
   Cnf.CndSet("Dir::Log::History","history.log");
   Cnf.CndSet("Dir::Log::Planner","eipp.log.xz");

   Cnf.Set("Dir::Ignore-Files-Silently::", "~$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.disabled$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.bak$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.dpkg-[a-z]+$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.ucf-[a-z]+$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.save$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.orig$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.distUpgrade$");

   // Repository security
   Cnf.CndSet("Acquire::AllowInsecureRepositories", false);
   Cnf.CndSet("Acquire::AllowWeakRepositories", false);
   Cnf.CndSet("Acquire::AllowDowngradeToInsecureRepositories", false);

   // Default cdrom mount point
   Cnf.CndSet("Acquire::cdrom::mount", "/media/cdrom/");

   // The default user we drop to in the methods
   Cnf.CndSet("APT::Sandbox::User", "_apt");

   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::MetaKey", "$(COMPONENT)/binary-$(ARCHITECTURE)/Packages");
   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::flatMetaKey", "Packages");
   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::ShortDescription", "Packages");
   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::Description", "$(RELEASE)/$(COMPONENT) $(ARCHITECTURE) Packages");
   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::flatDescription", "$(RELEASE) Packages");
   Cnf.CndSet("Acquire::IndexTargets::deb::Packages::Optional", false);
   Cnf.CndSet("Acquire::IndexTargets::deb::Translations::MetaKey", "$(COMPONENT)/i18n/Translation-$(LANGUAGE)");
   Cnf.CndSet("Acquire::IndexTargets::deb::Translations::flatMetaKey", "$(LANGUAGE)");
   Cnf.CndSet("Acquire::IndexTargets::deb::Translations::ShortDescription", "Translation-$(LANGUAGE)");
   Cnf.CndSet("Acquire::IndexTargets::deb::Translations::Description", "$(RELEASE)/$(COMPONENT) Translation-$(LANGUAGE)");
   Cnf.CndSet("Acquire::IndexTargets::deb::Translations::flatDescription", "$(RELEASE) Translation-$(LANGUAGE)");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::MetaKey", "$(COMPONENT)/source/Sources");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::flatMetaKey", "Sources");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::ShortDescription", "Sources");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::Description", "$(RELEASE)/$(COMPONENT) Sources");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::flatDescription", "$(RELEASE) Sources");
   Cnf.CndSet("Acquire::IndexTargets::deb-src::Sources::Optional", false);

   Cnf.CndSet("Acquire::Changelogs::URI::Origin::Debian", "http://metadata.ftp-master.debian.org/changelogs/@CHANGEPATH@_changelog");
   Cnf.CndSet("Acquire::Changelogs::URI::Origin::Tanglu", "http://metadata.tanglu.org/changelogs/@CHANGEPATH@_changelog");
   Cnf.CndSet("Acquire::Changelogs::URI::Origin::Ubuntu", "http://changelogs.ubuntu.com/changelogs/pool/@CHANGEPATH@/changelog");
   Cnf.CndSet("Acquire::Changelogs::URI::Origin::Ultimedia", "http://packages.ultimediaos.com/changelogs/pool/@CHANGEPATH@/changelog.txt");
   Cnf.CndSet("Acquire::Changelogs::AlwaysOnline::Origin::Ubuntu", true);

   bool Res = true;

   // Read an alternate config file
   const char *Cfg = getenv("APT_CONFIG");
   if (Cfg != 0 && strlen(Cfg) != 0)
   {
      if (RealFileExists(Cfg) == true)
	 Res &= ReadConfigFile(Cnf,Cfg);
      else
	 _error->WarningE("RealFileExists",_("Unable to read %s"),Cfg);
   }

   // Read the configuration parts dir
   std::string const Parts = Cnf.FindDir("Dir::Etc::parts", "/dev/null");
   if (DirectoryExists(Parts) == true)
      Res &= ReadConfigDir(Cnf,Parts);
   else if (APT::String::Endswith(Parts, "/dev/null") == false)
      _error->WarningE("DirectoryExists",_("Unable to read %s"),Parts.c_str());

   // Read the main config file
   std::string const FName = Cnf.FindFile("Dir::Etc::main", "/dev/null");
   if (RealFileExists(FName) == true)
      Res &= ReadConfigFile(Cnf,FName);

   if (Res == false)
      return false;

   if (Cnf.FindB("Debug::pkgInitConfig",false) == true)
      Cnf.Dump();
   
#ifdef APT_DOMAIN
   if (Cnf.Exists("Dir::Locale"))
   {  
      bindtextdomain(APT_DOMAIN,Cnf.FindDir("Dir::Locale").c_str());
      bindtextdomain(textdomain(0),Cnf.FindDir("Dir::Locale").c_str());
   }
#endif

   return true;
}
									/*}}}*/
// pkgInitSystem - Initialize the _system calss				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgInitSystem(Configuration &Cnf,pkgSystem *&Sys)
{
   Sys = 0;
   std::string Label = Cnf.Find("Apt::System","");
   if (Label.empty() == false)
   {
      Sys = pkgSystem::GetSystem(Label.c_str());
      if (Sys == 0)
	 return _error->Error(_("Packaging system '%s' is not supported"),Label.c_str());
   }
   else
   {
      signed MaxScore = 0;
      for (unsigned I = 0; I != pkgSystem::GlobalListLen; I++)
      {
	 signed Score = pkgSystem::GlobalList[I]->Score(Cnf);
	 if (Score > MaxScore)
	 {
	    MaxScore = Score;
	    Sys = pkgSystem::GlobalList[I];
	 }
      }
      
      if (Sys == 0)
	 return _error->Error(_("Unable to determine a suitable packaging system type"));
   }

   if (pkgInitArchTupleMap() == false)
      return false;
   
   return Sys->Initialize(Cnf);
}
									/*}}}*/
