// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: hashes.h,v 1.1 2001/03/06 07:15:29 jgg Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_HASHES_H
#define APTPKG_HASHES_H

#ifdef __GNUG__
#pragma interface "apt-pkg/hashesh.h"
#endif 

#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>

class Hashes
{
   public:

   MD5Summation MD5;
   SHA1Summation SHA1;
   
   inline bool Add(const unsigned char *Data,unsigned long Size)
   {
      MD5.Add(Data,Size);
      SHA1.Add(Data,Size);
   };
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
};

#endif
