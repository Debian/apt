#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>

#include <apt-private/private-main.h>

#include <iostream>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <apti18n.h>


void InitSignals()
{
   // Setup the signals
   signal(SIGPIPE,SIG_IGN);
}


void CheckSimulateMode(CommandLine &CmdL)
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
