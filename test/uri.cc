#include <apt-pkg/strutl.h>
#include <stdio.h>

void Test(const char *Foo)
{
   URI U(Foo);
   
   printf("%s a='%s' u='%s' p='%s' port='%u'\n   h='%s' p='%s'\n",
	  Foo,U.Access.c_str(),U.User.c_str(),U.Password.c_str(),
	  U.Port,U.Host.c_str(),U.Path.c_str());
}

int main()
{
   // Basic stuff
   Test("http://www.debian.org:90/temp/test");
   Test("http://jgg:foo@ualberta.ca/blah");
   Test("file:/usr/bin/foo");
   Test("cdrom:Moo Cow Rom:/debian");
   Test("gzip:./bar/cow");
	   
   // RFC 2732 stuff
   Test("http://[1080::8:800:200C:417A]/foo");
   Test("http://[::FFFF:129.144.52.38]:80/index.html");
   Test("http://[::FFFF:129.144.52.38:]:80/index.html");
   Test("http://[::FFFF:129.144.52.38:]/index.html");
   
   /* My Evil Corruption of RFC 2732 to handle CDROM names! Fun for 
      the whole family! */
   Test("cdrom:[The Debian 1.2 disk, 1/2 R1:6]/debian/");
   Test("cdrom:Foo Bar Cow/debian/");
      
   Test("ftp:ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb");
}
