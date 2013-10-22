#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-progress.h>

#include <apti18n.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <sstream>
#include <fcntl.h>

namespace APT {
namespace Progress {

bool PackageManager::StatusChanged(std::string PackageName, 
                                   unsigned int StepsDone,
                                   unsigned int TotalSteps,
                                   std::string HumanReadableAction)
{
   int reporting_steps = _config->FindI("DpkgPM::Reporting-Steps", 1);
   percentage = StepsDone/(float)TotalSteps * 100.0;
   strprintf(progress_str, _("Progress: [%3i%%]"), (int)percentage);

   if(percentage < (last_reported_progress + reporting_steps))
      return false;

   return true;
}

PackageManagerProgressFd::PackageManagerProgressFd(int progress_fd)
   : StepsDone(0), StepsTotal(1)
{
   OutStatusFd = progress_fd;
}

void PackageManagerProgressFd::WriteToStatusFd(std::string s)
{
   if(OutStatusFd <= 0)
      return;
   FileFd::Write(OutStatusFd, s.c_str(), s.size());   
}

void PackageManagerProgressFd::Start()
{
   if(OutStatusFd <= 0)
      return;

   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   // send status information that we are about to fork dpkg
   std::ostringstream status;
   status << "pmstatus:dpkg-exec:" 
          << (StepsDone/float(StepsTotal)*100.0) 
          << ":" << _("Running dpkg")
          << std::endl;
   WriteToStatusFd(status.str());
}

void PackageManagerProgressFd::Stop()
{
   // clear the Keep-Fd again
   _config->Clear("APT::Keep-Fds", OutStatusFd);
}

void PackageManagerProgressFd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   std::ostringstream status;
   status << "pmerror:" << PackageName
          << ":"  << (StepsDone/float(TotalSteps)*100.0) 
          << ":" << ErrorMessage
          << std::endl;
   WriteToStatusFd(status.str());
}

void PackageManagerProgressFd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   std::ostringstream status;
   status << "pmconffile:" << PackageName
          << ":"  << (StepsDone/float(TotalSteps)*100.0) 
          << ":" << ConfMessage
          << std::endl;
   WriteToStatusFd(status.str());
}


bool PackageManagerProgressFd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string pkg_action)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   // build the status str
   std::ostringstream status;
   status << "pmstatus:" << StringSplit(PackageName, ":")[0]
          << ":"  << (StepsDone/float(StepsTotal)*100.0) 
          << ":" << pkg_action
          << std::endl;
   WriteToStatusFd(status.str());

   if(_config->FindB("Debug::APT::Progress::PackageManagerFd", false) == true)
      std::cerr << "progress: " << PackageName << " " << xStepsDone
                << " " << xTotalSteps << " " << pkg_action
                << std::endl;


   return true;
}


PackageManagerProgressDeb822Fd::PackageManagerProgressDeb822Fd(int progress_fd)
   : StepsDone(0), StepsTotal(1)
{
   OutStatusFd = progress_fd;
}

void PackageManagerProgressDeb822Fd::WriteToStatusFd(std::string s)
{
   FileFd::Write(OutStatusFd, s.c_str(), s.size());   
}

void PackageManagerProgressDeb822Fd::Start()
{
   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   // send status information that we are about to fork dpkg
   std::ostringstream status;
   status << "Status: " << "progress" << std::endl
          << "Percent: " << (StepsDone/float(StepsTotal)*100.0) << std::endl
          << "Message: " << _("Running dpkg") << std::endl
          << std::endl;
   WriteToStatusFd(status.str());
}

void PackageManagerProgressDeb822Fd::Stop()
{
   // clear the Keep-Fd again
   _config->Clear("APT::Keep-Fds", OutStatusFd);
}

void PackageManagerProgressDeb822Fd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   std::ostringstream status;
   status << "Status: " << "Error" << std::endl
          << "Package:" << PackageName << std::endl
          << "Percent: "  << (StepsDone/float(TotalSteps)*100.0) << std::endl
          << "Message: " << ErrorMessage << std::endl
          << std::endl;
   WriteToStatusFd(status.str());
}

void PackageManagerProgressDeb822Fd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   std::ostringstream status;
   status << "Status: " << "ConfFile" << std::endl
          << "Package:" << PackageName << std::endl
          << "Percent: "  << (StepsDone/float(TotalSteps)*100.0) << std::endl
          << "Message: " << ConfMessage << std::endl
          << std::endl;
   WriteToStatusFd(status.str());
}


bool PackageManagerProgressDeb822Fd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string message)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   // build the status str
   std::ostringstream status;
   status << "Status: " << "progress" << std::endl
          << "Package: " << PackageName << std::endl
          << "Percent: "  << (StepsDone/float(StepsTotal)*100.0) << std::endl
          << "Message: " << message << std::endl
          << std::endl;
   WriteToStatusFd(status.str());

   return true;
}


void PackageManagerFancy::SetupTerminalScrollArea(int nr_rows)
{
     // scroll down a bit to avoid visual glitch when the screen
     // area shrinks by one row
     std::cout << "\n";
         
     // save cursor
     std::cout << "\033[s";
         
     // set scroll region (this will place the cursor in the top left)
     std::cout << "\033[1;" << nr_rows - 1 << "r";
            
     // restore cursor but ensure its inside the scrolling area
     std::cout << "\033[u";
     static const char *move_cursor_up = "\033[1A";
     std::cout << move_cursor_up;

     std::flush(std::cout);
}

PackageManagerFancy::PackageManagerFancy()
   : nr_terminal_rows(-1)
{
   struct winsize win;
   if(ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&win) == 0)
   {
      nr_terminal_rows = win.ws_row;
   }
}

void PackageManagerFancy::Start()
{
   if (nr_terminal_rows > 0)
      SetupTerminalScrollArea(nr_terminal_rows);
}

void PackageManagerFancy::Stop()
{
   if (nr_terminal_rows > 0)
   {
      SetupTerminalScrollArea(nr_terminal_rows + 1);

      // override the progress line (sledgehammer)
      static const char* clear_screen_below_cursor = "\033[J";
      std::cout << clear_screen_below_cursor;
   }
}

bool PackageManagerFancy::StatusChanged(std::string PackageName, 
                                        unsigned int StepsDone,
                                        unsigned int TotalSteps,
                                        std::string HumanReadableAction)
{
   if (!PackageManager::StatusChanged(PackageName, StepsDone, TotalSteps,
          HumanReadableAction))
      return false;

   int row = nr_terminal_rows;

   static string save_cursor = "\033[s";
   static string restore_cursor = "\033[u";
   
   static string set_bg_color = "\033[42m"; // green
   static string set_fg_color = "\033[30m"; // black
   
   static string restore_bg =  "\033[49m";
   static string restore_fg = "\033[39m";
   
   std::cout << save_cursor
      // move cursor position to last row
             << "\033[" << row << ";0f" 
             << set_bg_color
             << set_fg_color
             << progress_str
             << restore_cursor
             << restore_bg
             << restore_fg;
   std::flush(std::cout);
   last_reported_progress = percentage;

   return true;
}

bool PackageManagerText::StatusChanged(std::string PackageName, 
                                       unsigned int StepsDone,
                                       unsigned int TotalSteps,
                                       std::string HumanReadableAction)
{
   if (!PackageManager::StatusChanged(PackageName, StepsDone, TotalSteps, HumanReadableAction))
      return false;

   std::cout << progress_str << "\r\n";
   std::flush(std::cout);
                   
   last_reported_progress = percentage;

   return true;
}



}; // namespace progress
}; // namespace apt
