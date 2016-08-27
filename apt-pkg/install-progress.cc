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
#include <cmath>

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

static std::string GetProgressFdString(char const * const status,
      char const * const pkg, unsigned long long Done,
      unsigned long long Total, char const * const msg)
{
   float const progress{Done / static_cast<float>(Total) * 100};
   std::ostringstream str;
   str.imbue(std::locale::classic());
   str.precision(4);
   str << status << ':' << pkg << ':' << std::fixed << progress << ':' << msg << '\n';
   return str.str();
}

void PackageManagerProgressFd::StartDpkg()
{
   if(OutStatusFd <= 0)
      return;

   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   // send status information that we are about to fork dpkg
   WriteToStatusFd(GetProgressFdString("pmstatus", "dpkg-exec", StepsDone, StepsTotal, _("Running dpkg")));
}

APT_CONST void PackageManagerProgressFd::Stop()
{
}

void PackageManagerProgressFd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   WriteToStatusFd(GetProgressFdString("pmerror", PackageName.c_str(),
	    StepsDone, TotalSteps, ErrorMessage.c_str()));
}

void PackageManagerProgressFd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   WriteToStatusFd(GetProgressFdString("pmconffile", PackageName.c_str(),
	    StepsDone, TotalSteps, ConfMessage.c_str()));
}


bool PackageManagerProgressFd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string pkg_action)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   WriteToStatusFd(GetProgressFdString("pmstatus", StringSplit(PackageName, ":")[0].c_str(),
	    StepsDone, StepsTotal, pkg_action.c_str()));

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

static std::string GetProgressDeb822String(char const * const status,
      char const * const pkg, unsigned long long Done,
      unsigned long long Total, char const * const msg)
{
   float const progress{Done / static_cast<float>(Total) * 100};
   std::ostringstream str;
   str.imbue(std::locale::classic());
   str.precision(4);
   str << "Status: " << status << '\n';
   if (pkg != nullptr)
      str << "Package: " << pkg << '\n';
   str << "Percent: " << std::fixed << progress << '\n'
      << "Message: " << msg << "\n\n";
   return str.str();
}

void PackageManagerProgressDeb822Fd::StartDpkg()
{
   // FIXME: use SetCloseExec here once it taught about throwing
   //        exceptions instead of doing _exit(100) on failure
   fcntl(OutStatusFd,F_SETFD,FD_CLOEXEC); 

   WriteToStatusFd(GetProgressDeb822String("progress", nullptr, StepsDone, StepsTotal, _("Running dpkg")));
}

APT_CONST void PackageManagerProgressDeb822Fd::Stop()
{
}

void PackageManagerProgressDeb822Fd::Error(std::string PackageName,
                                     unsigned int StepsDone,
                                     unsigned int TotalSteps,
                                     std::string ErrorMessage)
{
   WriteToStatusFd(GetProgressDeb822String("Error", PackageName.c_str(), StepsDone, TotalSteps, ErrorMessage.c_str()));
}

void PackageManagerProgressDeb822Fd::ConffilePrompt(std::string PackageName,
                                              unsigned int StepsDone,
                                              unsigned int TotalSteps,
                                              std::string ConfMessage)
{
   WriteToStatusFd(GetProgressDeb822String("ConfFile", PackageName.c_str(), StepsDone, TotalSteps, ConfMessage.c_str()));
}


bool PackageManagerProgressDeb822Fd::StatusChanged(std::string PackageName, 
                                             unsigned int xStepsDone,
                                             unsigned int xTotalSteps,
                                             std::string message)
{
   StepsDone = xStepsDone;
   StepsTotal = xTotalSteps;

   WriteToStatusFd(GetProgressDeb822String("progress", PackageName.c_str(), StepsDone, StepsTotal, message.c_str()));
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
   if (unlikely(OutputSize < 3))
      return output;

   int const BarSize = OutputSize - 2; // bar without the leading "[" and trailing "]"
   int const BarDone = std::max(0, std::min(BarSize, static_cast<int>(std::floor(Percent * BarSize))));
   output.append("[");
   std::fill_n(std::fill_n(std::back_inserter(output), BarDone, '#'), BarSize - BarDone, '.');
   output.append("]");
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
