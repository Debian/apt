// Cryptographic API Base
#include <config.h>

#include <unistd.h>
#include "hashsum_template.h"

// Summation::AddFD - Add content of file into the checksum		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SummationImplementation::AddFD(int const Fd, unsigned long long Size) {
   unsigned char Buf[64 * 64];
   ssize_t Res = 0;
   int ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned long long n = sizeof(Buf);
      if (!ToEOF) n = std::min(Size, n);
      Res = read(Fd, Buf, n);
      if (Res < 0 || (!ToEOF && Res != (ssize_t) n)) // error, or short read
	 return false;
      if (ToEOF && Res == 0) // EOF
	 break;
      Size -= Res;
      Add(Buf,Res);
   }
   return true;
}
bool SummationImplementation::AddFD(FileFd &Fd, unsigned long long Size) {
   unsigned char Buf[64 * 64];
   bool ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned long long n = sizeof(Buf);
      if (!ToEOF) n = std::min(Size, n);
      unsigned long long a = 0;
      if (Fd.Read(Buf, n, &a) == false) // error
	 return false;
      if (ToEOF == false)
      {
	 if (a != n) // short read
	    return false;
      }
      else if (a == 0) // EOF
	 break;
      Size -= a;
      Add(Buf, a);
   }
   return true;
}
									/*}}}*/
