// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.cc,v 1.3 1998/11/23 07:03:11 jgg Exp $
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
      
      unsigned int n = 0;
      unsigned long Size = 0;
      Args[n++] = _config->Find("Dir::Bin::dpkg","dpkg").c_str();
      Size += strlen(Args[n-1]);
      
      switch (I->Op)
      {
	 case Item::Remove:
	 Args[n++] = "--force-depends";
	 Size += strlen(Args[n-1]);
	 Args[n++] = "--force-remove-essential";
	 Size += strlen(Args[n-1]);
	 Args[n++] = "--remove";
	 Size += strlen(Args[n-1]);
	 break;
	 
	 case Item::Configure:
	 Args[n++] = "--configure";
	 Size += strlen(Args[n-1]);
	 break;
	 
	 case Item::Install:
	 Args[n++] = "--unpack";
	 Size += strlen(Args[n-1]);
	 break;
      }
      
      // Write in the file or package names
      if (I->Op == Item::Install)
      {
	 for (;I != J && Size < 1024; I++)
	 {
	    Args[n++] = I->File.c_str();
	    Size += strlen(Args[n-1]);
	 }
      }      
      else
      {
	 for (;I != J && Size < 1024; I++)
	 {
	    Args[n++] = I->Pkg.Name();
	    Size += strlen(Args[n-1]);
	 }	 
      }      
      Args[n] = 0;
      J = I;
      
      if (_config->FindB("Debug::pkgDPkgPM",false) == true)
      {
	 for (unsigned int k = 0; k != n; k++)
	    clog << Args[k] << ' ';
	 clog << endl;
	 continue;
      }
      
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
	 signal(SIGPIPE,SIG_DFL);
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
