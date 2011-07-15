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


#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>

#include <algorithm>
#include <vector>
#include <cstring>

using std::min;
using std::vector;

// helper class that contains hash function name
// and hash
class HashString
{
 protected:
   string Type;
   string Hash;
   static const char * _SupportedHashes[10];

 public:
   HashString(string Type, string Hash);
   HashString(string StringedHashString);  // init from str as "type:hash"
   HashString();

   // get hash type used
   string HashType() { return Type; };

   // verify the given filename against the currently loaded hash
   bool VerifyFile(string filename) const;

   // helper
   string toStr() const;                    // convert to str as "type:hash"
   bool empty() const;

   // return the list of hashes we support
   static const char** SupportedHashes();
};

class Hashes
{
   public:

   MD5Summation MD5;
   SHA1Summation SHA1;
   SHA256Summation SHA256;
   SHA512Summation SHA512;
   
   inline bool Add(const unsigned char *Data,unsigned long Size)
   {
      return MD5.Add(Data,Size) && SHA1.Add(Data,Size) && SHA256.Add(Data,Size) && SHA512.Add(Data,Size);
   };
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   inline bool AddFD(int const Fd,unsigned long Size = 0)
   { return AddFD(Fd, Size, true, true, true, true); };
   bool AddFD(int const Fd, unsigned long Size, bool const addMD5,
	      bool const addSHA1, bool const addSHA256, bool const addSHA512);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
};

#endif
