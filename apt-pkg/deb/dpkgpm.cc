// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.cc,v 1.26 2003/04/27 03:02:40 doogie Exp $
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
#include <apt-pkg/depcache.h>
#include <apt-pkg/strutl.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
									/*}}}*/

using namespace std;

// DPkgPM::pkgDPkgPM - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::pkgDPkgPM(pkgDepCache *Cache) : pkgPackageManager(Cache)
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
bool pkgDPkgPM::Remove(PkgIterator Pkg,bool Purge)
{
   if (Pkg.end() == true)
      return false;
   
   if (Purge == true)
      List.push_back(Item(Item::Purge,Pkg));
   else
      List.push_back(Item(Item::Remove,Pkg));
   return true;
}
									/*}}}*/
// DPkgPM::RunScripts - Run a set of scripts				/*{{{*/
// ---------------------------------------------------------------------
/* This looks for a list of script sto run from the configuration file,
   each one is run with system from a forked child. */
bool pkgDPkgPM::RunScripts(const char *Cnf)
{
   Configuration::Item const *Opts = _config->Tree(Cnf);
   if (Opts == 0 || Opts->Child == 0)
      return true;
   Opts = Opts->Child;

   // Fork for running the system calls
   pid_t Child = ExecFork();
   
   // This is the child
   if (Child == 0)
   {
      if (chdir("/tmp/") != 0)
	 _exit(100);
	 
      unsigned int Count = 1;
      for (; Opts != 0; Opts = Opts->Next, Count++)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 
	 if (system(Opts->Value.c_str()) != 0)
	    _exit(100+Count);
      }
      _exit(0);
   }      

   // Wait for the child
   int Status = 0;
   while (waitpid(Child,&Status,0) != Child)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }

   // Restore sig int/quit
   signal(SIGQUIT,SIG_DFL);
   signal(SIGINT,SIG_DFL);   

   // Check for an error code.
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
   {
      unsigned int Count = WEXITSTATUS(Status);
      if (Count > 100)
      {
	 Count -= 100;
	 for (; Opts != 0 && Count != 1; Opts = Opts->Next, Count--);
	 _error->Error("Problem executing scripts %s '%s'",Cnf,Opts->Value.c_str());
      }
      
      return _error->Error("Sub-process returned an error code");
   }
   
   return true;
}
                                                                        /*}}}*/
// DPkgPM::SendV2Pkgs - Send version 2 package info			/*{{{*/
// ---------------------------------------------------------------------
/* This is part of the helper script communication interface, it sends
   very complete information down to the other end of the pipe.*/
bool pkgDPkgPM::SendV2Pkgs(FILE *F)
{
   fprintf(F,"VERSION 2\n");
   
   /* Write out all of the configuration directives by walking the 
      configuration tree */
   const Configuration::Item *Top = _config->Tree(0);
   for (; Top != 0;)
   {
      if (Top->Value.empty() == false)
      {
	 fprintf(F,"%s=%s\n",
		 QuoteString(Top->FullTag(),"=\"\n").c_str(),
		 QuoteString(Top->Value,"\n").c_str());
      }

      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }
      
      while (Top != 0 && Top->Next == 0)
	 Top = Top->Parent;
      if (Top != 0)
	 Top = Top->Next;
   }   
   fprintf(F,"\n");
 
   // Write out the package actions in order.
   for (vector<Item>::iterator I = List.begin(); I != List.end(); I++)
   {
      pkgDepCache::StateCache &S = Cache[I->Pkg];
      
      fprintf(F,"%s ",I->Pkg.Name());
      // Current version
      if (I->Pkg->CurrentVer == 0)
	 fprintf(F,"- ");
      else
	 fprintf(F,"%s ",I->Pkg.CurrentVer().VerStr());
      
      // Show the compare operator
      // Target version
      if (S.InstallVer != 0)
      {
	 int Comp = 2;
	 if (I->Pkg->CurrentVer != 0)
	    Comp = S.InstVerIter(Cache).CompareVer(I->Pkg.CurrentVer());
	 if (Comp < 0)
	    fprintf(F,"> ");
	 if (Comp == 0)
	    fprintf(F,"= ");
	 if (Comp > 0)
	    fprintf(F,"< ");
	 fprintf(F,"%s ",S.InstVerIter(Cache).VerStr());
      }
      else
	 fprintf(F,"> - ");
      
      // Show the filename/operation
      if (I->Op == Item::Install)
      {
	 // No errors here..
	 if (I->File[0] != '/')
	    fprintf(F,"**ERROR**\n");
	 else
	    fprintf(F,"%s\n",I->File.c_str());
      }      
      if (I->Op == Item::Configure)
	 fprintf(F,"**CONFIGURE**\n");
      if (I->Op == Item::Remove ||
	  I->Op == Item::Purge)
	 fprintf(F,"**REMOVE**\n");
      
      if (ferror(F) != 0)
	 return false;
   }
   return true;
}
									/*}}}*/
// DPkgPM::RunScriptsWithPkgs - Run scripts with package names on stdin /*{{{*/
// ---------------------------------------------------------------------
/* This looks for a list of scripts to run from the configuration file
   each one is run and is fed on standard input a list of all .deb files
   that are due to be installed. */
bool pkgDPkgPM::RunScriptsWithPkgs(const char *Cnf)
{
   Configuration::Item const *Opts = _config->Tree(Cnf);
   if (Opts == 0 || Opts->Child == 0)
      return true;
   Opts = Opts->Child;
   
   unsigned int Count = 1;
   for (; Opts != 0; Opts = Opts->Next, Count++)
   {
      if (Opts->Value.empty() == true)
         continue;

      // Determine the protocol version
      string OptSec = Opts->Value;
      string::size_type Pos;
      if ((Pos = OptSec.find(' ')) == string::npos || Pos == 0)
	 Pos = OptSec.length();
      OptSec = "DPkg::Tools::Options::" + string(Opts->Value.c_str(),Pos);
      
      unsigned int Version = _config->FindI(OptSec+"::Version",1);
      
      // Create the pipes
      int Pipes[2];
      if (pipe(Pipes) != 0)
	 return _error->Errno("pipe","Failed to create IPC pipe to subprocess");
      SetCloseExec(Pipes[0],true);
      SetCloseExec(Pipes[1],true);
      
      // Purified Fork for running the script
      pid_t Process = ExecFork();      
      if (Process == 0)
      {
	 // Setup the FDs
	 dup2(Pipes[0],STDIN_FILENO);
	 SetCloseExec(STDOUT_FILENO,false);
	 SetCloseExec(STDIN_FILENO,false);      
	 SetCloseExec(STDERR_FILENO,false);

	 const char *Args[4];
	 Args[0] = "/bin/sh";
	 Args[1] = "-c";
	 Args[2] = Opts->Value.c_str();
	 Args[3] = 0;
	 execv(Args[0],(char **)Args);
	 _exit(100);
      }
      close(Pipes[0]);
      FILE *F = fdopen(Pipes[1],"w");
      if (F == 0)
	 return _error->Errno("fdopen","Faild to open new FD");
      
      // Feed it the filenames.
      bool Die = false;
      if (Version <= 1)
      {
	 for (vector<Item>::iterator I = List.begin(); I != List.end(); I++)
	 {
	    // Only deal with packages to be installed from .deb
	    if (I->Op != Item::Install)
	       continue;

	    // No errors here..
	    if (I->File[0] != '/')
	       continue;
	    
	    /* Feed the filename of each package that is pending install
	       into the pipe. */
	    fprintf(F,"%s\n",I->File.c_str());
	    if (ferror(F) != 0)
	    {
	       Die = true;
	       break;
	    }
	 }
      }
      else
	 Die = !SendV2Pkgs(F);

      fclose(F);
      
      // Clean up the sub process
      if (ExecWait(Process,Opts->Value.c_str()) == false)
	 return _error->Error("Failure running script %s",Opts->Value.c_str());
   }

   return true;
}
									/*}}}*/
// DPkgPM::Go - Run the sequence					/*{{{*/
// ---------------------------------------------------------------------
/* This globs the operations and calls dpkg */
bool pkgDPkgPM::Go()
{
   unsigned int MaxArgs = _config->FindI("Dpkg::MaxArgs",350);   
   unsigned int MaxArgBytes = _config->FindI("Dpkg::MaxArgBytes",8192);

   if (RunScripts("DPkg::Pre-Invoke") == false)
      return false;

   if (RunScriptsWithPkgs("DPkg::Pre-Install-Pkgs") == false)
      return false;

   for (vector<Item>::iterator I = List.begin(); I != List.end();)
   {
      vector<Item>::iterator J = I;
      for (; J != List.end() && J->Op == I->Op; J++);

      // Generate the argument list
      const char *Args[MaxArgs + 50];
      if (J - I > (signed)MaxArgs)
	 J = I + MaxArgs;
      
      unsigned int n = 0;
      unsigned long Size = 0;
      string Tmp = _config->Find("Dir::Bin::dpkg","dpkg");
      Args[n++] = Tmp.c_str();
      Size += strlen(Args[n-1]);
      
      // Stick in any custom dpkg options
      Configuration::Item const *Opts = _config->Tree("DPkg::Options");
      if (Opts != 0)
      {
	 Opts = Opts->Child;
	 for (; Opts != 0; Opts = Opts->Next)
	 {
	    if (Opts->Value.empty() == true)
	       continue;
	    Args[n++] = Opts->Value.c_str();
	    Size += Opts->Value.length();
	 }	 
      }
      
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
	 
	 case Item::Purge:
	 Args[n++] = "--force-depends";
	 Size += strlen(Args[n-1]);
	 Args[n++] = "--force-remove-essential";
	 Size += strlen(Args[n-1]);
	 Args[n++] = "--purge";
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
	 for (;I != J && Size < MaxArgBytes; I++)
	 {
	    if (I->File[0] != '/')
	       return _error->Error("Internal Error, Pathname to install is not absolute '%s'",I->File.c_str());
	    Args[n++] = I->File.c_str();
	    Size += strlen(Args[n-1]);
	 }
      }      
      else
      {
	 for (;I != J && Size < MaxArgBytes; I++)
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
      pid_t Child = ExecFork();
            
      // This is the child
      if (Child == 0)
      {
	 if (chdir(_config->FindDir("DPkg::Run-Directory","/").c_str()) != 0)
	    _exit(100);
	 
	 if (_config->FindB("DPkg::FlushSTDIN",true) == true)
	 {
	    int Flags,dummy;
	    if ((Flags = fcntl(STDIN_FILENO,F_GETFL,dummy)) < 0)
	       _exit(100);
	    
	    // Discard everything in stdin before forking dpkg
	    if (fcntl(STDIN_FILENO,F_SETFL,Flags | O_NONBLOCK) < 0)
	       _exit(100);
	    
	    while (read(STDIN_FILENO,&dummy,1) == 1);
	    
	    if (fcntl(STDIN_FILENO,F_SETFL,Flags & (~(long)O_NONBLOCK)) < 0)
	       _exit(100);
	 }
	 
	 /* No Job Control Stop Env is a magic dpkg var that prevents it
	    from using sigstop */
	 putenv("DPKG_NO_TSTP=yes");
	 execvp(Args[0],(char **)Args);
	 cerr << "Could not exec dpkg!" << endl;
	 _exit(100);
      }      

      // Wait for dpkg
      int Status = 0;
      while (waitpid(Child,&Status,0) != Child)
      {
	 if (errno == EINTR)
	    continue;
	 RunScripts("DPkg::Post-Invoke");
	 return _error->Errno("waitpid","Couldn't wait for subprocess");
      }

      // Restore sig int/quit
      signal(SIGQUIT,SIG_DFL);
      signal(SIGINT,SIG_DFL);
       
      // Check for an error code.
      if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      {
	 RunScripts("DPkg::Post-Invoke");
	 if (WIFSIGNALED(Status) != 0 && WTERMSIG(Status) == SIGSEGV)
	    return _error->Error("Sub-process %s received a segmentation fault.",Args[0]);

	 if (WIFEXITED(Status) != 0)
	    return _error->Error("Sub-process %s returned an error code (%u)",Args[0],WEXITSTATUS(Status));
	 
	 return _error->Error("Sub-process %s exited unexpectedly",Args[0]);
      }      
   }

   if (RunScripts("DPkg::Post-Invoke") == false)
      return false;
   return true;
}
									/*}}}*/
// pkgDpkgPM::Reset - Dump the contents of the command list		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDPkgPM::Reset() 
{
   List.erase(List.begin(),List.end());
}
									/*}}}*/
