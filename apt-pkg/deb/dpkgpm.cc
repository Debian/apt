// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.cc,v 1.1 1998/11/13 04:23:39 jgg Exp $
/* ######################################################################

   DPKG Package Manager - Provide an interface to dpkg
   
   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/dpkgpm.h"
#endif
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
									/*}}}*/

// DPkgPM::pkgDPkgPM - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::pkgDPkgPM(pkgDepCache &Cache) : pkgPackageManager(Cache)
{
}
									/*}}}*/
// DPkgPM::pkgDPkgPM - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::~pkgDPkgPM()
{
}
									/*}}}*/
// DPkgPM::Install - Install a package					/*{{{*/
// ---------------------------------------------------------------------
/* Add an install operation to the sequence list */
bool pkgDPkgPM::Install(PkgIterator Pkg,string File)
{
   if (File.empty() == true || Pkg.end() == true)
      return _error->Error("Internal Error, No file name for %s",Pkg.Name());

   List.push_back(Item(Item::Install,Pkg,File));
   return true;
}
									/*}}}*/
// DPkgPM::Configure - Configure a package				/*{{{*/
// ---------------------------------------------------------------------
/* Add a configure operation to the sequence list */
bool pkgDPkgPM::Configure(PkgIterator Pkg)
{
   if (Pkg.end() == true)
      return false;
   
   List.push_back(Item(Item::Configure,Pkg));
   return true;
}
									/*}}}*/
// DPkgPM::Remove - Remove a package					/*{{{*/
// ---------------------------------------------------------------------
/* Add a remove operation to the sequence list */
bool pkgDPkgPM::Remove(PkgIterator Pkg)
{
   if (Pkg.end() == true)
      return false;
   
   List.push_back(Item(Item::Remove,Pkg));
   return true;
}
									/*}}}*/
// DPkgPM::Go - Run the sequence					/*{{{*/
// ---------------------------------------------------------------------
/* This globs the operations and calls dpkg */
bool pkgDPkgPM::Go()
{
   for (vector<Item>::iterator I = List.begin(); I != List.end();)
   {
      vector<Item>::iterator J = I;
      for (; J != List.end() && J->Op == I->Op; J++);
      
      // Generate the argument list
      const char *Args[400];
      if (J - I > 350)
	 J = I + 350;
      
      int n= 0;
      Args[n++] = "dpkg";
      
      switch (I->Op)
      {
	 case Item::Remove:
	 Args[n++] = "--force-depends";
	 Args[n++] = "--force-remove-essential";
	 Args[n++] = "--remove";
	 break;
	 
	 case Item::Configure:
	 Args[n++] = "--configure";
	 break;
	 
	 case Item::Install:
	 Args[n++] = "--unpack";
	 break;
      }
      
      // Write in the file or package names
      if (I->Op == Item::Install)
	 for (;I != J; I++)
	    Args[n++] = I->File.c_str();
      else
	 for (;I != J; I++)
	    Args[n++] = I->Pkg.Name();
      Args[n] = 0;
      
/*      for (int k = 0; k != n; k++)
	 cout << Args[k] << ' ';
      cout << endl;*/

      cout << flush;
      clog << flush;
      cerr << flush;

      /* Mask off sig int/quit. We do this because dpkg also does when 
         it forks scripts. What happens is that when you hit ctrl-c it sends
	 it to all processes in the group. Since dpkg ignores the signal 
	 it doesn't die but we do! So we must also ignore it */
      signal(SIGQUIT,SIG_IGN);
      signal(SIGINT,SIG_IGN);
	     
      // Fork dpkg
      pid_t Child = fork();
      if (Child < 0)
	 return _error->Errno("fork","Could't fork");
      
      // This is the child
      if (Child == 0)
      {
	 signal(SIGQUIT,SIG_DFL);
	 signal(SIGINT,SIG_DFL);
	 signal(SIGWINCH,SIG_DFL);
	 signal(SIGCONT,SIG_DFL);
	 signal(SIGTSTP,SIG_DFL);

	 if (chdir(_config->FindDir("Dir::Cache::Archives").c_str()) != 0)
	    exit(100);
	 
	 // Close all of our FDs - just in case
	 for (int K = 3; K != 40; K++)
	    fcntl(K,F_SETFD,FD_CLOEXEC);

	 int Flags,dummy;
	 if ((Flags = fcntl(STDIN_FILENO,F_GETFL,dummy)) < 0)
	    exit(100);
	 
	 // Discard everything in stdin before forking dpkg
	 if (fcntl(STDIN_FILENO,F_SETFL,Flags | O_NONBLOCK) < 0)
	    exit(100);
	 
	 while (read(STDIN_FILENO,&dummy,1) == 1);
	 
	 if (fcntl(STDIN_FILENO,F_SETFL,Flags & (~(long)O_NONBLOCK)) < 0)
	    exit(100);

	 /* No Job Control Stop Env is a magic dpkg var that prevents it
	    from using sigstop */
	 setenv("DPKG_NO_TSTP","yes",1);
	 execvp("dpkg",(char **)Args);
	 cerr << "Could not exec dpkg!" << endl;
	 exit(100);
      }      

      // Wait for dpkg
      int Status = 0;
      while (waitpid(Child,&Status,0) != Child)
      {
	 if (errno == EINTR)
	    continue;
	 return _error->Errno("waitpid","Couldn't wait for subprocess");
      }
      
      // Check for an error code.
      if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
	 return _error->Error("Sub-process returned an error code");

      // Restore sig int/quit
      signal(SIGQUIT,SIG_DFL);
      signal(SIGINT,SIG_DFL);
   }
   return true;
}
									/*}}}*/
