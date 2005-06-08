// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: hashes.cc,v 1.1 2001/03/06 07:15:29 jgg Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/hashes.h"
#endif

#include <apt-pkg/hashes.h>
    
#include <unistd.h>    
#include <system.h>    
									/*}}}*/

// Hashes::AddFD - Add the contents of the FD				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Hashes::AddFD(int Fd,unsigned long Size)
{
   unsigned char Buf[64*64];
   int Res = 0;
   while (Size != 0)
   {
      Res = read(Fd,Buf,min(Size,(unsigned long)sizeof(Buf)));
      if (Res < 0 || (unsigned)Res != min(Size,(unsigned long)sizeof(Buf)))
	 return false;
      Size -= Res;
      MD5.Add(Buf,Res);
      SHA1.Add(Buf,Res);
   }
   return true;
}
									/*}}}*/

