#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <apt-private/private-main.h>

#include <iostream>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <apti18n.h>


void InitLocale()							/*{{{*/
{
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);
}
									/*}}}*/
void InitSignals()							/*{{{*/
{
   signal(SIGPIPE,SIG_IGN);
}
									/*}}}*/
void CheckIfSimulateMode(CommandLine &CmdL)				/*{{{*/
{
   // disable locking in simulation, but show the message only for users
   // as root hasn't the same problems like unreadable files which can heavily
   // distort the simulation.
   if (_config->FindB("APT::Get::Simulate") == true &&
	(CmdL.FileSize() == 0 ||
	 (strcmp(CmdL.FileList[0], "source") != 0 && strcmp(CmdL.FileList[0], "download") != 0 &&
	  strcmp(CmdL.FileList[0], "changelog") != 0)))
   {
      if (getuid() != 0 && _config->FindB("APT::Get::Show-User-Simulation-Note",true) == true)
         std::cout << _("NOTE: This is only a simulation!\n"
	    "      apt-get needs root privileges for real execution.\n"
	    "      Keep also in mind that locking is deactivated,\n"
	    "      so don't depend on the relevance to the real current situation!"
	 ) << std::endl;
      _config->Set("Debug::NoLocking",true);
   }
}
									/*}}}*/
void CheckIfCalledByScript(int argc, const char *argv[])		/*{{{*/
{
   if (unlikely(argc < 1)) return;

   if(!isatty(STDOUT_FILENO) &&
      _config->FindB("Apt::Cmd::Disable-Script-Warning", false) == false)
   {
      std::cerr << std::endl
                << "WARNING: " << flNotDir(argv[0]) << " "
                << "does not have a stable CLI interface. "
                << "Use with caution in scripts."
                << std::endl
                << std::endl;
   }
}
									/*}}}*/
