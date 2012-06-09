#include <apt-pkg/cmndline.h>

#include "assert.h"

int main()
{
   CommandLine::Args Args[] = {
      { 't', 0, "Test::Worked", 0 },
      { 'z', "zero", "Test::Zero", 0 },
      {0,0,0,0}
   };

   CommandLine CmdL(Args,_config);
   char const * argv[] = { "test", "--zero", "-t" };
   CmdL.Parse(3 , argv);

   equals(true, _config->FindB("Test::Worked", false));
   equals(true, _config->FindB("Test::Zero", false));

   return 0;
}
