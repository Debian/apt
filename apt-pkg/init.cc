// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.cc,v 1.3 1998/07/16 06:08:36 jgg Exp $
/* ######################################################################

   Init - Initialize the package library
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include <apt-pkg/init.h>
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
   Cnf.Set("Dir::State::xstatus","xstatus");
   Cnf.Set("Dir::State::userstatus","status.user");
   
   // Cache
   Cnf.Set("Dir::Cache","/etc/apt/");
   Cnf.Set("Dir::Cache::archives","archives/");
   Cnf.Set("Dir::Cache::srcpkgcache","srcpkgcache");
   Cnf.Set("Dir::Cache::pkhcache","pkgcache");
   
   // Configuration
   Cnf.Set("Dir::Etc","/etc/apt/");
   Cnf.Set("Dir::Etc::sourcelist","sources.list");
   Cnf.Set("Dir::Etc::main","apt.conf");
   
   return true;
}
									/*}}}*/
