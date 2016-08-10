#include <config.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <signal.h>

#include "http.h"

int main(int, const char *argv[])
{
   // ignore SIGPIPE, this can happen on write() if the socket
   // closes the connection (this is dealt with via ServerDie())
   signal(SIGPIPE, SIG_IGN);
   std::string Binary = flNotDir(argv[0]);
   if (Binary.find('+') == std::string::npos && Binary != "http")
      Binary.append("+http");
   return HttpMethod(std::move(Binary)).Loop();
}
