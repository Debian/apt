#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/netrc.h>
#include <apt-pkg/strutl.h>

#include <string>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(NetRCTest, Parsing)
{
   FileFd fd;
   URI U("https://file.not/open");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());
   EXPECT_EQ("file.not", U.Host);
   EXPECT_EQ("/open", U.Path);

   openTemporaryFile("doublesignedfile", fd, R"apt(
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
machine socks5h://example.last/debian login debian password rules)apt");
   U = URI("https://example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
   EXPECT_EQ("example.net", U.Host);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://user:pass@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user", U.User);
   EXPECT_EQ("pass", U.Password);
   EXPECT_EQ("example.net", U.Host);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.org:90/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("apt", U.User);
   EXPECT_EQ("apt", U.Password);
   EXPECT_EQ("example.org", U.Host);
   EXPECT_EQ(90u, U.Port);
   EXPECT_EQ("/foo", U.Path);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.org:8080/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("example", U.User);
   EXPECT_EQ("foobar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.net:42/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("anonymous", U.User);
   EXPECT_EQ("pass", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/apt");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user1", U.User);
   EXPECT_EQ("pass1", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/fooo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user1", U.User);
   EXPECT_EQ("pass1", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/fo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/bar");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("user2", U.User);
   EXPECT_EQ("pass2", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.com/user");
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
   openTemporaryFile("doublesignedfile", fd, R"apt(
foo example.org login foo1 password bar
machin example.org login foo2 password bar
machine2 example.org login foo3 password bar
)apt");

   URI U("https://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());
}
TEST(NetRCTest, BadFileEndsMachine)
{
   FileFd fd;
   openTemporaryFile("doublesignedfile", fd, R"apt(
machine example.org login foo1 password bar
machine)apt");

   URI U("https://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}
TEST(NetRCTest, BadFileEndsLogin)
{
   FileFd fd;
   openTemporaryFile("doublesignedfile", fd, R"apt(
machine example.org login foo1 password bar
machine example.net login)apt");

   URI U("https://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}
TEST(NetRCTest, BadFileEndsPassword)
{
   FileFd fd;
   openTemporaryFile("doublesignedfile", fd, R"apt(
machine example.org login foo1 password bar
machine example.net password)apt");

   URI U("https://example.org/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://example.net/foo");
   EXPECT_FALSE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://foo:bar@example.net/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo", U.User);
   EXPECT_EQ("bar", U.Password);
}

TEST(NetRCTest, MatchesOnlyHTTPS)
{
   FileFd fd;
   openTemporaryFile("doublesignedfile", fd, R"apt(
machine https.example login foo1 password bar
machine http://http.example login foo1 password bar
)apt");

   URI U("https://https.example/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   _error->PushToStack();
   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://https.example/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());
   EXPECT_FALSE(_error->empty());
   EXPECT_TRUE(U.Password.empty());
   _error->RevertToStack();

   EXPECT_TRUE(fd.Seek(0));
   U = URI("http://http.example/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_EQ("foo1", U.User);
   EXPECT_EQ("bar", U.Password);

   EXPECT_TRUE(fd.Seek(0));
   U = URI("https://http.example/foo");
   EXPECT_TRUE(MaybeAddAuth(fd, U));
   EXPECT_TRUE(U.User.empty());
   EXPECT_TRUE(U.Password.empty());

}
