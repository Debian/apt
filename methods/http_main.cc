#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>

#include "connect.h"
#include "rfc2553emu.h"
#include "http.h"


int main()
{
   setlocale(LC_ALL, "");

   HttpMethod Mth;
   return Mth.Loop();
}
