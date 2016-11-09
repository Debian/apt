#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <unistd.h>

int main(int const argc, const char * argv[])
{
   CommandLine::Args Args[] = {
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0, "user", "APT::Sandbox::User", CommandLine::HasArg},
      {0,0,0,0}
   };

   CommandLine CmdL(Args, _config);
   if(CmdL.Parse(argc,argv) == false || DropPrivileges() == false)
   {
      _error->DumpErrors(std::cerr, GlobalError::DEBUG);
      return 42;
   }

   return execv(CmdL.FileList[0], const_cast<char**>(CmdL.FileList));
}
