// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: hashes.h,v 1.2 2001/03/11 05:30:20 jgg Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_HASHES_H
#define APTPKG_HASHES_H

#ifdef __GNUG__
#pragma interface "apt-pkg/hashes.h"
#endif 

#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>

#include <algorithm>

using std::min;

class Hashes
{
   public:

   MD5Summation MD5;
   SHA1Summation SHA1;
   
   inline bool Add(const unsigned char *Data,unsigned long Size)
   {
      return MD5.Add(Data,Size) && SHA1.Add(Data,Size);
   };
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
};

#endif
