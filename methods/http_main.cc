#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>
#include <signal.h>

#include "connect.h"
#include "rfc2553emu.h"
#include "http.h"


int main()
{
   setlocale(LC_ALL, "");

   // ignore SIGPIPE, this can happen on write() if the socket
   // closes the connection (this is dealt with via ServerDie())
   signal(SIGPIPE, SIG_IGN);

   HttpMethod Mth;
   return Mth.Loop();
}
