#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/netrc.h>
#include <apt-pkg/strutl.h>

#include <string>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(NetRCTest, Parsing)
{
   FileFd fd;
   URI U("http://file.not/open");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());
   EXPECT_EQ("file.not", U.Host);
   EXPECT_EQ("/open", U.Path);

   createTemporaryFile("doublesignedfile", fd, nullptr, R"apt(
machine example.netter login bar password foo
machine example.net login foo password bar

machine example.org:90 login apt password apt
machine	example.org:8080
login
example	password 	 foobar

machine example.org
login anonymous
password pass

machine example.com/foo login user1 unknown token password pass1
machine example.com/bar password pass2 login user2
		  unknown token
machine example.com/user login user
machine example.netter login unused password firstentry
machine example.last/debian login debian password rules)apt");
   U = URI("http://example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
   EXPECT_EQ("example.net", U.Host);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://user:pass@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user", U.User);
   EXPECT_EQ("pass", U.Password);
   EXPECT_EQ("example.net", U.Host);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.org:90/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("apt", U.User);
   EXPECT_EQ("apt", U.Password);
   EXPECT_EQ("example.org", U.Host);
   EXPECT_EQ(90, U.Port);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.org:8080/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("example", U.User);
   EXPECT_EQ("foobar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.net:42/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("anonymous", U.User);
   EXPECT_EQ("pass", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/apt");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user1", U.User);
   EXPECT_EQ("pass1", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/fooo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user1", U.User);
   EXPECT_EQ("pass1", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/fo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/bar");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user2", U.User);
   EXPECT_EQ("pass2", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.com/user");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user", U.User);
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("socks5h://example.last/debian");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("debian", U.User);
   EXPECT_EQ("rules", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("socks5h://example.debian/");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("socks5h://user:pass@example.debian/");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user", U.User);
   EXPECT_EQ("pass", U.Password);
}
TEST(NetRCTest, BadFileNoMachine)
{
   FileFd fd;
   createTemporaryFile("doublesignedfile", fd, nullptr, R"apt(
foo example.org login foo1 password bar
machin example.org login foo2 password bar
machine2 example.org login foo3 password bar
)apt");

   URI U("http://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());
}
TEST(NetRCTest, BadFileEndsMachine)
{
   FileFd fd;
   createTemporaryFile("doublesignedfile", fd, nullptr, R"apt(
machine example.org login foo1 password bar
machine)apt");

   URI U("http://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}
TEST(NetRCTest, BadFileEndsLogin)
{
   FileFd fd;
   createTemporaryFile("doublesignedfile", fd, nullptr, R"apt(
machine example.org login foo1 password bar
machine example.net login)apt");

   URI U("http://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}
TEST(NetRCTest, BadFileEndsPassword)
{
   FileFd fd;
   createTemporaryFile("doublesignedfile", fd, nullptr, R"apt(
machine example.org login foo1 password bar
machine example.net password)apt");

   URI U("http://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}
