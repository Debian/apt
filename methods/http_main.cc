#include <config.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <signal.h>

#include "http.h"

int main()
{
   // ignore SIGPIPE, this can happen on write() if the socket
   // closes the connection (this is dealt with via ServerDie())
   signal(SIGPIPE, SIG_IGN);

   return HttpMethod().Loop();
}
