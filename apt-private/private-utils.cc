#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <apt-private/private-utils.h>

#include <cstdlib>
#include <unistd.h>

// DisplayFileInPager - Display File with pager        			/*{{{*/
void DisplayFileInPager(std::string filename)
{
   std::string pager = _config->Find("Dir::Bin::Pager", 
                                        "/usr/bin/sensible-pager");

   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char *Args[3];
      Args[0] = pager.c_str();
      Args[1] = filename.c_str();
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      exit(100);
   }
         
   // Wait for the subprocess
   ExecWait(Process, "sensible-pager", false);
}
									/*}}}*/
// EditFileInSensibleEditor - Edit File with editor    			/*{{{*/
void EditFileInSensibleEditor(std::string filename)
{
   std::string editor = _config->Find("Dir::Bin::Editor", 
                                        "/usr/bin/sensible-editor");

   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char *Args[3];
      Args[0] = editor.c_str();
      Args[1] = filename.c_str();
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      exit(100);
   }
         
   // Wait for the subprocess
   ExecWait(Process, "sensible-editor", false);
}
									/*}}}*/
