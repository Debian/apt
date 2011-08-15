#include <apt-pkg/strutl.h>

#include "assert.h"

int main() {
	// Basic stuff
	{
	URI U("http://www.debian.org:90/temp/test");
	equals("http", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(90, U.Port);
	equals("www.debian.org", U.Host);
	equals("/temp/test", U.Path);
	} {
	URI U("http://jgg:foo@ualberta.ca/blah");
	equals("http", U.Access);
	equals("jgg", U.User);
	equals("foo", U.Password);
	equals(0, U.Port);
	equals("ualberta.ca", U.Host);
	equals("/blah", U.Path);
	} {
	URI U("file:/usr/bin/foo");
	equals("file", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("", U.Host);
	equals("/usr/bin/foo", U.Path);
	} {
	URI U("cdrom:Moo Cow Rom:/debian");
	equals("cdrom", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("Moo Cow Rom", U.Host);
	equals("/debian", U.Path);
	} {
	URI U("gzip:./bar/cow");
	equals("gzip", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals(".", U.Host);
	equals("/bar/cow", U.Path);
	} {
	URI U("ftp:ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb");
	equals("ftp", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("ftp.fr.debian.org", U.Host);
	equals("/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb", U.Path);
	}

	// RFC 2732 stuff
	{
	URI U("http://[1080::8:800:200C:417A]/foo");
	equals("http", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("1080::8:800:200C:417A", U.Host);
	equals("/foo", U.Path);
	} {
	URI U("http://[::FFFF:129.144.52.38]:80/index.html");
	equals("http", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(80, U.Port);
	equals("::FFFF:129.144.52.38", U.Host);
	equals("/index.html", U.Path);
	} {
	URI U("http://[::FFFF:129.144.52.38:]:80/index.html");
	equals("http", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(80, U.Port);
	equals("::FFFF:129.144.52.38:", U.Host);
	equals("/index.html", U.Path);
	} {
	URI U("http://[::FFFF:129.144.52.38:]/index.html");
	equals("http", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("::FFFF:129.144.52.38:", U.Host);
	equals("/index.html", U.Path);
	}
	/* My Evil Corruption of RFC 2732 to handle CDROM names! Fun for
	   the whole family! */
	{
	URI U("cdrom:[The Debian 1.2 disk, 1/2 R1:6]/debian/");
	equals("cdrom", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("The Debian 1.2 disk, 1/2 R1:6", U.Host);
	equals("/debian/", U.Path);
	} {
	URI U("cdrom:Foo Bar Cow/debian/");
	equals("cdrom", U.Access);
	equals("", U.User);
	equals("", U.Password);
	equals(0, U.Port);
	equals("Foo Bar Cow", U.Host);
	equals("/debian/", U.Path);
	}

	return 0;
}
