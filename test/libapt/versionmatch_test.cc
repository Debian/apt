#include <config.h>

#include <apt-pkg/versionmatch.h>

#include <gtest/gtest.h>

using MT = pkgVersionMatch::MatchType;
using DT = pkgVersionMatch::DataType;

TEST(VersionMatchTest,ReleaseVersion)
{
   pkgVersionMatch vm("9", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("9", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleaseVersionStar)
{
   pkgVersionMatch vm("9*", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("9", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleaseString)
{
   pkgVersionMatch vm("rocksolid", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("rocksolid", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleaseStringStar)
{
   pkgVersionMatch vm("rocksolid*", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("rocksolid*", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleasePolicy)
{
   pkgVersionMatch vm("o=Debian,a=unstable,n=sid,l=DebianLabel,c=main,b=amd64", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("Debian", vm.GetMatchData(MT::Release, DT::ORIGIN));
   EXPECT_EQ("unstable", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("sid", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("DebianLabel", vm.GetMatchData(MT::Release, DT::LABEL));
   EXPECT_EQ("main", vm.GetMatchData(MT::Release, DT::COMPONENT));
   EXPECT_EQ("amd64", vm.GetMatchData(MT::Release, DT::ARCHITECTURE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleasePolicySpaces)
{
   pkgVersionMatch vm("  o=Debian, a=unstable  ,\tn=sid ,  , l=DebianLabel\t,c=main\t,\tb=amd64  ", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("Debian", vm.GetMatchData(MT::Release, DT::ORIGIN));
   EXPECT_EQ("unstable", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("sid", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("DebianLabel", vm.GetMatchData(MT::Release, DT::LABEL));
   EXPECT_EQ("main", vm.GetMatchData(MT::Release, DT::COMPONENT));
   EXPECT_EQ("amd64", vm.GetMatchData(MT::Release, DT::ARCHITECTURE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}


/* These examples come from apt_preferences manpage */
TEST(VersionMatchTest,VersionExamplePerl)
{
   pkgVersionMatch vm("5.20*", MT::Version);
   EXPECT_EQ("5.20", vm.GetMatchData(MT::Version, DT::VERSION));
   EXPECT_EQ("", vm.GetMatchData(MT::Version, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,OriginExampleEmpty)
{
   pkgVersionMatch vm("", MT::Origin);
   EXPECT_EQ("", vm.GetMatchData(MT::Origin, DT::ORIGIN));
   EXPECT_EQ("", vm.GetMatchData(MT::Origin, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,OriginExampleDebianOrg)
{
   pkgVersionMatch vm("ftp.de.debian.org", MT::Origin);
   EXPECT_EQ("ftp.de.debian.org", vm.GetMatchData(MT::Origin, DT::ORIGIN));
   EXPECT_EQ("", vm.GetMatchData(MT::Origin, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleaseExampleA)
{
   pkgVersionMatch vm("a=unstable", MT::Release);
   EXPECT_EQ("unstable", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
}
TEST(VersionMatchTest,ReleaseExampleN)
{
   pkgVersionMatch vm("n=buster", MT::Release);
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("buster", vm.GetMatchData(MT::Release, DT::CODENAME));
}
TEST(VersionMatchTest,ReleaseExampleAV)
{
   pkgVersionMatch vm("a=stable,v=9", MT::Release);
   EXPECT_EQ("stable", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("9", vm.GetMatchData(MT::Release, DT::VERSION));
}
TEST(VersionMatchTest,ReleaseExampleAVSpace)
{
   pkgVersionMatch vm("a=stable, v=9", MT::Release);
   EXPECT_EQ("stable", vm.GetMatchData(MT::Release, DT::ARCHIVE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::RELEASE));
   EXPECT_EQ("", vm.GetMatchData(MT::Release, DT::CODENAME));
   EXPECT_EQ("9", vm.GetMatchData(MT::Release, DT::VERSION));
}
