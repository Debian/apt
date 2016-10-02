#include <config.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/proxy.h>
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
   EXPECT_EQ("http://www.debian.org:90/temp/test", (std::string)U);
   EXPECT_EQ("http://www.debian.org:90", URI::SiteOnly(U));
   EXPECT_EQ("http://www.debian.org:90/temp/test", URI::ArchiveOnly(U));
   EXPECT_EQ("http://www.debian.org:90/temp/test", URI::NoUserPassword(U));
   // Login data
   U = URI("http://jgg:foo@ualberta.ca/blah");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("jgg", U.User);
   EXPECT_EQ("foo", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("ualberta.ca", U.Host);
   EXPECT_EQ("/blah", U.Path);
   EXPECT_EQ("http://jgg:foo@ualberta.ca/blah", (std::string)U);
   EXPECT_EQ("http://ualberta.ca", URI::SiteOnly(U));
   EXPECT_EQ("http://ualberta.ca/blah", URI::ArchiveOnly(U));
   EXPECT_EQ("http://ualberta.ca/blah", URI::NoUserPassword(U));
   // just a user
   U = URI("https://apt@example.org/blah");
   EXPECT_EQ("https", U.Access);
   EXPECT_EQ("apt", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("example.org", U.Host);
   EXPECT_EQ("/blah", U.Path);
   EXPECT_EQ("https://apt@example.org/blah", (std::string)U);
   EXPECT_EQ("https://example.org", URI::SiteOnly(U));
   EXPECT_EQ("https://example.org/blah", URI::ArchiveOnly(U));
   EXPECT_EQ("https://example.org/blah", URI::NoUserPassword(U));
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
   EXPECT_EQ("file:/usr/bin/foo", (std::string)U);
   EXPECT_EQ("file:", URI::SiteOnly(U));
   EXPECT_EQ("file:/usr/bin/foo", URI::ArchiveOnly(U));
   EXPECT_EQ("file:/usr/bin/foo", URI::NoUserPassword(U));
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
   EXPECT_EQ("cdrom://Moo Cow Rom/debian", (std::string)U);
   EXPECT_EQ("cdrom://Moo Cow Rom", URI::SiteOnly(U));
   EXPECT_EQ("cdrom://Moo Cow Rom/debian", URI::ArchiveOnly(U));
   EXPECT_EQ("cdrom://Moo Cow Rom/debian", URI::NoUserPassword(U));
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
   EXPECT_EQ("gzip://./bar/cow", (std::string)U);
   EXPECT_EQ("gzip://.", URI::SiteOnly(U));
   EXPECT_EQ("gzip://./bar/cow", URI::ArchiveOnly(U));
   EXPECT_EQ("gzip://./bar/cow", URI::NoUserPassword(U));
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
   EXPECT_EQ("ftp://ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb", (std::string)U);
   EXPECT_EQ("ftp://ftp.fr.debian.org", URI::SiteOnly(U));
   EXPECT_EQ("ftp://ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb", URI::ArchiveOnly(U));
   EXPECT_EQ("ftp://ftp.fr.debian.org/debian/pool/main/x/xtel/xtel_3.2.1-15_i386.deb", URI::NoUserPassword(U));
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
   EXPECT_EQ("http://[1080::8:800:200C:417A]/foo", (std::string)U);
   EXPECT_EQ("http://[1080::8:800:200C:417A]", URI::SiteOnly(U));
   EXPECT_EQ("http://[1080::8:800:200C:417A]/foo", URI::ArchiveOnly(U));
   EXPECT_EQ("http://[1080::8:800:200C:417A]/foo", URI::NoUserPassword(U));
   // with port
   U = URI("http://[::FFFF:129.144.52.38]:80/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(80, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   EXPECT_EQ("http://[::FFFF:129.144.52.38]:80/index.html", (std::string)U);
   EXPECT_EQ("http://[::FFFF:129.144.52.38]:80", URI::SiteOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38]:80/index.html", URI::ArchiveOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38]:80/index.html", URI::NoUserPassword(U));
   // extra colon
   U = URI("http://[::FFFF:129.144.52.38:]:80/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(80, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38:", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]:80/index.html", (std::string)U);
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]:80", URI::SiteOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]:80/index.html", URI::ArchiveOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]:80/index.html", URI::NoUserPassword(U));
   // extra colon port
   U = URI("http://[::FFFF:129.144.52.38:]/index.html");
   EXPECT_EQ("http", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("::FFFF:129.144.52.38:", U.Host);
   EXPECT_EQ("/index.html", U.Path);
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]/index.html", (std::string)U);
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]", URI::SiteOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]/index.html", URI::ArchiveOnly(U));
   EXPECT_EQ("http://[::FFFF:129.144.52.38:]/index.html", URI::NoUserPassword(U));
   // My Evil Corruption of RFC 2732 to handle CDROM names!
   // Fun for the whole family! */
   U = URI("cdrom:[The Debian 1.2 disk, 1/2 R1:6]/debian/");
   EXPECT_EQ("cdrom", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("The Debian 1.2 disk, 1/2 R1:6", U.Host);
   EXPECT_EQ("/debian/", U.Path);
   EXPECT_EQ("cdrom://[The Debian 1.2 disk, 1/2 R1:6]/debian/", (std::string)U);
   EXPECT_EQ("cdrom://[The Debian 1.2 disk, 1/2 R1:6]", URI::SiteOnly(U));
   EXPECT_EQ("cdrom://[The Debian 1.2 disk, 1/2 R1:6]/debian", URI::ArchiveOnly(U));
   EXPECT_EQ("cdrom://[The Debian 1.2 disk, 1/2 R1:6]/debian/", URI::NoUserPassword(U));
   // no brackets
   U = URI("cdrom:Foo Bar Cow/debian/");
   EXPECT_EQ("cdrom", U.Access);
   EXPECT_EQ("", U.User);
   EXPECT_EQ("", U.Password);
   EXPECT_EQ(0, U.Port);
   EXPECT_EQ("Foo Bar Cow", U.Host);
   EXPECT_EQ("/debian/", U.Path);
   EXPECT_EQ("cdrom://Foo Bar Cow/debian/", (std::string)U);
   EXPECT_EQ("cdrom://Foo Bar Cow", URI::SiteOnly(U));
   EXPECT_EQ("cdrom://Foo Bar Cow/debian", URI::ArchiveOnly(U));
   EXPECT_EQ("cdrom://Foo Bar Cow/debian/", URI::NoUserPassword(U));
   // percent encoded password
   U = URI("ftp://foo:b%40r@example.org");
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("b@r", U.Password);
   EXPECT_EQ("ftp://foo:b%40r@example.org/", (std::string) U);
   EXPECT_EQ("ftp://example.org", URI::SiteOnly(U));
   EXPECT_EQ("ftp://example.org", URI::ArchiveOnly(U));
   EXPECT_EQ("ftp://example.org/", URI::NoUserPassword(U));
   // percent encoded user
   U = URI("ftp://f%40o:bar@example.org");
   EXPECT_EQ("f@o", U.User);
   EXPECT_EQ("bar", U.Password);
   EXPECT_EQ("ftp://f%40o:bar@example.org/", (std::string) U);
   EXPECT_EQ("ftp://example.org", URI::SiteOnly(U));
   EXPECT_EQ("ftp://example.org", URI::ArchiveOnly(U));
   EXPECT_EQ("ftp://example.org/", URI::NoUserPassword(U));
}
TEST(URITest, AutoProxyTest)
{
   URI u0("http://www.debian.org:90/temp/test");
   URI u1("http://www.debian.org:91/temp/test");

   _config->Set("Acquire::http::Proxy-Auto-Detect", "./apt-proxy-script");

   // Scenario 0: Autodetecting a simple proxy
   AutoDetectProxy(u0);
   EXPECT_EQ(_config->Find("Acquire::http::proxy::www.debian.org", ""), "http://example.com");

   // Scenario 1: Proxy stays the same if it is already set
   AutoDetectProxy(u1);
   EXPECT_EQ(_config->Find("Acquire::http::proxy::www.debian.org", ""), "http://example.com");

   // Scenario 2: Reading with stderr output works fine
   _config->Clear("Acquire::http::proxy::www.debian.org");
   AutoDetectProxy(u1);
   EXPECT_EQ(_config->Find("Acquire::http::proxy::www.debian.org", ""), "http://example.com/foo");

   // Scenario 1 again: Proxy stays the same if it is already set
   AutoDetectProxy(u0);
   EXPECT_EQ(_config->Find("Acquire::http::proxy::www.debian.org", ""), "http://example.com/foo");
}
