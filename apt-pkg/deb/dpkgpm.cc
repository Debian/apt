// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.cc,v 1.28 2004/01/27 02:25:01 mdz Exp $
/* ######################################################################

   DPKG Package Manager - Provide an interface to dpkg
   
   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/strutl.h>
#include <apti18n.h>
#include <apt-pkg/fileutl.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <sstream>
#include <map>

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pty.h>

#include <config.h>
#include <apti18n.h>
									/*}}}*/

using namespace std;

namespace
{
  // Maps the dpkg "processing" info to human readable names.  Entry 0
  // of each array is the key, entry 1 is the value.
  const std::pair<const char *, const char *> PackageProcessingOps[] = {
    std::make_pair("install",   N_("Installing %s")),
    std::make_pair("configure", N_("Configuring %s")),
    std::make_pair("remove",    N_("Removing %s")),
    std::make_pair("trigproc",  N_("Running post-installation trigger %s"))
  };

  const std::pair<const char *, const char *> * const PackageProcessingOpsBegin = PackageProcessingOps;
  const std::pair<const char *, const char *> * const PackageProcessingOpsEnd   = PackageProcessingOps + sizeof(PackageProcessingOps) / sizeof(PackageProcessingOps[0]);

  // Predicate to test whether an entry in the PackageProcessingOps
  // array matches a string.
  class MatchProcessingOp
  {
    const char *target;

  public:
    MatchProcessingOp(const char *the_target)
      : target(the_target)
    {
    }

    bool operator()(const std::pair<const char *, const char *> &pair) const
    {
      return strcmp(pair.first, target) == 0;
    }
  };
}

// DPkgPM::pkgDPkgPM - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::pkgDPkgPM(pkgDepCache *Cache) 
   : pkgPackageManager(Cache), dpkgbuf_pos(0),
     term_out(NULL), PackagesDone(0), PackagesTotal(0)
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
// DPkgPM::DoStdin - Read stdin and pass to slave pty			/*{{{*/
// ---------------------------------------------------------------------
/*
*/
void pkgDPkgPM::DoStdin(int master)
{
   unsigned char input_buf[256] = {0,}; 
   ssize_t len = read(0, input_buf, sizeof(input_buf));
   if (len)
      write(master, input_buf, len);
   else
      stdin_is_dev_null = true;
}
									/*}}}*/
// DPkgPM::DoTerminalPty - Read the terminal pty and write log		/*{{{*/
// ---------------------------------------------------------------------
/*
 * read the terminal pty and write log
 */
void pkgDPkgPM::DoTerminalPty(int master)
{
   unsigned char term_buf[1024] = {0,0, };

   ssize_t len=read(master, term_buf, sizeof(term_buf));
   if(len == -1 && errno == EIO)
   {
      // this happens when the child is about to exit, we
      // give it time to actually exit, otherwise we run
      // into a race
      usleep(500000);
      return;
   }  
   if(len <= 0) 
      return;
   write(1, term_buf, len);
   if(term_out)
      fwrite(term_buf, len, sizeof(char), term_out);
}
									/*}}}*/
// DPkgPM::ProcessDpkgStatusBuf                                        	/*{{{*/
// ---------------------------------------------------------------------
/*
 */
void pkgDPkgPM::ProcessDpkgStatusLine(int OutStatusFd, char *line)
{
   // the status we output
   ostringstream status;

   if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
      std::clog << "got from dpkg '" << line << "'" << std::endl;


   /* dpkg sends strings like this:
      'status:   <pkg>:  <pkg  qstate>'
      errors look like this:
      'status: /var/cache/apt/archives/krecipes_0.8.1-0ubuntu1_i386.deb : error : trying to overwrite `/usr/share/doc/kde/HTML/en/krecipes/krectip.png', which is also in package krecipes-data 
      and conffile-prompt like this
      'status: conffile-prompt: conffile : 'current-conffile' 'new-conffile' useredited distedited
      
      Newer versions of dpkg sent also:
      'processing: install: pkg'
      'processing: configure: pkg'
      'processing: remove: pkg'
      'processing: trigproc: trigger'
	    
   */
   char* list[5];
   //        dpkg sends multiline error messages sometimes (see
   //        #374195 for a example. we should support this by
   //        either patching dpkg to not send multiline over the
   //        statusfd or by rewriting the code here to deal with
   //        it. for now we just ignore it and not crash
   TokSplitString(':', line, list, sizeof(list)/sizeof(list[0]));
   if( list[0] == NULL || list[1] == NULL || list[2] == NULL) 
   {
      if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	 std::clog << "ignoring line: not enough ':'" << std::endl;
      return;
   }
   char *pkg = list[1];
   char *action = _strstrip(list[2]);

   // 'processing' from dpkg looks like
   // 'processing: action: pkg'
   if(strncmp(list[0], "processing", strlen("processing")) == 0)
   {
      char s[200];
      char *pkg_or_trigger = _strstrip(list[2]);
      action =_strstrip( list[1]);
      const std::pair<const char *, const char *> * const iter =
	std::find_if(PackageProcessingOpsBegin,
		     PackageProcessingOpsEnd,
		     MatchProcessingOp(action));
      if(iter == PackageProcessingOpsEnd)
      {
	 if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	    std::clog << "ignoring unknwon action: " << action << std::endl;
	 return;
      }
      snprintf(s, sizeof(s), _(iter->second), pkg_or_trigger);

      status << "pmstatus:" << pkg_or_trigger
	     << ":"  << (PackagesDone/float(PackagesTotal)*100.0) 
	     << ":" << s
	     << endl;
      if(OutStatusFd > 0)
	 write(OutStatusFd, status.str().c_str(), status.str().size());
      if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	 std::clog << "send: '" << status.str() << "'" << endl;
      return;
   }

   if(strncmp(action,"error",strlen("error")) == 0)
   {
      status << "pmerror:" << list[1]
	     << ":"  << (PackagesDone/float(PackagesTotal)*100.0) 
	     << ":" << list[3]
	     << endl;
      if(OutStatusFd > 0)
	 write(OutStatusFd, status.str().c_str(), status.str().size());
      if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	 std::clog << "send: '" << status.str() << "'" << endl;
      return;
   }
   if(strncmp(action,"conffile",strlen("conffile")) == 0)
   {
      status << "pmconffile:" << list[1]
	     << ":"  << (PackagesDone/float(PackagesTotal)*100.0) 
	     << ":" << list[3]
	     << endl;
      if(OutStatusFd > 0)
	 write(OutStatusFd, status.str().c_str(), status.str().size());
      if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	 std::clog << "send: '" << status.str() << "'" << endl;
      return;
   }

   vector<struct DpkgState> &states = PackageOps[pkg];
   const char *next_action = NULL;
   if(PackageOpsDone[pkg] < states.size())
      next_action = states[PackageOpsDone[pkg]].state;
   // check if the package moved to the next dpkg state
   if(next_action && (strcmp(action, next_action) == 0)) 
   {
      // only read the translation if there is actually a next
      // action
      const char *translation = _(states[PackageOpsDone[pkg]].str);
      char s[200];
      snprintf(s, sizeof(s), translation, pkg);

      // we moved from one dpkg state to a new one, report that
      PackageOpsDone[pkg]++;
      PackagesDone++;
      // build the status str
      status << "pmstatus:" << pkg 
	     << ":"  << (PackagesDone/float(PackagesTotal)*100.0) 
	     << ":" << s
	     << endl;
      if(OutStatusFd > 0)
	 write(OutStatusFd, status.str().c_str(), status.str().size());
      if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true)
	 std::clog << "send: '" << status.str() << "'" << endl;
   }
   if (_config->FindB("Debug::pkgDPkgProgressReporting",false) == true) 
      std::clog << "(parsed from dpkg) pkg: " << pkg 
		<< " action: " << action << endl;
}

// DPkgPM::DoDpkgStatusFd                                           	/*{{{*/
// ---------------------------------------------------------------------
/*
 */
void pkgDPkgPM::DoDpkgStatusFd(int statusfd, int OutStatusFd)
{
   char *p, *q;
   int len;

   len=read(statusfd, &dpkgbuf[dpkgbuf_pos], sizeof(dpkgbuf)-dpkgbuf_pos);
   dpkgbuf_pos += len;
   if(len <= 0)
      return;

   // process line by line if we have a buffer
   p = q = dpkgbuf;
   while((q=(char*)memchr(p, '\n', dpkgbuf+dpkgbuf_pos-p)) != NULL)
   {
      *q = 0;
      ProcessDpkgStatusLine(OutStatusFd, p);
      p=q+1; // continue with next line
   }

   // now move the unprocessed bits (after the final \n that is now a 0x0) 
   // to the start and update dpkgbuf_pos
   p = (char*)memrchr(dpkgbuf, 0, dpkgbuf_pos);
   if(p == NULL)
      return;

   // we are interessted in the first char *after* 0x0
   p++;

   // move the unprocessed tail to the start and update pos
   memmove(dpkgbuf, p, p-dpkgbuf);
   dpkgbuf_pos = dpkgbuf+dpkgbuf_pos-p;
}
									/*}}}*/

bool pkgDPkgPM::OpenLog()
{
   string logdir = _config->FindDir("Dir::Log");
   if(not FileExists(logdir))
      return _error->Error(_("Directory '%s' missing"), logdir.c_str());
   string logfile_name = flCombine(logdir,
				   _config->Find("Dir::Log::Terminal"));
   if (!logfile_name.empty())
   {
      term_out = fopen(logfile_name.c_str(),"a");
      chmod(logfile_name.c_str(), 0600);
      // output current time
      char outstr[200];
      time_t t = time(NULL);
      struct tm *tmp = localtime(&t);
      strftime(outstr, sizeof(outstr), "%F  %T", tmp);
      fprintf(term_out, "\nLog started: ");
      fprintf(term_out, "%s", outstr);
      fprintf(term_out, "\n");
   }
   return true;
}

bool pkgDPkgPM::CloseLog()
{
   if(term_out)
   {
      char outstr[200];
      time_t t = time(NULL);
      struct tm *tmp = localtime(&t);
      strftime(outstr, sizeof(outstr), "%F  %T", tmp);
      fprintf(term_out, "Log ended: ");
      fprintf(term_out, "%s", outstr);
      fprintf(term_out, "\n");
      fclose(term_out);
   }
   term_out = NULL;
   return true;
}

/*{{{*/
// This implements a racy version of pselect for those architectures
// that don't have a working implementation.
// FIXME: Probably can be removed on Lenny+1
static int racy_pselect(int nfds, fd_set *readfds, fd_set *writefds,
   fd_set *exceptfds, const struct timespec *timeout,
   const sigset_t *sigmask)
{
   sigset_t origmask;
   struct timeval tv;
   int retval;

   tv.tv_sec = timeout->tv_sec;
   tv.tv_usec = timeout->tv_nsec/1000;

   sigprocmask(SIG_SETMASK, sigmask, &origmask);
   retval = select(nfds, readfds, writefds, exceptfds, &tv);
   sigprocmask(SIG_SETMASK, &origmask, 0);
   return retval;
}
/*}}}*/

// DPkgPM::Go - Run the sequence					/*{{{*/
// ---------------------------------------------------------------------
/* This globs the operations and calls dpkg 
 *   
 * If it is called with "OutStatusFd" set to a valid file descriptor
 * apt will report the install progress over this fd. It maps the
 * dpkg states a package goes through to human readable (and i10n-able)
 * names and calculates a percentage for each step.
*/
bool pkgDPkgPM::Go(int OutStatusFd)
{
   unsigned int MaxArgs = _config->FindI("Dpkg::MaxArgs",8*1024);   
   unsigned int MaxArgBytes = _config->FindI("Dpkg::MaxArgBytes",32*1024);
   bool NoTriggers = _config->FindB("DPkg::NoTriggers",false);

   if (RunScripts("DPkg::Pre-Invoke") == false)
      return false;

   if (RunScriptsWithPkgs("DPkg::Pre-Install-Pkgs") == false)
      return false;

   // map the dpkg states to the operations that are performed
   // (this is sorted in the same way as Item::Ops)
   static const struct DpkgState DpkgStatesOpMap[][7] = {
      // Install operation
      { 
	 {"half-installed", N_("Preparing %s")}, 
	 {"unpacked", N_("Unpacking %s") }, 
	 {NULL, NULL}
      },
      // Configure operation
      { 
	 {"unpacked",N_("Preparing to configure %s") },
	 {"half-configured", N_("Configuring %s") },
#if 0
	 {"triggers-awaited", N_("Processing triggers for %s") },
	 {"triggers-pending", N_("Processing triggers for %s") },
#endif
	 { "installed", N_("Installed %s")},
	 {NULL, NULL}
      },
      // Remove operation
      { 
	 {"half-configured", N_("Preparing for removal of %s")},
#if 0
	 {"triggers-awaited", N_("Preparing for removal of %s")},
	 {"triggers-pending", N_("Preparing for removal of %s")},
#endif
	 {"half-installed", N_("Removing %s")},
	 {"config-files",  N_("Removed %s")},
	 {NULL, NULL}
      },
      // Purge operation
      { 
	 {"config-files", N_("Preparing to completely remove %s")},
	 {"not-installed", N_("Completely removed %s")},
	 {NULL, NULL}
      },
   };

   // init the PackageOps map, go over the list of packages that
   // that will be [installed|configured|removed|purged] and add
   // them to the PackageOps map (the dpkg states it goes through)
   // and the PackageOpsTranslations (human readable strings)
   for (vector<Item>::iterator I = List.begin(); I != List.end();I++)
   {
      string name = (*I).Pkg.Name();
      PackageOpsDone[name] = 0;
      for(int i=0; (DpkgStatesOpMap[(*I).Op][i]).state != NULL;  i++) 
      {
	 PackageOps[name].push_back(DpkgStatesOpMap[(*I).Op][i]);
	 PackagesTotal++;
      }
   }   

   stdin_is_dev_null = false;

   // create log
   OpenLog();

   // this loop is runs once per operation
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
      
      char status_fd_buf[20];
      int fd[2];
      pipe(fd);
      
      Args[n++] = "--status-fd";
      Size += strlen(Args[n-1]);
      snprintf(status_fd_buf,sizeof(status_fd_buf),"%i", fd[1]);
      Args[n++] = status_fd_buf;
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
	 if (NoTriggers)
	    Args[n++] = "--no-triggers";
	 Size += strlen(Args[n-1]);
	 break;
	 
	 case Item::Install:
	 Args[n++] = "--unpack";
	 Size += strlen(Args[n-1]);
	 Args[n++] = "--auto-deconfigure";
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
      sighandler_t old_SIGQUIT = signal(SIGQUIT,SIG_IGN);
      sighandler_t old_SIGINT = signal(SIGINT,SIG_IGN);

      // ignore SIGHUP as well (debian #463030)
      sighandler_t old_SIGHUP = signal(SIGHUP,SIG_IGN);

      struct	termios tt;
      struct	termios tt_out;
      struct	winsize win;
      int	master;
      int	slave;

      // FIXME: setup sensible signal handling (*ick*)
      tcgetattr(0, &tt);
      tcgetattr(1, &tt_out);
      ioctl(0, TIOCGWINSZ, (char *)&win);
      if (openpty(&master, &slave, NULL, &tt_out, &win) < 0) 
      {
	 const char *s = _("Can not write log, openpty() "
			   "failed (/dev/pts not mounted?)\n");
	 fprintf(stderr, "%s",s);
	 fprintf(term_out, "%s",s);
	 master = slave = -1;
      }  else {
	 struct termios rtt;
	 rtt = tt;
	 cfmakeraw(&rtt);
	 rtt.c_lflag &= ~ECHO;
	 tcsetattr(0, TCSAFLUSH, &rtt);
      }

       // Fork dpkg
      pid_t Child;
      _config->Set("APT::Keep-Fds::",fd[1]);
      Child = ExecFork();
            
      // This is the child
      if (Child == 0)
      {
	 if(slave >= 0 && master >= 0) 
	 {
	    setsid();
	    ioctl(slave, TIOCSCTTY, 0);
	    close(master);
	    dup2(slave, 0);
	    dup2(slave, 1);
	    dup2(slave, 2);
	    close(slave);
	 }
	 close(fd[0]); // close the read end of the pipe

	 if (chdir(_config->FindDir("DPkg::Run-Directory","/").c_str()) != 0)
	    _exit(100);
	 
	 if (_config->FindB("DPkg::FlushSTDIN",true) == true && isatty(STDIN_FILENO))
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
	 putenv((char *)"DPKG_NO_TSTP=yes");
	 execvp(Args[0],(char **)Args);
	 cerr << "Could not exec dpkg!" << endl;
	 _exit(100);
      }      

      // clear the Keep-Fd again
      _config->Clear("APT::Keep-Fds",fd[1]);

      // Wait for dpkg
      int Status = 0;

      // we read from dpkg here
      int _dpkgin = fd[0];
      close(fd[1]);                        // close the write end of the pipe

      // the result of the waitpid call
      int res;
      if(slave > 0)
	 close(slave);

      // setups fds
      fd_set rfds;
      struct timespec tv;
      sigset_t sigmask;
      sigset_t original_sigmask;
      sigemptyset(&sigmask);
      sigprocmask(SIG_BLOCK,&sigmask,&original_sigmask);

      int select_ret;
      while ((res=waitpid(Child,&Status, WNOHANG)) != Child) {
	 if(res < 0) {
	    // FIXME: move this to a function or something, looks ugly here
	    // error handling, waitpid returned -1
	    if (errno == EINTR)
	       continue;
	    RunScripts("DPkg::Post-Invoke");

	    // Restore sig int/quit
	    signal(SIGQUIT,old_SIGQUIT);
	    signal(SIGINT,old_SIGINT);
	    signal(SIGHUP,old_SIGHUP);
	    return _error->Errno("waitpid","Couldn't wait for subprocess");
	 }

	 // wait for input or output here
	 FD_ZERO(&rfds);
	 if (!stdin_is_dev_null)
	    FD_SET(0, &rfds); 
	 FD_SET(_dpkgin, &rfds);
	 if(master >= 0)
	    FD_SET(master, &rfds);
	 tv.tv_sec = 1;
	 tv.tv_nsec = 0;
	 select_ret = pselect(max(master, _dpkgin)+1, &rfds, NULL, NULL, 
			      &tv, &original_sigmask);
	 if (select_ret < 0 && (errno == EINVAL || errno == ENOSYS))
	    select_ret = racy_pselect(max(master, _dpkgin)+1, &rfds, NULL,
				      NULL, &tv, &original_sigmask);
	 if (select_ret == 0) 
  	    continue;
  	 else if (select_ret < 0 && errno == EINTR)
  	    continue;
  	 else if (select_ret < 0) 
 	 {
  	    perror("select() returned error");
  	    continue;
  	 } 
	 
	 if(master >= 0 && FD_ISSET(master, &rfds))
	    DoTerminalPty(master);
	 if(master >= 0 && FD_ISSET(0, &rfds))
	    DoStdin(master);
	 if(FD_ISSET(_dpkgin, &rfds))
	    DoDpkgStatusFd(_dpkgin, OutStatusFd);
      }
      close(_dpkgin);

      // Restore sig int/quit
      signal(SIGQUIT,old_SIGQUIT);
      signal(SIGINT,old_SIGINT);
      signal(SIGHUP,old_SIGHUP);

      if(master >= 0) 
      {
	 tcsetattr(0, TCSAFLUSH, &tt);
	 close(master);
      }
       
      // Check for an error code.
      if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      {
	 // if it was set to "keep-dpkg-runing" then we won't return
	 // here but keep the loop going and just report it as a error
	 // for later
	 bool stopOnError = _config->FindB("Dpkg::StopOnError",true);
	 
	 if(stopOnError)
	    RunScripts("DPkg::Post-Invoke");

	 if (WIFSIGNALED(Status) != 0 && WTERMSIG(Status) == SIGSEGV) 
	    _error->Error("Sub-process %s received a segmentation fault.",Args[0]);
	 else if (WIFEXITED(Status) != 0)
	    _error->Error("Sub-process %s returned an error code (%u)",Args[0],WEXITSTATUS(Status));
	 else 
	    _error->Error("Sub-process %s exited unexpectedly",Args[0]);

	 if(stopOnError) 
	 {
	    CloseLog();
	    return false;
	 }
      }      
   }
   CloseLog();

   if (RunScripts("DPkg::Post-Invoke") == false)
      return false;

   Cache.writeStateFile(NULL);
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
