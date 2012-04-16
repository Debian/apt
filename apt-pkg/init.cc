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

#include <cstdlib>
#include <sys/stat.h>

#include <apti18n.h>
									/*}}}*/

#define Stringfy_(x) # x
#define Stringfy(x)  Stringfy_(x)
const char *pkgVersion = PACKAGE_VERSION;
const char *pkgLibVersion = Stringfy(APT_PKG_MAJOR) "."
                            Stringfy(APT_PKG_MINOR) "." 
                            Stringfy(APT_PKG_RELEASE);
    
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
   Cnf.CndSet("Dir::State","var/lib/apt/");
   
   /* Just in case something goes horribly wrong, we can fall back to the
      old /var/state paths.. */
   struct stat St;   
   if (stat("/var/lib/apt/.",&St) != 0 &&
       stat("/var/state/apt/.",&St) == 0)
      Cnf.CndSet("Dir::State","var/state/apt/");
       
   Cnf.CndSet("Dir::State::lists","lists/");
   Cnf.CndSet("Dir::State::cdroms","cdroms.list");
   Cnf.CndSet("Dir::State::mirrors","mirrors/");

   // Cache
   Cnf.CndSet("Dir::Cache","var/cache/apt/");
   Cnf.CndSet("Dir::Cache::archives","archives/");
   Cnf.CndSet("Dir::Cache::srcpkgcache","srcpkgcache.bin");
   Cnf.CndSet("Dir::Cache::pkgcache","pkgcache.bin");
   
   // Configuration
   Cnf.CndSet("Dir::Etc","etc/apt/");
   Cnf.CndSet("Dir::Etc::sourcelist","sources.list");
   Cnf.CndSet("Dir::Etc::sourceparts","sources.list.d");
   Cnf.CndSet("Dir::Etc::vendorlist","vendors.list");
   Cnf.CndSet("Dir::Etc::vendorparts","vendors.list.d");
   Cnf.CndSet("Dir::Etc::main","apt.conf");
   Cnf.CndSet("Dir::Etc::netrc", "auth.conf");
   Cnf.CndSet("Dir::Etc::parts","apt.conf.d");
   Cnf.CndSet("Dir::Etc::preferences","preferences");
   Cnf.CndSet("Dir::Etc::preferencesparts","preferences.d");
   Cnf.CndSet("Dir::Etc::trusted", "trusted.gpg");
   Cnf.CndSet("Dir::Etc::trustedparts","trusted.gpg.d");
   Cnf.CndSet("Dir::Bin::methods","/usr/lib/apt/methods");
   Cnf.CndSet("Dir::Bin::solvers::","/usr/lib/apt/solvers");
   Cnf.CndSet("Dir::Media::MountPath","/media/apt");

   // State   
   Cnf.CndSet("Dir::Log","var/log/apt");
   Cnf.CndSet("Dir::Log::Terminal","term.log");
   Cnf.CndSet("Dir::Log::History","history.log");

   Cnf.Set("Dir::Ignore-Files-Silently::", "~$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.disabled$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.bak$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.dpkg-[a-z]+$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.save$");
   Cnf.Set("Dir::Ignore-Files-Silently::", "\\.orig$");

   // Default cdrom mount point
   Cnf.CndSet("Acquire::cdrom::mount", "/media/cdrom/");

   bool Res = true;
   
   // Read an alternate config file
   const char *Cfg = getenv("APT_CONFIG");
   if (Cfg != 0)
   {
      if (RealFileExists(Cfg) == true)
	 Res &= ReadConfigFile(Cnf,Cfg);
      else
	 _error->WarningE("RealFileExists",_("Unable to read %s"),Cfg);
   }

   // Read the configuration parts dir
   std::string Parts = Cnf.FindDir("Dir::Etc::parts");
   if (DirectoryExists(Parts) == true)
      Res &= ReadConfigDir(Cnf,Parts);
   else
      _error->WarningE("DirectoryExists",_("Unable to read %s"),Parts.c_str());

   // Read the main config file
   std::string FName = Cnf.FindFile("Dir::Etc::main");
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
   
   return Sys->Initialize(Cnf);
}
									/*}}}*/
