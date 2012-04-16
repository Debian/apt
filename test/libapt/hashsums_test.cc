#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>
#include <iostream>

#include <stdio.h>

#include "assert.h"

template <class T> void Test(const char *In,const char *Out)
{
   T Sum;
   Sum.Add(In);
   equals(Sum.Result().Value(), Out);
}

template <class T> void TestMill(const char *Out)
{
   T Sum;

   const unsigned char As[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
   unsigned Count = 1000000;
   for (; Count != 0;)
   {
      if (Count >= 64)
      {
	 Sum.Add(As,64);
	 Count -= 64;
      }
      else
      {
	 Sum.Add(As,Count);
	 Count = 0;
      }
   }

   if (stringcasecmp(Sum.Result().Value(), Out) != 0)
      abort();
}

int main(int argc, char** argv)
{
   // From  FIPS PUB 180-1
   Test<SHA1Summation>("","da39a3ee5e6b4b0d3255bfef95601890afd80709");
   Test<SHA1Summation>("abc","a9993e364706816aba3e25717850c26c9cd0d89d");
   Test<SHA1Summation>("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		       "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
   TestMill<SHA1Summation>("34aa973cd4c4daa4f61eeb2bdbad27316534016f");

   // MD5 tests from RFC 1321
   Test<MD5Summation>("","d41d8cd98f00b204e9800998ecf8427e");
   Test<MD5Summation>("a","0cc175b9c0f1b6a831c399e269772661");
   Test<MD5Summation>("abc","900150983cd24fb0d6963f7d28e17f72");
   Test<MD5Summation>("message digest","f96b697d7cb7938d525a2f31aaf161d0");
   Test<MD5Summation>("abcdefghijklmnopqrstuvwxyz","c3fcd3d76192e4007dfb496cca67e13b");
   Test<MD5Summation>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		      "d174ab98d277d9f5a5611c2c9f419d9f");
   Test<MD5Summation>("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		      "57edf4a22be3c955ac49da2e2107b67a");

   // SHA-256, From FIPS 180-2
   Test<SHA256Summation>("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
   Test<SHA256Summation>("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
			 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

   // SHA-512
   Test<SHA512Summation>("",
	"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
	"47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
   Test<SHA512Summation>(
      "abc",
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");


   Test<MD5Summation>("The quick brown fox jumps over the lazy dog", "9e107d9d372bb6826bd81d3542a419d6");
   Test<MD5Summation>("The quick brown fox jumps over the lazy dog.", "e4d909c290d0fb1ca068ffaddf22cbd0");
   Test<SHA1Summation>("The quick brown fox jumps over the lazy dog", "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
   Test<SHA1Summation>("The quick brown fox jumps over the lazy cog", "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3");
   Test<SHA256Summation>("The quick brown fox jumps over the lazy dog", "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
   Test<SHA256Summation>("The quick brown fox jumps over the lazy dog.", "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c");
   Test<SHA512Summation>("The quick brown fox jumps over the lazy dog", "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb64"
									"2e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6");
   Test<SHA512Summation>("The quick brown fox jumps over the lazy dog.", "91ea1245f20d46ae9a037a989f54f1f790f0a47607eeb8a14d12890cea77a1bb"
									 "c6c7ed9cf205e67b7f2b8fd4c7dfd3a7a8617e45f3c463d481c7e586c39ac1ed");

   FILE* fd = fopen(argv[1], "r");
   if (fd == NULL) {
      std::cerr << "Can't open file for 1. testing: " << argv[1] << std::endl;
      return 1;
   }
   {
   Hashes hashes;
   hashes.AddFD(fileno(fd));
   equals(argv[2], hashes.MD5.Result().Value());
   equals(argv[3], hashes.SHA1.Result().Value());
   equals(argv[4], hashes.SHA256.Result().Value());
   equals(argv[5], hashes.SHA512.Result().Value());
   }
   fseek(fd, 0L, SEEK_END);
   unsigned long sz = ftell(fd);
   fseek(fd, 0L, SEEK_SET);
   {
   Hashes hashes;
   hashes.AddFD(fileno(fd), sz);
   equals(argv[2], hashes.MD5.Result().Value());
   equals(argv[3], hashes.SHA1.Result().Value());
   equals(argv[4], hashes.SHA256.Result().Value());
   equals(argv[5], hashes.SHA512.Result().Value());
   }
   fseek(fd, 0L, SEEK_SET);
   {
   MD5Summation md5;
   md5.AddFD(fileno(fd));
   equals(argv[2], md5.Result().Value());
   }
   fseek(fd, 0L, SEEK_SET);
   {
   SHA1Summation sha1;
   sha1.AddFD(fileno(fd));
   equals(argv[3], sha1.Result().Value());
   }
   fseek(fd, 0L, SEEK_SET);
   {
   SHA256Summation sha2;
   sha2.AddFD(fileno(fd));
   equals(argv[4], sha2.Result().Value());
   }
   fseek(fd, 0L, SEEK_SET);
   {
   SHA512Summation sha2;
   sha2.AddFD(fileno(fd));
   equals(argv[5], sha2.Result().Value());
   }
   fclose(fd);

   // test HashString code
   {
   HashString sha2("SHA256", argv[4]);
   equals(sha2.VerifyFile(argv[1]), true);
   }
   {
   HashString sha2("SHA512", argv[5]);
   equals(sha2.VerifyFile(argv[1]), true);
   }
   {
   HashString sha2("SHA256:" + std::string(argv[4]));
   equals(sha2.VerifyFile(argv[1]), true);
   }

   return 0;
}


