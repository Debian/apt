// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.cc,v 1.7 1998/10/09 19:57:21 jgg Exp $
/* ######################################################################

   Init - Initialize the package library
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include <apt-pkg/init.h>

#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

// pkgInitialize - Initialize the configuration class			/*{{{*/
// ---------------------------------------------------------------------
/* Directories are specified in such a way that the FindDir function will
   understand them. That is, if they don't start with a / then their parent
   is prepended, this allows a fair degree of flexability. */
bool pkgInitialize(Configuration &Cnf)
{
   // General APT things
   Cnf.Set("APT::Architecture","i386");

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

   // Read the main config file
   string FName = Cnf.FindDir("Dir::Etc::main");
   struct stat Buf;   
   if (stat(FName.c_str(),&Buf) != 0)
      return true;
   return ReadConfigFile(Cnf,FName);
}
									/*}}}*/
