#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>

#include <string>

#include <gtest/gtest.h>

#include "file-helpers.h"

/* The test files are created with the 'Joe Sixpack' and 'Marvin Paranoid'
   test key included in the integration testing framework */

TEST(OpenMaybeClearSignedFileTest,SimpleSignedFile)
{
   std::string tempfile;
   FileFd fd;
   // Using c++11 raw-strings would be nifty, but travis doesn't support it…
   createTemporaryFile("simplesignedfile", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n");

   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,WhitespaceSignedFile)
{
   std::string tempfile;
   FileFd fd;
   // no raw-string here to protect the whitespace from cleanup
   createTemporaryFile("simplesignedfile", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE----- \t    \n"
"Hash:    SHA512     \n"
"	   \n"
"Test	\n"
"-----BEGIN PGP SIGNATURE----- \n"
"   \n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt \n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l	\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg \n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k \n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx \n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns	\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq			\n"
"=TB1F	\n"
"-----END PGP SIGNATURE-----");

   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,SignedFileWithContentHeaders)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("headerssignedfile", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Version: 0.8.15~exp1\n"
"Hash: SHA512\n"
"Comment: I love you!\n"
"X-Expires: never\n"
"Multilines: no\n"
" yes\n"
" maybe\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n");

   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

// That isn't how multiple signatures are done
TEST(OpenMaybeClearSignedFileTest,SignedFileWithTwoSignatures)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("doublesignedfile", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFHBAEBCgAxFiEE3mauypFRr6GHfsMd6FJdR1KBROIFAlhT/yYTHG1hcnZpbkBl\n"
"eGFtcGxlLm9yZwAKCRDoUl1HUoFE4qq3B/459MSk3xCW30wc5+ul5ZxTSg6eLYPJ\n"
"tfVNYi90/ZxRrYQAN+EWozEIZcxoMYp8Ans3++irkjPbHs4NsesmFKt2W5meFl4V\n"
"oUzYrOh5y5GlDeF7ok5g9atQe8BojjBics+g1IBYcnaMU+ywONmlixa03IPGfxV5\n"
"oTx02Xvlns20i6HRc0WFtft5q1hXo4EIlVc9O0u902SVEEkeuHF3+bCcXrNLPBJA\n"
"+8dxmH5+i89f/kVqURrdHdEuA1tsTNyb2C+lvRONh21H8QRRTU/iUQSzV6vZvof5\n"
"ASc9hsAZRG0xHuRU0F94V/XrkWw8QYAobJ/yxvs4L0EuA4optbSqawDB\n"
"=CP8j\n"
"-----END PGP SIGNATURE-----\n");

   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,TwoSimpleSignedFile)
{
   std::string tempfile;
   FileFd fd;
   // read only the first message
   createTemporaryFile("twosimplesignedfile", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n"
"-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----");

   EXPECT_TRUE(_error->empty());
   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_FALSE(_error->empty());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
   ASSERT_FALSE(_error->empty());

   std::string msg;
   _error->PopMessage(msg);
   EXPECT_EQ("Clearsigned file '" + tempfile + "' contains unsigned lines.", msg);
}

TEST(OpenMaybeClearSignedFileTest,UnsignedFile)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("unsignedfile", fd, &tempfile, "Test");

   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,GarbageTop)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("garbagetop", fd, &tempfile, "Garbage\n"
"-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n");

   EXPECT_TRUE(_error->empty());
   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
   ASSERT_FALSE(_error->empty());
   ASSERT_FALSE(_error->PendingError());

   std::string msg;
   _error->PopMessage(msg);
   EXPECT_EQ("Clearsigned file '" + tempfile + "' does not start with a signed message block.", msg);
}

TEST(OpenMaybeClearSignedFileTest,GarbageBottom)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("garbagebottom", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"X/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n"
"Garbage");

   EXPECT_TRUE(_error->empty());
   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
   ASSERT_FALSE(_error->empty());
   ASSERT_FALSE(_error->PendingError());

   std::string msg;
   _error->PopMessage(msg);
   EXPECT_EQ("Clearsigned file '" + tempfile + "' contains unsigned lines.", msg);
}

TEST(OpenMaybeClearSignedFileTest,BogusNoSig)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("bogusnosig", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test");

   EXPECT_TRUE(_error->empty());
   EXPECT_FALSE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_FALSE(_error->empty());
   EXPECT_FALSE(fd.IsOpen());

   std::string msg;
   _error->PopMessage(msg);
   EXPECT_EQ("Splitting of file " + tempfile + " failed as it doesn't contain all expected parts 0 1 0", msg);
}

TEST(OpenMaybeClearSignedFileTest,BogusSigStart)
{
   std::string tempfile;
   FileFd fd;
   createTemporaryFile("bogusnosig", fd, &tempfile, "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----");

   EXPECT_TRUE(_error->empty());
   EXPECT_FALSE(OpenMaybeClearSignedFile(tempfile, fd));
   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
   EXPECT_FALSE(_error->empty());
   EXPECT_FALSE(fd.IsOpen());

   std::string msg;
   _error->PopMessage(msg);
   EXPECT_EQ("Signature in file " + tempfile + " wasn't closed", msg);
}
