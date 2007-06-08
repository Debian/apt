// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: clean.cc,v 1.4 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   Clean - Clean out downloaded directories
   
   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#include <apt-pkg/clean.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>

#include <apti18n.h>    

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

// ArchiveCleaner::Go - Perform smart cleanup of the archive		/*{{{*/
// ---------------------------------------------------------------------
/* Scan the directory for files to erase, we check the version information
   against our database to see if it is interesting */
bool pkgArchiveCleaner::Go(string Dir,pkgCache &Cache)
{
   bool CleanInstalled = _config->FindB("APT::Clean-Installed",true);
   string MyArch = _config->Find("APT::Architecture");
      
   DIR *D = opendir(Dir.c_str());
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());

   string StartDir = SafeGetCWD();
   if (chdir(Dir.c_str()) != 0)
   {
      closedir(D);
      return _error->Errno("chdir",_("Unable to change to %s"),Dir.c_str());
   }
   
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,"lock") == 0 ||
	  strcmp(Dir->d_name,"partial") == 0 ||
	  strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;

      struct stat St;
      if (stat(Dir->d_name,&St) != 0)
      {
	 chdir(StartDir.c_str());
	 closedir(D);
	 return _error->Errno("stat",_("Unable to stat %s."),Dir->d_name);
      }
      
      // Grab the package name
      const char *I = Dir->d_name;
      for (; *I != 0 && *I != '_';I++);
      if (*I != '_')
	 continue;
      string Pkg = DeQuoteString(string(Dir->d_name,I-Dir->d_name));

      // Grab the version
      const char *Start = I + 1;
      for (I = Start; *I != 0 && *I != '_';I++);
      if (*I != '_')
	 continue;
      string Ver = DeQuoteString(string(Start,I-Start));
  
      // Grab the arch
      Start = I + 1;
      for (I = Start; *I != 0 && *I != '.' ;I++);
      if (*I != '.')
	 continue;
      string Arch = DeQuoteString(string(Start,I-Start));
      
      if (Arch != "all" && Arch != MyArch)
	 continue;
      
      // Lookup the package
      pkgCache::PkgIterator P = Cache.FindPkg(Pkg);
      if (P.end() != true)
      {
	 pkgCache::VerIterator V = P.VersionList();
	 for (; V.end() == false; V++)
	 {
	    // See if we can fetch this version at all
	    bool IsFetchable = false;
	    for (pkgCache::VerFileIterator J = V.FileList(); 
		 J.end() == false; J++)
	    {
	       if (CleanInstalled == true &&
		   (J.File()->Flags & pkgCache::Flag::NotSource) != 0)
		  continue;
	       IsFetchable = true;
	       break;
	    }
	    
	    // See if this verison matches the file
	    if (IsFetchable == true && Ver == V.VerStr())
	       break;
	 }
	 
	 // We found a match, keep the file
	 if (V.end() == false)
	    continue;
      }
            
      Erase(Dir->d_name,Pkg,Ver,St);
   };
   
   chdir(StartDir.c_str());
   closedir(D);
   return true;   
}
									/*}}}*/
