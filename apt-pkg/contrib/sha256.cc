/*
 * Cryptographic API.
 *
 * SHA-256, as specified in
 * http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf
 *
 * SHA-256 code by Jean-Luc Cooke <jlcooke@certainkey.com>.
 *
 * Copyright (c) Jean-Luc Cooke <jlcooke@certainkey.com>
 * Copyright (c) Andrew McDonald <andrew@mcdonald.org.uk>
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 *
 * Ported from the Linux kernel to Apt by Anthony Towns <ajt@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */

#ifdef __GNUG__
#pragma implementation "apt-pkg/sha256.h"
#endif


#define SHA256_DIGEST_SIZE      32
#define SHA256_HMAC_BLOCK_SIZE  64

#define ror32(value,bits) (((value) >> (bits)) | ((value) << (32 - (bits))))

#include <apt-pkg/sha256.h>
#include <apt-pkg/strutl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

typedef uint32_t u32;
typedef uint8_t  u8;

static inline u32 Ch(u32 x, u32 y, u32 z)
{
        return z ^ (x & (y ^ z));
}

static inline u32 Maj(u32 x, u32 y, u32 z)
{
        return (x & y) | (z & (x | y));
}

#define e0(x)       (ror32(x, 2) ^ ror32(x,13) ^ ror32(x,22))
#define e1(x)       (ror32(x, 6) ^ ror32(x,11) ^ ror32(x,25))
#define s0(x)       (ror32(x, 7) ^ ror32(x,18) ^ (x >> 3))
#define s1(x)       (ror32(x,17) ^ ror32(x,19) ^ (x >> 10))

#define H0         0x6a09e667
#define H1         0xbb67ae85
#define H2         0x3c6ef372
#define H3         0xa54ff53a
#define H4         0x510e527f
#define H5         0x9b05688c
#define H6         0x1f83d9ab
#define H7         0x5be0cd19

static inline void LOAD_OP(int I, u32 *W, const u8 *input)
{
	W[I] = (  ((u32) input[I * 4 + 0] << 24)
		| ((u32) input[I * 4 + 1] << 16)
		| ((u32) input[I * 4 + 2] << 8)
		| ((u32) input[I * 4 + 3]));
}

static inline void BLEND_OP(int I, u32 *W)
{
        W[I] = s1(W[I-2]) + W[I-7] + s0(W[I-15]) + W[I-16];
}

static void sha256_transform(u32 *state, const u8 *input)
{
        u32 a, b, c, d, e, f, g, h, t1, t2;
        u32 W[64];
        int i;

        /* load the input */
        for (i = 0; i < 16; i++)
                LOAD_OP(i, W, input);

        /* now blend */
        for (i = 16; i < 64; i++)
                BLEND_OP(i, W);
    
        /* load the state into our registers */
        a=state[0];  b=state[1];  c=state[2];  d=state[3];
        e=state[4];  f=state[5];  g=state[6];  h=state[7];

        /* now iterate */
        t1 = h + e1(e) + Ch(e,f,g) + 0x428a2f98 + W[ 0];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0x71374491 + W[ 1];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0xb5c0fbcf + W[ 2];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0xe9b5dba5 + W[ 3];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x3956c25b + W[ 4];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0x59f111f1 + W[ 5];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x923f82a4 + W[ 6];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0xab1c5ed5 + W[ 7];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0xd807aa98 + W[ 8];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0x12835b01 + W[ 9];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0x243185be + W[10];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0x550c7dc3 + W[11];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x72be5d74 + W[12];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0x80deb1fe + W[13];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x9bdc06a7 + W[14];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0xc19bf174 + W[15];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0xe49b69c1 + W[16];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0xefbe4786 + W[17];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0x0fc19dc6 + W[18];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0x240ca1cc + W[19];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x2de92c6f + W[20];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0x4a7484aa + W[21];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x5cb0a9dc + W[22];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0x76f988da + W[23];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0x983e5152 + W[24];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0xa831c66d + W[25];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0xb00327c8 + W[26];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0xbf597fc7 + W[27];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0xc6e00bf3 + W[28];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0xd5a79147 + W[29];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x06ca6351 + W[30];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0x14292967 + W[31];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0x27b70a85 + W[32];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0x2e1b2138 + W[33];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0x4d2c6dfc + W[34];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0x53380d13 + W[35];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x650a7354 + W[36];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0x766a0abb + W[37];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x81c2c92e + W[38];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0x92722c85 + W[39];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0xa2bfe8a1 + W[40];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0xa81a664b + W[41];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0xc24b8b70 + W[42];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0xc76c51a3 + W[43];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0xd192e819 + W[44];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0xd6990624 + W[45];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0xf40e3585 + W[46];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0x106aa070 + W[47];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0x19a4c116 + W[48];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0x1e376c08 + W[49];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0x2748774c + W[50];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0x34b0bcb5 + W[51];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x391c0cb3 + W[52];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0x4ed8aa4a + W[53];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0x5b9cca4f + W[54];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0x682e6ff3 + W[55];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        t1 = h + e1(e) + Ch(e,f,g) + 0x748f82ee + W[56];
        t2 = e0(a) + Maj(a,b,c);    d+=t1;    h=t1+t2;
        t1 = g + e1(d) + Ch(d,e,f) + 0x78a5636f + W[57];
        t2 = e0(h) + Maj(h,a,b);    c+=t1;    g=t1+t2;
        t1 = f + e1(c) + Ch(c,d,e) + 0x84c87814 + W[58];
        t2 = e0(g) + Maj(g,h,a);    b+=t1;    f=t1+t2;
        t1 = e + e1(b) + Ch(b,c,d) + 0x8cc70208 + W[59];
        t2 = e0(f) + Maj(f,g,h);    a+=t1;    e=t1+t2;
        t1 = d + e1(a) + Ch(a,b,c) + 0x90befffa + W[60];
        t2 = e0(e) + Maj(e,f,g);    h+=t1;    d=t1+t2;
        t1 = c + e1(h) + Ch(h,a,b) + 0xa4506ceb + W[61];
        t2 = e0(d) + Maj(d,e,f);    g+=t1;    c=t1+t2;
        t1 = b + e1(g) + Ch(g,h,a) + 0xbef9a3f7 + W[62];
        t2 = e0(c) + Maj(c,d,e);    f+=t1;    b=t1+t2;
        t1 = a + e1(f) + Ch(f,g,h) + 0xc67178f2 + W[63];
        t2 = e0(b) + Maj(b,c,d);    e+=t1;    a=t1+t2;

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;

        /* clear any sensitive info... */
        a = b = c = d = e = f = g = h = t1 = t2 = 0;
        memset(W, 0, 64 * sizeof(u32));
}

SHA256Summation::SHA256Summation()
{
        Sum.state[0] = H0;
        Sum.state[1] = H1;
        Sum.state[2] = H2;
        Sum.state[3] = H3;
        Sum.state[4] = H4;
        Sum.state[5] = H5;
        Sum.state[6] = H6;
        Sum.state[7] = H7;
        Sum.count[0] = Sum.count[1] = 0;
        memset(Sum.buf, 0, sizeof(Sum.buf));
        Done = false;
}

bool SHA256Summation::Add(const u8 *data, unsigned long len)
{
        struct sha256_ctx *sctx = &Sum;
        unsigned int i, index, part_len;

        if (Done) return false;

        /* Compute number of bytes mod 128 */
        index = (unsigned int)((sctx->count[0] >> 3) & 0x3f);

        /* Update number of bits */
        if ((sctx->count[0] += (len << 3)) < (len << 3)) {
                sctx->count[1]++;
                sctx->count[1] += (len >> 29);
        }

        part_len = 64 - index;

        /* Transform as many times as possible. */
        if (len >= part_len) {
                memcpy(&sctx->buf[index], data, part_len);
                sha256_transform(sctx->state, sctx->buf);

                for (i = part_len; i + 63 < len; i += 64)
                        sha256_transform(sctx->state, &data[i]);
                index = 0;
        } else {
                i = 0;
        }

        /* Buffer remaining input */
        memcpy(&sctx->buf[index], &data[i], len-i);

        return true;
}

SHA256SumValue SHA256Summation::Result()
{
   struct sha256_ctx *sctx = &Sum;
   if (!Done) {
        u8 bits[8];
        unsigned int index, pad_len, t;
        static const u8 padding[64] = { 0x80, };

        /* Save number of bits */
        t = sctx->count[0];
        bits[7] = t; t >>= 8;
        bits[6] = t; t >>= 8;
        bits[5] = t; t >>= 8;
        bits[4] = t;
        t = sctx->count[1];
        bits[3] = t; t >>= 8;
        bits[2] = t; t >>= 8;
        bits[1] = t; t >>= 8;
        bits[0] = t;

        /* Pad out to 56 mod 64. */
        index = (sctx->count[0] >> 3) & 0x3f;
        pad_len = (index < 56) ? (56 - index) : ((64+56) - index);
        Add(padding, pad_len);

        /* Append length (before padding) */
        Add(bits, 8);
   }

   Done = true;

   /* Store state in digest */

   SHA256SumValue res;
   u8 *out = res.Sum;

   int i, j;
   unsigned int t;
   for (i = j = 0; i < 8; i++, j += 4) {
      t = sctx->state[i];
      out[j+3] = t; t >>= 8;
      out[j+2] = t; t >>= 8;
      out[j+1] = t; t >>= 8;
      out[j  ] = t;
   }

   return res;
}

// SHA256SumValue::SHA256SumValue - Constructs the sum from a string   /*{{{*/
// ---------------------------------------------------------------------
/* The string form of a SHA256 is a 64 character hex number */
SHA256SumValue::SHA256SumValue(string Str)
{
   memset(Sum,0,sizeof(Sum));
   Set(Str);
}

                                                                       /*}}}*/
// SHA256SumValue::SHA256SumValue - Default constructor                /*{{{*/
// ---------------------------------------------------------------------
/* Sets the value to 0 */
SHA256SumValue::SHA256SumValue()
{
   memset(Sum,0,sizeof(Sum));
}

                                                                       /*}}}*/
// SHA256SumValue::Set - Set the sum from a string                     /*{{{*/
// ---------------------------------------------------------------------
/* Converts the hex string into a set of chars */
bool SHA256SumValue::Set(string Str)
{
   return Hex2Num(Str,Sum,sizeof(Sum));
}
                                                                       /*}}}*/
// SHA256SumValue::Value - Convert the number into a string            /*{{{*/
// ---------------------------------------------------------------------
/* Converts the set of chars into a hex string in lower case */
string SHA256SumValue::Value() const
{
   char Conv[16] =
      { '0','1','2','3','4','5','6','7','8','9','a','b',
      'c','d','e','f'
   };
   char Result[65];
   Result[64] = 0;

   // Convert each char into two letters
   int J = 0;
   int I = 0;
   for (; I != 64; J++,I += 2)
   {
      Result[I] = Conv[Sum[J] >> 4];
      Result[I + 1] = Conv[Sum[J] & 0xF];
   }

   return string(Result);
}



// SHA256SumValue::operator == - Comparator                            /*{{{*/
// ---------------------------------------------------------------------
/* Call memcmp on the buffer */
bool SHA256SumValue::operator == (const SHA256SumValue & rhs) const
{
   return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
}
                                                                       /*}}}*/


// SHA256Summation::AddFD - Add content of file into the checksum      /*{{{*/
// ---------------------------------------------------------------------
/* */
bool SHA256Summation::AddFD(int Fd,unsigned long Size)
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

