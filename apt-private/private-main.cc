#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-main.h>

#include <iostream>
#include <locale>

#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <apti18n.h>


void InitLocale(APT_CMD const binary)				/*{{{*/
{
   try {
      std::locale::global(std::locale(""));
   } catch (...) {
      setlocale(LC_ALL, "");
   }
   switch(binary)
   {
      case APT_CMD::APT:
      case APT_CMD::APT_CACHE:
      case APT_CMD::APT_CDROM:
      case APT_CMD::APT_CONFIG:
      case APT_CMD::APT_DUMP_SOLVER:
      case APT_CMD::APT_HELPER:
      case APT_CMD::APT_GET:
      case APT_CMD::APT_MARK:
	 textdomain("apt");
	 break;
      case APT_CMD::APT_EXTRACTTEMPLATES:
      case APT_CMD::APT_FTPARCHIVE:
      case APT_CMD::APT_INTERNAL_PLANNER:
      case APT_CMD::APT_INTERNAL_SOLVER:
      case APT_CMD::APT_SORTPKG:
	 textdomain("apt-utils");
	 break;
   }
}
void InitLocale() {}
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
	 // TRANSLATORS: placeholder is a binary name like apt or apt-get
	 ioprintf(std::cout, _("NOTE: This is only a simulation!\n"
	    "      %s needs root privileges for real execution.\n"
	    "      Keep also in mind that locking is deactivated,\n"
	    "      so don't depend on the relevance to the real current situation!\n"),
	    _config->Find("Binary").c_str());
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
