#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/install-progress.h>

#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <algorithm>
#include <stdio.h>
#include <sstream>

#include <apti18n.h>

namespace APT {
namespace Progress {

PackageManager::PackageManager() : d(NULL), percentage(0.0), last_reported_progress(-1) {}
PackageManager::~PackageManager() {}

/* Return a APT::Progress::PackageManager based on the global
 * apt configuration (i.e. APT::Status-Fd and APT::Status-deb822-Fd)
 */
PackageManager* PackageManagerProgressFactory()
{
   // select the right progress
   int status_fd = _config->FindI("APT::Status-Fd", -1);
   int status_deb822_fd = _config->FindI("APT::Status-deb822-Fd", -1);

   APT::Progress::PackageManager *progress = NULL;
   if (status_deb822_fd > 0)
      progress = new APT::Progress::PackageManagerProgressDeb822Fd(
         status_deb822_fd);
   else if (status_fd > 0)
      progress = new APT::Progress::PackageManagerProgressFd(status_fd);
   else if(_config->FindB("Dpkg::Progress-Fancy", false) == true)
      progress = new APT::Progress::PackageManagerFancy();
   else if (_config->FindB("Dpkg::Progress", 
                           _config->FindB("DpkgPM::Progress", false)) == true)
      progress = new APT::Progress::PackageManagerText();
   else
      progress = new APT::Progress::PackageManager();
   return progress;
}

bool PackageManager::StatusChanged(std::string /*PackageName*/,
                                   unsigned int StepsDone,
                                   unsigned int TotalSteps,
                                   std::string /*HumanReadableAction*/)
{
   int reporting_steps = _config->FindI("DpkgPM::Reporting-Steps", 1);
   percentage = StepsDone/(float)TotalSteps * 100.0;
   strprintf(progress_str, _("Progress: [%3i%%]"), (int)percentage);

   if(percentage < (last_reported_progress + reporting_steps))
      return false;

   return true;
}

PackageManagerProgressFd::PackageManagerProgressFd(int progress_fd)
   : d(NULL), StepsDone(0), StepsTotal(1)
{
   OutStatusFd = progress_fd;
}
PackageManagerProgressFd::~PackageManagerProgressFd() {}

void PackageManagerProgressFd::WriteToStatusFd(std::string s)
{
   if(OutStatusFd <= 0)
      return;
   FileFd::Write(OutStatusFd, s.c_str(), s.size());   
}

void PackageManagerProgressFd::StartDpkg()
{
   if(OutStatusFd <= 0)
      return;

   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   // send status information that we are about to fork dpkg
   std::string status;
   strprintf(status, "pmstatus:dpkg-exec:%.4f:%s\n",
	 (StepsDone/float(StepsTotal)*100.0), _("Running dpkg"));
   WriteToStatusFd(std::move(status));
}

APT_CONST void PackageManagerProgressFd::Stop()
{
}

void PackageManagerProgressFd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   std::string status;
   strprintf(status, "pmerror:%s:%.4f:%s\n", PackageName.c_str(),
	 (StepsDone/float(TotalSteps)*100.0), ErrorMessage.c_str());
   WriteToStatusFd(std::move(status));
}

void PackageManagerProgressFd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   std::string status;
   strprintf(status, "pmconffile:%s:%.4f:%s\n", PackageName.c_str(),
	 (StepsDone/float(TotalSteps)*100.0), ConfMessage.c_str());
   WriteToStatusFd(std::move(status));
}


bool PackageManagerProgressFd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string pkg_action)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   // build the status str
   std::string status;
   strprintf(status, "pmstatus:%s:%.4f:%s\n", StringSplit(PackageName, ":")[0].c_str(),
	 (StepsDone/float(StepsTotal)*100.0), pkg_action.c_str());
   WriteToStatusFd(std::move(status));

   if(_config->FindB("Debug::APT::Progress::PackageManagerFd", false) == true)
      std::cerr << "progress: " << PackageName << " " << xStepsDone
                << " " << xTotalSteps << " " << pkg_action
                << std::endl;


   return true;
}


PackageManagerProgressDeb822Fd::PackageManagerProgressDeb822Fd(int progress_fd)
   : d(NULL), StepsDone(0), StepsTotal(1)
{
   OutStatusFd = progress_fd;
}
PackageManagerProgressDeb822Fd::~PackageManagerProgressDeb822Fd() {}

void PackageManagerProgressDeb822Fd::WriteToStatusFd(std::string s)
{
   FileFd::Write(OutStatusFd, s.c_str(), s.size());   
}

void PackageManagerProgressDeb822Fd::StartDpkg()
{
   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   // send status information that we are about to fork dpkg
   std::string status;
   strprintf(status, "Status: %s\nPercent: %.4f\nMessage: %s\n\n", "progress",
	 (StepsDone/float(StepsTotal)*100.0), _("Running dpkg"));
   WriteToStatusFd(std::move(status));
}

APT_CONST void PackageManagerProgressDeb822Fd::Stop()
{
}

void PackageManagerProgressDeb822Fd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   std::string status;
   strprintf(status, "Status: %s\nPackage: %s\nPercent: %.4f\nMessage: %s\n\n", "Error",
	 PackageName.c_str(), (StepsDone/float(TotalSteps)*100.0), ErrorMessage.c_str());
   WriteToStatusFd(std::move(status));
}

void PackageManagerProgressDeb822Fd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   std::string status;
   strprintf(status, "Status: %s\nPackage: %s\nPercent: %.4f\nMessage: %s\n\n", "ConfFile",
	 PackageName.c_str(), (StepsDone/float(TotalSteps)*100.0), ConfMessage.c_str());
   WriteToStatusFd(std::move(status));
}


bool PackageManagerProgressDeb822Fd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string message)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   std::string status;
   strprintf(status, "Status: %s\nPackage: %s\nPercent: %.4f\nMessage: %s\n\n", "progress",
	 PackageName.c_str(), (StepsDone/float(StepsTotal)*100.0), message.c_str());
   WriteToStatusFd(std::move(status));
   return true;
}


PackageManagerFancy::PackageManagerFancy()
   : d(NULL), child_pty(-1)
{
   // setup terminal size
   old_SIGWINCH = signal(SIGWINCH, PackageManagerFancy::staticSIGWINCH);
   instances.push_back(this);
}
std::vector<PackageManagerFancy*> PackageManagerFancy::instances;

PackageManagerFancy::~PackageManagerFancy()
{
   instances.erase(find(instances.begin(), instances.end(), this));
   signal(SIGWINCH, old_SIGWINCH);
}

void PackageManagerFancy::staticSIGWINCH(int signum)
{
   std::vector<PackageManagerFancy *>::const_iterator I;
   for(I = instances.begin(); I != instances.end(); ++I)
      (*I)->HandleSIGWINCH(signum);
}

PackageManagerFancy::TermSize
PackageManagerFancy::GetTerminalSize()
{
   struct winsize win;
   PackageManagerFancy::TermSize s = { 0, 0 };

   // FIXME: get from "child_pty" instead?
   if(ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&win) != 0)
      return s;

   if(_config->FindB("Debug::InstallProgress::Fancy", false) == true)
      std::cerr << "GetTerminalSize: " << win.ws_row << " x " << win.ws_col << std::endl;

   s.rows = win.ws_row;
   s.columns = win.ws_col;
   return s;
}

void PackageManagerFancy::SetupTerminalScrollArea(int nr_rows)
{
     if(_config->FindB("Debug::InstallProgress::Fancy", false) == true)
        std::cerr << "SetupTerminalScrollArea: " << nr_rows << std::endl;

     if (unlikely(nr_rows <= 1))
	return;

     // scroll down a bit to avoid visual glitch when the screen
     // area shrinks by one row
     std::cout << "\n";
         
     // save cursor
     std::cout << "\0337";
         
     // set scroll region (this will place the cursor in the top left)
     std::cout << "\033[0;" << std::to_string(nr_rows - 1) << "r";
            
     // restore cursor but ensure its inside the scrolling area
     std::cout << "\0338";
     static const char *move_cursor_up = "\033[1A";
     std::cout << move_cursor_up;

     // ensure its flushed
     std::flush(std::cout);

     // setup tty size to ensure xterm/linux console are working properly too
     // see bug #731738
     struct winsize win;
     if (ioctl(child_pty, TIOCGWINSZ, (char *)&win) != -1)
     {
	win.ws_row = nr_rows - 1;
	ioctl(child_pty, TIOCSWINSZ, (char *)&win);
     }
}

void PackageManagerFancy::HandleSIGWINCH(int)
{
   int const nr_terminal_rows = GetTerminalSize().rows;
   SetupTerminalScrollArea(nr_terminal_rows);
   DrawStatusLine();
}

void PackageManagerFancy::Start(int a_child_pty)
{
   child_pty = a_child_pty;
   int const nr_terminal_rows = GetTerminalSize().rows;
   SetupTerminalScrollArea(nr_terminal_rows);
}

void PackageManagerFancy::Stop()
{
   int const nr_terminal_rows = GetTerminalSize().rows;
   if (nr_terminal_rows > 0)
   {
      SetupTerminalScrollArea(nr_terminal_rows + 1);

      // override the progress line (sledgehammer)
      static const char* clear_screen_below_cursor = "\033[J";
      std::cout << clear_screen_below_cursor;
      std::flush(std::cout);
   }
   child_pty = -1;
}

std::string 
PackageManagerFancy::GetTextProgressStr(float Percent, int OutputSize)
{
   std::string output;
   int i;
   
   // should we raise a exception here instead?
   if (Percent < 0.0 || Percent > 1.0 || OutputSize < 3)
      return output;
   
   int BarSize = OutputSize - 2; // bar without the leading "[" and trailing "]"
   output += "[";
   for(i=0; i < BarSize*Percent; i++)
      output += "#";
   for (/*nothing*/; i < BarSize; i++)
      output += ".";
   output += "]";
   return output;
}

bool PackageManagerFancy::StatusChanged(std::string PackageName, 
                                        unsigned int StepsDone,
                                        unsigned int TotalSteps,
                                        std::string HumanReadableAction)
{
   if (!PackageManager::StatusChanged(PackageName, StepsDone, TotalSteps,
          HumanReadableAction))
      return false;

   return DrawStatusLine();
}
bool PackageManagerFancy::DrawStatusLine()
{
   PackageManagerFancy::TermSize const size = GetTerminalSize();
   if (unlikely(size.rows < 1 || size.columns < 1))
      return false;

   static std::string save_cursor = "\0337";
   static std::string restore_cursor = "\0338";

   // green
   static std::string set_bg_color = DeQuoteString(
      _config->Find("Dpkg::Progress-Fancy::Progress-fg", "%1b[42m"));
   // black
   static std::string set_fg_color = DeQuoteString(
      _config->Find("Dpkg::Progress-Fancy::Progress-bg", "%1b[30m"));

   static std::string restore_bg =  "\033[49m";
   static std::string restore_fg = "\033[39m";

   std::cout << save_cursor
      // move cursor position to last row
             << "\033[" << std::to_string(size.rows) << ";0f"
             << set_bg_color
             << set_fg_color
             << progress_str
             << restore_bg
             << restore_fg;
   std::flush(std::cout);

   // draw text progress bar
   if (_config->FindB("Dpkg::Progress-Fancy::Progress-Bar", true))
   {
      int padding = 4;
      float progressbar_size = size.columns - padding - progress_str.size();
      float current_percent = percentage / 100.0;
      std::cout << " " 
                << GetTextProgressStr(current_percent, progressbar_size)
                << " ";
      std::flush(std::cout);
   }

   // restore
   std::cout << restore_cursor;
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

PackageManagerText::PackageManagerText() : PackageManager(), d(NULL) {}
PackageManagerText::~PackageManagerText() {}




} // namespace progress
} // namespace apt
