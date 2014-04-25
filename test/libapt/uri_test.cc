#include <config.h>
#include <apt-pkg/strutl.h>
#include <string>
#include <gtest/gtest.h>

TEST(URITest, BasicHTTP)
{
   URI U("http://www.debian.org:90/temp/test");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(90, U.Port);
   EXPECT_EQ("www.debian.org", U.Host);
   EXPECT_EQ("/temp/test", U.Path);
   // Login data
   U = URI("http://jgg:foo@ualberta.ca/blah");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("jgg", U.User);
   EXPECT_EQ("foo", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("ualberta.ca", U.Host);
   EXPECT_EQ("/blah", U.Path);
}
TEST(URITest, SingeSlashFile)
{
   URI U("file:/usr/bin/foo");
   EXPECT_EQ("file", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("", U.Host);
   EXPECT_EQ("/usr/bin/foo", U.Path);
}
TEST(URITest, BasicCDROM)
{
   URI U("cdrom:Moo Cow Rom:/debian");
   EXPECT_EQ("cdrom", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("Moo Cow Rom", U.Host);
   EXPECT_EQ("/debian", U.Path);
}
TEST(URITest, RelativeGzip)
{
   URI U("gzip:./bar/cow");
   EXPECT_EQ("gzip", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ(".", U.Host);
   EXPECT_EQ("/bar/cow", U.Path);
}
TEST(URITest, NoSlashFTP)
{
   URI U("ftp:ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb");
   EXPECT_EQ("ftp", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("ftp.fr.debian.org", U.Host);
   EXPECT_EQ("/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb", U.Path);
}
TEST(URITest, RFC2732)
{
   URI U("http://[1080::8:800:200C:417A]/foo");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("1080::8:800:200C:417A", U.Host);
   EXPECT_EQ("/foo", U.Path);
   // with port
   U = URI("http://[::FFFF:129.144.52.38]:80/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(80, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   // extra colon
   U = URI("http://[::FFFF:129.144.52.38:]:80/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(80, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38:", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   // extra colon port
   U = URI("http://[::FFFF:129.144.52.38:]/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38:", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   // My Evil Corruption of RFC 2732 to handle CDROM names!
   // Fun for the whole family! */
   U = URI("cdrom:[The Debian 1.2 disk, 1/2 R1:6]/debian/");
   EXPECT_EQ("cdrom", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("The Debian 1.2 disk, 1/2 R1:6", U.Host);
   EXPECT_EQ("/debian/", U.Path);
   // no brackets
   U = URI("cdrom:Foo Bar Cow/debian/");
   EXPECT_EQ("cdrom", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("Foo Bar Cow", U.Host);
   EXPECT_EQ("/debian/", U.Path);
   // percent encoded
   U = URI("ftp://foo:b%40r@example.org");
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("b@r", U.Password);
   EXPECT_EQ("ftp://foo:b%40r@example.org/", (std::string) U);
}
