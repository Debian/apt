/*
 * Cryptographic API.							{{{
 *
 * SHA-512, as specified in
 * http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */									/*}}}*/

#ifdef __GNUG__
#pragma implementation "apt-pkg/sha2.h"
#endif

#include <apt-pkg/sha2.h>
#include <apt-pkg/strutl.h>

// SHA2Summation::AddFD - Add content of file into the checksum      /*{{{*/
// ---------------------------------------------------------------------
/* */
bool SHA2SummationBase::AddFD(int Fd,unsigned long Size){
   unsigned char Buf[64 * 64];
   int Res = 0;
   int ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned n = sizeof(Buf);
      if (!ToEOF) n = min(Size,(unsigned long)n);
      Res = read(Fd,Buf,n);
      if (Res < 0 || (!ToEOF && (unsigned) Res != n)) // error, or short read
         return false;
      if (ToEOF && Res == 0) // EOF
         break;
      Size -= Res;
      Add(Buf,Res);
   }
   return true;
}
                                                                       /*}}}*/

