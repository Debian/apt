// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.cc,v 1.4 1998/07/26 04:49:26 jgg Exp $
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
   
   /* These really should be jammed into a generic 'Local Database' engine
      which is yet to be determined. The functions in pkgcachegen should
      be the only users of these */
   Cnf.Set("Dir::State::xstatus","xstatus");
   Cnf.Set("Dir::State::userstatus","status.user");   
   Cnf.Set("Dir::State::status","/var/lib/dpkg/status");
   
   // Cache
   Cnf.Set("Dir::Cache","/tmp/");
   Cnf.Set("Dir::Cache::archives","archives/");
   Cnf.Set("Dir::Cache::srcpkgcache","srcpkgcache");
   Cnf.Set("Dir::Cache::pkgcache","pkgcache");
   
   // Configuration
   Cnf.Set("Dir::Etc","/etc/apt/");
   Cnf.Set("Dir::Etc::sourcelist","sources.list");
   Cnf.Set("Dir::Etc::main","apt.conf");
   
   return true;
}
									/*}}}*/
