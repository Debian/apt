// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version Test - Simple program to run through a file and comare versions.
   
   Each version is compared and the result is checked against an expected
   result in the file. The format of the file is
       a b Res
   Where Res is -1, 1, 0. dpkg -D=1 --compare-versions a "<" b can be
   used to determine what Res should be. # at the start of the line
   is a comment and blank lines are skipped

   The runner will also call dpkg --compare-versions to check if APT and
   dpkg have (still) the same idea.
   
   ##################################################################### */
									/*}}}*/
#include <config.h>

#include <apt-pkg/debversion.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <fstream>
#include <string>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

using namespace std;

static bool callDPKG(const char * const val, const char * const ref, char const * const op) {
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char * args[] = {
	 "dpkg",
	 "--compare-versions",
	 val,
	 op,
	 ref,
	 nullptr
      };
      execvp(args[0], (char**) args);
      exit(1);
   }
   int Ret;
   waitpid(Process, &Ret, 0);
   EXPECT_TRUE(WIFEXITED(Ret));
   return WEXITSTATUS(Ret) == 0;
}


#define EXPECT_VERSION_PART(A, compare, B) \
{ \
   int Res = debVS.CmpVersion(A, B); \
   Res = (Res < 0) ? -1 : ( (Res > 0) ? 1 : Res); \
   EXPECT_EQ(compare, Res) << "APT: A: »" << A << "« B: »" << B << "«"; \
   EXPECT_PRED3(callDPKG, A, B,  ((compare == 1) ? ">>" : ( (compare == 0) ? "=" : "<<"))); \
}
#define EXPECT_VERSION(A, compare, B) \
   EXPECT_VERSION_PART(A, compare, B); \
   EXPECT_VERSION_PART(B, compare * -1, A)

// History-Remark: The versions used to be specified in a versions.lst file

enum CompareVersionType { LESS = -1, GREATER = 1, EQUAL = 0 };

TEST(CompareVersionTest,Basic)
{
   EXPECT_VERSION("7.6p2-4", GREATER, "7.6-0");
   EXPECT_VERSION("1.0.3-3", GREATER, "1.0-1");
   EXPECT_VERSION("1.3", GREATER, "1.2.2-2");
   EXPECT_VERSION("1.3", GREATER, "1.2.2");

   /* disabled as dpkg doesn't like them… (versions have to start with a number)
   EXPECT_VERSION("-", LESS, ".");
   EXPECT_VERSION("p", LESS, "-");
   EXPECT_VERSION("a", LESS, "-");
   EXPECT_VERSION("z", LESS, "-");
   EXPECT_VERSION("a", LESS, ".");
   EXPECT_VERSION("z", LESS, ".");
   // */

   /* disabled as dpkg doesn't like them… (versions have to start with a number)
   EXPECT_VERSION("III-alpha9.8", LESS, "III-alpha9.8-1.5");
   // */

   // Test some properties of text strings
   EXPECT_VERSION("0-pre", EQUAL, "0-pre");
   EXPECT_VERSION("0-pre", LESS, "0-pree");

   EXPECT_VERSION("1.1.6r2-2", GREATER, "1.1.6r-1");
   EXPECT_VERSION("2.6b2-1", GREATER, "2.6b-2");

   EXPECT_VERSION("98.1p5-1", LESS, "98.1-pre2-b6-2");
   EXPECT_VERSION("0.4a6-2", GREATER, "0.4-1");

   EXPECT_VERSION("1:3.0.5-2", LESS, "1:3.0.5.1");
}
TEST(CompareVersionTest,Epochs)
{
   EXPECT_VERSION("1:0.4", GREATER, "10.3");
   EXPECT_VERSION("1:1.25-4", LESS, "1:1.25-8");
   EXPECT_VERSION("0:1.18.36", EQUAL, "1.18.36");

   EXPECT_VERSION("1.18.36", GREATER, "1.18.35");
   EXPECT_VERSION("0:1.18.36", GREATER, "1.18.35");
}
TEST(CompareVersionTest,Strangeness)
{
   // Funky, but allowed, characters in upstream version
   EXPECT_VERSION("9:1.18.36:5.4-20", LESS, "10:0.5.1-22");
   EXPECT_VERSION("9:1.18.36:5.4-20", LESS, "9:1.18.36:5.5-1");
   EXPECT_VERSION("9:1.18.36:5.4-20", LESS, " 9:1.18.37:4.3-22");
   EXPECT_VERSION("1.18.36-0.17.35-18", GREATER, "1.18.36-19");

   // Junk
   EXPECT_VERSION("1:1.2.13-3", LESS, "1:1.2.13-3.1");
   EXPECT_VERSION("2.0.7pre1-4", LESS, "2.0.7r-1");

   // if a version includes a dash, it should be the debrev dash - policy says so…
   EXPECT_VERSION("0:0-0-0", GREATER, "0-0");

   // do we like strange versions? Yes we like strange versions…
   EXPECT_VERSION("0", EQUAL, "0");
   EXPECT_VERSION("0", EQUAL, "00");
}
TEST(CompareVersionTest,DebianBug)
{
   // #205960
   EXPECT_VERSION("3.0~rc1-1", LESS, "3.0-1");
   // #573592 - debian policy 5.6.12
   EXPECT_VERSION("1.0", EQUAL, "1.0-0");
   EXPECT_VERSION("0.2", LESS, "1.0-0");
   EXPECT_VERSION("1.0", LESS, "1.0-0+b1");
   EXPECT_VERSION("1.0", GREATER, "1.0-0~");
}
TEST(CompareVersionTest,CuptTests)
{
   // "steal" the testcases from (old perl) cupt
   EXPECT_VERSION("1.2.3", EQUAL, "1.2.3"); // identical
   EXPECT_VERSION("4.4.3-2", EQUAL, "4.4.3-2"); // identical
   EXPECT_VERSION("1:2ab:5", EQUAL, "1:2ab:5"); // this is correct...
   EXPECT_VERSION("7:1-a:b-5", EQUAL, "7:1-a:b-5"); // and this
   EXPECT_VERSION("57:1.2.3abYZ+~-4-5", EQUAL, "57:1.2.3abYZ+~-4-5"); // and those too
   EXPECT_VERSION("1.2.3", EQUAL, "0:1.2.3"); // zero epoch
   EXPECT_VERSION("1.2.3", EQUAL, "1.2.3-0"); // zero revision
   EXPECT_VERSION("009", EQUAL, "9"); // zeroes…
   EXPECT_VERSION("009ab5", EQUAL, "9ab5"); // there as well
   EXPECT_VERSION("1.2.3", LESS, "1.2.3-1"); // added non-zero revision
   EXPECT_VERSION("1.2.3", LESS, "1.2.4"); // just bigger
   EXPECT_VERSION("1.2.4", GREATER, "1.2.3"); // order doesn't matter
   EXPECT_VERSION("1.2.24", GREATER, "1.2.3"); // bigger, eh?
   EXPECT_VERSION("0.10.0", GREATER, "0.8.7"); // bigger, eh?
   EXPECT_VERSION("3.2", GREATER, "2.3"); // major number rocks
   EXPECT_VERSION("1.3.2a", GREATER, "1.3.2"); // letters rock
   EXPECT_VERSION("0.5.0~git", LESS, "0.5.0~git2"); // numbers rock
   EXPECT_VERSION("2a", LESS, "21"); // but not in all places
   EXPECT_VERSION("1.3.2a", LESS, "1.3.2b"); // but there is another letter
   EXPECT_VERSION("1:1.2.3", GREATER, "1.2.4"); // epoch rocks
   EXPECT_VERSION("1:1.2.3", LESS, "1:1.2.4"); // bigger anyway
   EXPECT_VERSION("1.2a+~bCd3", LESS, "1.2a++"); // tilde doesn't rock
   EXPECT_VERSION("1.2a+~bCd3", GREATER, "1.2a+~"); // but first is longer!
   EXPECT_VERSION("5:2", GREATER, "304-2"); // epoch rocks
   EXPECT_VERSION("5:2", LESS, "304:2"); // so big epoch?
   EXPECT_VERSION("25:2", GREATER, "3:2"); // 25 > 3, obviously
   EXPECT_VERSION("1:2:123", LESS, "1:12:3"); // 12 > 2
   EXPECT_VERSION("1.2-5", LESS, "1.2-3-5"); // 1.2 < 1.2-3
   EXPECT_VERSION("5.10.0", GREATER, "5.005"); // preceding zeroes don't matters
   EXPECT_VERSION("3a9.8", LESS, "3.10.2"); // letters are before all letter symbols
   EXPECT_VERSION("3a9.8", GREATER, "3~10"); // but after the tilde
   EXPECT_VERSION("1.4+OOo3.0.0~", LESS, "1.4+OOo3.0.0-4"); // another tilde check
   EXPECT_VERSION("2.4.7-1", LESS, "2.4.7-z"); // revision comparing
   EXPECT_VERSION("1.002-1+b2", GREATER, "1.00"); // whatever...
   /* disabled as dpkg doesn't like them… (versions with illegal char)
   EXPECT_VERSION("2.2.4-47978_Debian_lenny", EQUAL, "2.2.4-47978_Debian_lenny"); // and underscore...
   // */
}
