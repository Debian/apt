// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
// $Id: sha1.cc,v 1.3 2001/05/13 05:15:03 jgg Exp $
/* ######################################################################
   
   SHA1 - SHA-1 Secure Hash Algorithm.
   
   This file is a Public Domain wrapper for the Public Domain SHA1 
   calculation code that is at it's end.

   The algorithm was originally implemented by 
   Steve Reid <sreid@sea-to-sky.net> and later modified by 
   James H. Brown <jbrown@burgoyne.com>.
   
   Modifications for APT were done by Alfredo K. Kojima and Jason 
   Gunthorpe.
   
   Still in the public domain.
   
   Test Vectors (from FIPS PUB 180-1)
   "abc"
   A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
   84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
   A million repetitions of "a"
   34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
   
   ##################################################################### 
 */
									/*}}} */
// Include Files                                                        /*{{{*/
#include <apt-pkg/sha1.h>
#include <apt-pkg/strutl.h>

#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <config.h>
#include <system.h>
									/*}}}*/

// SHA1Transform - Alters an existing SHA-1 hash			/*{{{*/
// ---------------------------------------------------------------------
/* The core of the SHA-1 algorithm. This alters an existing SHA-1 hash to
   reflect the addition of 16 longwords of new data. Other routines convert
   incoming stream data into 16 long word chunks for this routine */

#define rol(value,bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifndef WORDS_BIGENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1),R2,R3,R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

static void SHA1Transform(uint32_t state[5],uint8_t const buffer[64])
{
   uint32_t a,b,c,d,e;
   typedef union
   {
      uint8_t c[64];
      uint32_t l[16];
   }
   CHAR64LONG16;
   CHAR64LONG16 *block;

   uint8_t workspace[64];
   block = (CHAR64LONG16 *)workspace;
   memcpy(block,buffer,sizeof(workspace));

   /* Copy context->state[] to working vars */
   a = state[0];
   b = state[1];
   c = state[2];
   d = state[3];
   e = state[4];
   
   /* 4 rounds of 20 operations each. Loop unrolled. */
   R0(a,b,c,d,e,0);
   R0(e,a,b,c,d,1);
   R0(d,e,a,b,c,2);
   R0(c,d,e,a,b,3);
   R0(b,c,d,e,a,4);
   R0(a,b,c,d,e,5);
   R0(e,a,b,c,d,6);
   R0(d,e,a,b,c,7);
   R0(c,d,e,a,b,8);
   R0(b,c,d,e,a,9);
   R0(a,b,c,d,e,10);
   R0(e,a,b,c,d,11);
   R0(d,e,a,b,c,12);
   R0(c,d,e,a,b,13);
   R0(b,c,d,e,a,14);
   R0(a,b,c,d,e,15);
   R1(e,a,b,c,d,16);
   R1(d,e,a,b,c,17);
   R1(c,d,e,a,b,18);
   R1(b,c,d,e,a,19);
   R2(a,b,c,d,e,20);
   R2(e,a,b,c,d,21);
   R2(d,e,a,b,c,22);
   R2(c,d,e,a,b,23);
   R2(b,c,d,e,a,24);
   R2(a,b,c,d,e,25);
   R2(e,a,b,c,d,26);
   R2(d,e,a,b,c,27);
   R2(c,d,e,a,b,28);
   R2(b,c,d,e,a,29);
   R2(a,b,c,d,e,30);
   R2(e,a,b,c,d,31);
   R2(d,e,a,b,c,32);
   R2(c,d,e,a,b,33);
   R2(b,c,d,e,a,34);
   R2(a,b,c,d,e,35);
   R2(e,a,b,c,d,36);
   R2(d,e,a,b,c,37);
   R2(c,d,e,a,b,38);
   R2(b,c,d,e,a,39);
   R3(a,b,c,d,e,40);
   R3(e,a,b,c,d,41);
   R3(d,e,a,b,c,42);
   R3(c,d,e,a,b,43);
   R3(b,c,d,e,a,44);
   R3(a,b,c,d,e,45);
   R3(e,a,b,c,d,46);
   R3(d,e,a,b,c,47);
   R3(c,d,e,a,b,48);
   R3(b,c,d,e,a,49);
   R3(a,b,c,d,e,50);
   R3(e,a,b,c,d,51);
   R3(d,e,a,b,c,52);
   R3(c,d,e,a,b,53);
   R3(b,c,d,e,a,54);
   R3(a,b,c,d,e,55);
   R3(e,a,b,c,d,56);
   R3(d,e,a,b,c,57);
   R3(c,d,e,a,b,58);
   R3(b,c,d,e,a,59);
   R4(a,b,c,d,e,60);
   R4(e,a,b,c,d,61);
   R4(d,e,a,b,c,62);
   R4(c,d,e,a,b,63);
   R4(b,c,d,e,a,64);
   R4(a,b,c,d,e,65);
   R4(e,a,b,c,d,66);
   R4(d,e,a,b,c,67);
   R4(c,d,e,a,b,68);
   R4(b,c,d,e,a,69);
   R4(a,b,c,d,e,70);
   R4(e,a,b,c,d,71);
   R4(d,e,a,b,c,72);
   R4(c,d,e,a,b,73);
   R4(b,c,d,e,a,74);
   R4(a,b,c,d,e,75);
   R4(e,a,b,c,d,76);
   R4(d,e,a,b,c,77);
   R4(c,d,e,a,b,78);
   R4(b,c,d,e,a,79);
   
   /* Add the working vars back into context.state[] */
   state[0] += a;
   state[1] += b;
   state[2] += c;
   state[3] += d;
   state[4] += e;   
}
									/*}}}*/

// SHA1SumValue::SHA1SumValue - Constructs the summation from a string  /*{{{*/
// ---------------------------------------------------------------------
/* The string form of a SHA1 is a 40 character hex number */
SHA1SumValue::SHA1SumValue(string Str)
{
   memset(Sum,0,sizeof(Sum));
   Set(Str);
}

									/*}}} */
// SHA1SumValue::SHA1SumValue - Default constructor                     /*{{{*/
// ---------------------------------------------------------------------
/* Sets the value to 0 */
SHA1SumValue::SHA1SumValue()
{
   memset(Sum,0,sizeof(Sum));
}

									/*}}} */
// SHA1SumValue::Set - Set the sum from a string                        /*{{{*/
// ---------------------------------------------------------------------
/* Converts the hex string into a set of chars */
bool SHA1SumValue::Set(string Str)
{
   return Hex2Num(Str,Sum,sizeof(Sum));
}

									/*}}} */
// SHA1SumValue::Value - Convert the number into a string               /*{{{*/
// ---------------------------------------------------------------------
/* Converts the set of chars into a hex string in lower case */
string SHA1SumValue::Value() const
{
   char Conv[16] =
      { '0','1','2','3','4','5','6','7','8','9','a','b',
      'c','d','e','f'
   };
   char Result[41];
   Result[40] = 0;

   // Convert each char into two letters
   int J = 0;
   int I = 0;
   for (; I != 40; J++,I += 2)
   {
      Result[I] = Conv[Sum[J] >> 4];
      Result[I + 1] = Conv[Sum[J] & 0xF];
   }

   return string(Result);
}

									/*}}} */
// SHA1SumValue::operator == - Comparator                               /*{{{*/
// ---------------------------------------------------------------------
/* Call memcmp on the buffer */
bool SHA1SumValue::operator == (const SHA1SumValue & rhs) const
{
   return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
}
									/*}}}*/
// SHA1Summation::SHA1Summation - Constructor                           /*{{{*/
// ---------------------------------------------------------------------
/* */
SHA1Summation::SHA1Summation()
{
   uint32_t *state = (uint32_t *)State;
   uint32_t *count = (uint32_t *)Count;
   
   /* SHA1 initialization constants */
   state[0] = 0x67452301;
   state[1] = 0xEFCDAB89;
   state[2] = 0x98BADCFE;
   state[3] = 0x10325476;
   state[4] = 0xC3D2E1F0;
   count[0] = 0;
   count[1] = 0;
   Done = false;
}
									/*}}}*/
// SHA1Summation::Result - Return checksum value                        /*{{{*/
// ---------------------------------------------------------------------
/* Add() may not be called after this */
SHA1SumValue SHA1Summation::Result()
{
   uint32_t *state = (uint32_t *)State;
   uint32_t *count = (uint32_t *)Count;
   
   // Apply the padding
   if (Done == false)
   {
      unsigned char finalcount[8];

      for (unsigned i = 0; i < 8; i++)
      {
	 // Endian independent
	 finalcount[i] = (unsigned char) ((count[(i >= 4 ? 0 : 1)]
					   >> ((3 - (i & 3)) * 8)) & 255);	
      }
      
      Add((unsigned char *) "\200",1);
      while ((count[0] & 504) != 448)
	 Add((unsigned char *) "\0",1);
      
      Add(finalcount,8);	/* Should cause a SHA1Transform() */
      
   }

   Done = true;

   // Transfer over the result
   SHA1SumValue Value;
   for (unsigned i = 0; i < 20; i++)
   {
      Value.Sum[i] = (unsigned char)
	 ((state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
   }
   return Value;
}
									/*}}}*/
// SHA1Summation::Add - Adds content of buffer into the checksum        /*{{{*/
// ---------------------------------------------------------------------
/* May not be called after Result() is called */
bool SHA1Summation::Add(const unsigned char *data,unsigned long len)
{
   if (Done)
      return false;

   uint32_t *state = (uint32_t *)State;
   uint32_t *count = (uint32_t *)Count;
   uint8_t *buffer = (uint8_t *)Buffer;
   uint32_t i,j;

   j = (count[0] >> 3) & 63;
   if ((count[0] += len << 3) < (len << 3))
      count[1]++;
   count[1] += (len >> 29);
   if ((j + len) > 63)
   {
      memcpy(&buffer[j],data,(i = 64 - j));
      SHA1Transform(state,buffer);
      for (; i + 63 < len; i += 64)
      {
	 SHA1Transform(state,&data[i]);
      }
      j = 0;
   }
   else
      i = 0;
   memcpy(&buffer[j],&data[i],len - i);
   
   return true;
}
									/*}}}*/
// SHA1Summation::AddFD - Add content of file into the checksum         /*{{{*/
// ---------------------------------------------------------------------
/* */
bool SHA1Summation::AddFD(int Fd,unsigned long Size)
{
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
