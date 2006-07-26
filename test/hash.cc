#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha256.h>
#include <apt-pkg/strutl.h>
#include <iostream>

using namespace std;

template <class T> void Test(const char *In,const char *Out)
{
   T Sum;
   Sum.Add(In);
   cout << Sum.Result().Value() << endl;
   if (stringcasecmp(Sum.Result().Value(),Out) != 0)
      abort();
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
   
   cout << Sum.Result().Value() << endl;
   if (stringcasecmp(Sum.Result().Value(),Out) != 0)
      abort();
}

int main()
{
   // From  FIPS PUB 180-1
   Test<SHA1Summation>("abc","A9993E364706816ABA3E25717850C26C9CD0D89D");
   Test<SHA1Summation>("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		       "84983E441C3BD26EBAAE4AA1F95129E5E54670F1");
   TestMill<SHA1Summation>("34AA973CD4C4DAA4F61EEB2BDBAD27316534016F");
   
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
   Test<SHA256Summation>("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 
			 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
   

   return 0; 
}

	 
