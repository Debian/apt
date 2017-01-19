#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <apt-private/private-utils.h>

#include <cstdlib>
#include <sstream>
#include <unistd.h>

// DisplayFileInPager - Display File with pager				/*{{{*/
bool DisplayFileInPager(std::string const &filename)
{
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char *Args[3];
      Args[1] = filename.c_str();
      Args[2] = NULL;
      if (isatty(STDOUT_FILENO) == 1)
      {
	 // likely installed, provided by sensible-utils
	 std::string const pager = _config->Find("Dir::Bin::Pager",
	       "sensible-pager");
	 Args[0] = pager.c_str();
	 execvp(Args[0],(char **)Args);
	 // lets try some obvious alternatives
	 Args[0] = getenv("PAGER");
	 if (Args[0] != NULL)
	    execvp(Args[0],(char **)Args);

	 Args[0] = "pager";
	 execvp(Args[0],(char **)Args);
      }
      // we could read the file ourselves, but… meh
      Args[0] = "cat";
      execvp(Args[0],(char **)Args);
      exit(100);
   }

   // Wait for the subprocess
   return ExecWait(Process, "pager", false);
}
									/*}}}*/
// EditFileInSensibleEditor - Edit File with editor			/*{{{*/
bool EditFileInSensibleEditor(std::string const &filename)
{
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      // likely installed, provided by sensible-utils
      std::string const editor = _config->Find("Dir::Bin::Editor",
	    "sensible-editor");
      const char *Args[3];
      Args[0] = editor.c_str();
      Args[1] = filename.c_str();
      Args[2] = NULL;
      execvp(Args[0],(char **)Args);
      // the usual suspects we can try as an alternative
      Args[0] = getenv("VISUAL");
      if (Args[0] != NULL)
	 execvp(Args[0],(char **)Args);

      Args[0] = getenv("EDITOR");
      if (Args[0] != NULL)
	 execvp(Args[0],(char **)Args);

      Args[0] = "editor";
      execvp(Args[0],(char **)Args);
      exit(100);
   }

   // Wait for the subprocess
   return ExecWait(Process, "editor", false);
}
									/*}}}*/
time_t GetSecondsSinceEpoch()						/*{{{*/
{
   auto const source_date_epoch = getenv("SOURCE_DATE_EPOCH");
   if (source_date_epoch == nullptr)
      return time(nullptr);

   time_t epoch;
   std::stringstream ss(source_date_epoch);
   ss >> epoch;

   if (ss.fail() || !ss.eof())
   {
      _error->Warning("Environment variable SOURCE_DATE_EPOCH was ignored as it has an invalid value: \"%s\"",
            source_date_epoch);
      return time(nullptr);
   }

   return epoch;
}
									/*}}}*/
