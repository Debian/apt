// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.cc,v 1.12 1998/11/06 02:52:21 jgg Exp $
/* ######################################################################

   Init - Initialize the package library
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include <apt-pkg/init.h>
#include <apt-pkg/fileutl.h>
#include <config.h>
									/*}}}*/

// pkgInitialize - Initialize the configuration class			/*{{{*/
// ---------------------------------------------------------------------
/* Directories are specified in such a way that the FindDir function will
   understand them. That is, if they don't start with a / then their parent
   is prepended, this allows a fair degree of flexability. */
bool pkgInitialize(Configuration &Cnf)
{
   // General APT things
   Cnf.Set("APT::Architecture",ARCHITECTURE);

   // State
   Cnf.Set("Dir::State","/var/state/apt/");
   Cnf.Set("Dir::State::lists","lists/");
   
   /* These really should be jammed into a generic 'Local Database' engine
      which is yet to be determined. The functions in pkgcachegen should
      be the only users of these */
   Cnf.Set("Dir::State::xstatus","xstatus");
   Cnf.Set("Dir::State::userstatus","status.user");   
   Cnf.Set("Dir::State::status","/var/lib/dpkg/status");
   
   // Cache
   Cnf.Set("Dir::Cache","/var/cache/apt/");
   Cnf.Set("Dir::Cache::archives","archives/");
   Cnf.Set("Dir::Cache::srcpkgcache","srcpkgcache.bin");
   Cnf.Set("Dir::Cache::pkgcache","pkgcache.bin");
   
   // Configuration
   Cnf.Set("Dir::Etc","/etc/apt/");
   Cnf.Set("Dir::Etc::sourcelist","sources.list");
   Cnf.Set("Dir::Etc::main","apt.conf");
   Cnf.Set("Dir::Bin::methods","/usr/lib/apt/methods");
   
   // Read the main config file
   string FName = Cnf.FindFile("Dir::Etc::main");
   bool Res = true;
   if (FileExists(FName) == true)
      Res &= ReadConfigFile(Cnf,FName);
   
   // Read an alternate config file
   const char *Cfg = getenv("APT_CONFIG");
   if (Cfg != 0 && FileExists(Cfg) == true)
      Res &= ReadConfigFile(Cnf,Cfg);
   
   if (Res == false)
      return false;
   
   if (Cnf.FindB("Debug::pkgInitialize",false) == true)
      Cnf.Dump();
      
   return true;
}
									/*}}}*/
