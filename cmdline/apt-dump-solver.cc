// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################

   dummy solver to get quickly a scenario file out of APT

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>

#include <sys/types.h>
#include <sys/wait.h>

#include <string.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
      _("Usage: apt-dump-solver\n"
	    "\n"
	    "apt-dump-solver is an interface to store an EDSP scenario in\n"
	    "a file and optionally forwards it to another solver.\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {};
}
									/*}}}*/
static int WriteError(char const * const uid, std::ostringstream &out, FileFd &stdoutfd, pid_t const &Solver)/*{{{*/
{
   _error->DumpErrors(out);
   // ensure the solver isn't printing into "our" error message, too
   if (Solver != 0)
      ExecWait(Solver, "dump", true);
   EDSP::WriteError(uid, out.str(), stdoutfd);
   return 0;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine CmdL;
   ParseCommandLine(CmdL, APT_CMD::APT_DUMP_SOLVER, &_config, nullptr, argc, argv, &ShowHelp, &GetCommands);
   _config->Clear("Dir::Log");

   bool const is_forwarding_dumper = (CmdL.FileSize() != 0);

   FileFd stdoutfd;
   if (stdoutfd.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly | FileFd::BufferedWrite, true) == false)
      return 252;

   FileFd dump;
   char const * const filename = is_forwarding_dumper ? CmdL.FileList[0] : getenv("APT_EDSP_DUMP_FILENAME");
   if (filename == nullptr || strlen(filename) == 0)
   {
      if (is_forwarding_dumper == false)
      {
	 EDSP::WriteError("ERR_NO_FILENAME", "You have to set the environment variable APT_EDSP_DUMP_FILENAME\n"
	       "to a valid filename to store the dump of EDSP solver input in.\n"
	       "For example with: export APT_EDSP_DUMP_FILENAME=/tmp/dump.edsp", stdoutfd);
	 return 0;
      }
   }
   else
   {
      // ignore errors here as logging isn't really critical
      _error->PushToStack();
      if (dump.Open(filename, FileFd::WriteOnly | FileFd::Exclusive | FileFd::Create, FileFd::Extension, 0644) == false &&
	    is_forwarding_dumper == false)
      {
	 _error->MergeWithStack();
	 std::ostringstream out;
	 out << "Writing EDSP solver input to file '" << filename << "' failed as it couldn't be created!\n";
	 return WriteError("ERR_CREATE_FILE", out, stdoutfd, 0);
      }
      _error->RevertToStack();
   }

   pid_t Solver = 0;
   FileFd forward;
   if (is_forwarding_dumper)
   {
      signal(SIGPIPE, SIG_IGN);
      int external[] = {-1, -1};
      if (pipe(external) != 0)
	 return 250;
      for (int i = 0; i < 2; ++i)
	 SetCloseExec(external[i], true);

      Solver = ExecFork();
      if (Solver == 0) {
	 _config->Set("APT::Sandbox::User", _config->Find("APT::Solver::RunAsUser", _config->Find("APT::Sandbox::User")));
	 DropPrivileges();
	 dup2(external[0], STDIN_FILENO);
	 execv(CmdL.FileList[1], const_cast<char**>(CmdL.FileList + 1));
	 std::cerr << "Failed to execute  '" << CmdL.FileList[1] << "'!" << std::endl;
	 _exit(100);
      }
      close(external[0]);

      if (WaitFd(external[1], true, 5) == false)
	 return 251;

      if (forward.OpenDescriptor(external[1], FileFd::WriteOnly | FileFd::BufferedWrite, true) == false)
	 return 252;
   }

   DropPrivileges();

   FileFd input;
   if (input.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false)
   {
      std::ostringstream out;
      out << "Writing EDSP solver input to file '" << filename << "' failed as stdin couldn't be opened!\n";
      return WriteError("ERR_READ_ERROR", out, stdoutfd, Solver);
   }

   std::unique_ptr<char[]> Buf(new char[APT_BUFFER_SIZE]);
   unsigned long long ToRead = 0;
   do {
      if (input.Read(Buf.get(), APT_BUFFER_SIZE, &ToRead) == false)
      {
	 std::ostringstream out;
	 out << "Writing EDSP solver input to file '" << filename << "' failed as reading from stdin failed!\n";
	 return WriteError("ERR_READ_ERROR", out, stdoutfd, Solver);
      }
      if (ToRead == 0)
	 break;
      if (forward.IsOpen() && forward.Failed() == false && forward.Write(Buf.get(),ToRead) == false)
	 forward.Close();
      if (dump.IsOpen() && dump.Failed() == false && dump.Write(Buf.get(),ToRead) == false)
	 dump.Close();
   } while (true);
   input.Close();
   forward.Close();
   dump.Close();

   if (is_forwarding_dumper)
   {
      // Wait and collect the error code
      int Status;
      while (waitpid(Solver, &Status, 0) != Solver)
      {
	 if (errno == EINTR)
	    continue;

	 std::ostringstream out;
	 ioprintf(out, _("Waited for %s but it wasn't there"), CmdL.FileList[1]);
	 return WriteError("ERR_FORWARD", out, stdoutfd, 0);
      }
      if (WIFEXITED(Status))
	 return WEXITSTATUS(Status);
      else
	 return 255;
   }
   else if (_error->PendingError())
   {
      std::ostringstream out;
      out << "Writing EDSP solver input to file '" << filename << "' failed due to write errors!\n";
      return WriteError("ERR_WRITE_ERROR", out, stdoutfd, Solver);
   }
   else
      EDSP::WriteError("ERR_JUST_DUMPING", "I am too dumb, i can just dump!\nPlease use one of my friends instead!", stdoutfd);
   return 0;
}
