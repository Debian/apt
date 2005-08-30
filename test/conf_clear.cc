#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

using namespace std;

int main(int argc,const char *argv[])
{
   Configuration Cnf;

   cout << "adding elements" << endl;
   Cnf.Set("APT::Keep-Fds::",28);
   Cnf.Set("APT::Keep-Fds::",17);
   Cnf.Set("APT::Keep-Fds::",47);
   Cnf.Dump();

   cout << "Removing  elements" << endl;
   Cnf.Clear("APT::Keep-Fds",17);
   Cnf.Clear("APT::Keep-Fds",28);
   Cnf.Clear("APT::Keep-Fds",47);
   Cnf.Dump();

   return 0;
}
