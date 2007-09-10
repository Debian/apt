// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: md5.h,v 1.6 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   MD5SumValue - Storage for a MD5Sum
   MD5Summation - MD5 Message Digest Algorithm.
   
   This is a C++ interface to a set of MD5Sum functions. The class can
   store a MD5Sum in 16 bytes of memory.
   
   A MD5Sum is used to generate a (hopefully) unique 16 byte number for a
   block of data. This can be used to gaurd against corruption of a file.
   MD5 should not be used for tamper protection, use SHA or something more
   secure.
   
   There are two classes because computing a MD5 is not a continual 
   operation unless 64 byte blocks are used. Also the summation requires an
   extra 18*4 bytes to operate.
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_MD5_H
#define APTPKG_MD5_H


#include <string>
#include <cstring>
#include <algorithm>
#include <stdint.h>

using std::string;
using std::min;

class MD5Summation;

class MD5SumValue
{
   friend class MD5Summation;
   unsigned char Sum[4*4];
   
   public:

   // Accessors
   bool operator ==(const MD5SumValue &rhs) const; 
   string Value() const;
   inline void Value(unsigned char S[16]) 
         {for (int I = 0; I != sizeof(Sum); I++) S[I] = Sum[I];};
   inline operator string() const {return Value();};
   bool Set(string Str);
   inline void Set(unsigned char S[16]) 
         {for (int I = 0; I != sizeof(Sum); I++) Sum[I] = S[I];};

   MD5SumValue(string Str);
   MD5SumValue();
};

class MD5Summation
{
   uint32_t Buf[4];
   unsigned char Bytes[2*4];
   unsigned char In[16*4];
   bool Done;
   
   public:

   bool Add(const unsigned char *Data,unsigned long Size);
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
   MD5SumValue Result();
   
   MD5Summation();
};

#endif
