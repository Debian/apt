#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>

#include <string>

#include <gtest/gtest.h>

#include "file-helpers.h"

/* The test files are created with the 'Joe Sixpack' and 'Marvin Paranoid'
   test key included in the integration testing framework */

static void EXPECT_SUCCESSFUL_PARSE(std::string const &tempfile)
{
   FileFd fd;
   EXPECT_TRUE(OpenMaybeClearSignedFile(tempfile, fd));
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "Test");
   EXPECT_TRUE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,SimpleSignedFile)
{
   // Using c++11 raw-strings would be nifty, but travis doesn't support it…
   auto const file = createTemporaryFile("simplesignedfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}

TEST(OpenMaybeClearSignedFileTest,WhitespaceSignedFile)
{
   // no raw-string here to protect the whitespace from cleanup
   auto const file = createTemporaryFile("simplesignedfile", "-----BEGIN PGP SIGNED MESSAGE----- \t    \n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}

TEST(OpenMaybeClearSignedFileTest,SignedFileWithContentHeaders)
{
   auto const file = createTemporaryFile("headerssignedfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}

TEST(OpenMaybeClearSignedFileTest,SignedFileWithTwoSignatures)
{
   auto const file = createTemporaryFile("doublesignedfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}


static void EXPECT_FAILED_PARSE(std::string const &tempfile, std::string const &error)
{
   EXPECT_TRUE(_error->empty());
   FileFd fd;
   EXPECT_FALSE(OpenMaybeClearSignedFile(tempfile, fd));
   EXPECT_FALSE(_error->empty());
   EXPECT_FALSE(fd.IsOpen());
   ASSERT_TRUE(_error->PendingError());

   std::string msg;
   EXPECT_TRUE(_error->PopMessage(msg));
   EXPECT_EQ(msg, error);
}

TEST(OpenMaybeClearSignedFileTest,TwoSimpleSignedFile)
{
   // read only the first message
   auto const file = createTemporaryFile("twosimplesignedfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   // technically they are signed, but we just want one message
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' contains unsigned lines.");
}

TEST(OpenMaybeClearSignedFileTest,UnsignedFile)
{
   auto const file = createTemporaryFile("unsignedfile", "Test");
   EXPECT_FALSE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}

TEST(OpenMaybeClearSignedFileTest,GarbageTop)
{
   auto const file = createTemporaryFile("garbagetop", "Garbage\n"
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
   EXPECT_FALSE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' does not start with a signed message block.");
}

TEST(OpenMaybeClearSignedFileTest,GarbageHeader)
{
   auto const file = createTemporaryFile("garbageheader", "-----BEGIN PGP SIGNED MESSAGE----- Garbage\n"
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
   EXPECT_FALSE(StartsWithGPGClearTextSignature(file.Name()));
   // beware: the file will be successfully opened as unsigned file
   FileFd fd;
   EXPECT_TRUE(OpenMaybeClearSignedFile(file.Name(), fd));
   EXPECT_TRUE(fd.IsOpen());
   char buffer[100];
   EXPECT_TRUE(fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(buffer, "-----BEGIN PGP SIGNED MESSAGE----- Garbage\n");
   EXPECT_FALSE(fd.Eof());
}

TEST(OpenMaybeClearSignedFileTest,GarbageBottom)
{
   auto const file = createTemporaryFile("garbagebottom", "-----BEGIN PGP SIGNED MESSAGE-----\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' contains unsigned lines.");
}

TEST(OpenMaybeClearSignedFileTest,BogusNoSig)
{
   auto const file = createTemporaryFile("bogusnosig", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test");
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Splitting of clearsigned file " + file.Name() + " failed as it doesn't contain all expected parts");
}

TEST(OpenMaybeClearSignedFileTest,BogusSigStart)
{
   auto const file = createTemporaryFile("bogusnosig", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----");
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Signature in file " + file.Name() + " wasn't closed");
}

TEST(OpenMaybeClearSignedFileTest,DashedSignedFile)
{
   auto const file = createTemporaryFile("dashedsignedfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"- Test\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_SUCCESSFUL_PARSE(file.Name());
}
TEST(OpenMaybeClearSignedFileTest,StrangeDashArmorFile)
{
   auto const file = createTemporaryFile("strangedashfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"-Hash: SHA512\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' contains unexpected line starting with a dash (armor)");
}
TEST(OpenMaybeClearSignedFileTest,StrangeDashMsgFile)
{
   auto const file = createTemporaryFile("strangedashfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"-Test\n"
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
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' contains unexpected line starting with a dash (msg)");
}
TEST(OpenMaybeClearSignedFileTest,StrangeDashSigFile)
{
   auto const file = createTemporaryFile("strangedashfile", "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA512\n"
"\n"
"Test\n"
"-----BEGIN PGP SIGNATURE-----\n"
"\n"
"iQFEBAEBCgAuFiEENKjp0Y2zIPNn6OqgWpDRQdusja4FAlhT7+kQHGpvZUBleGFt\n"
"cGxlLm9yZwAKCRBakNFB26yNrjvEB/9/e3jA1l0fvPafx9LEXcH8CLpUFQK7ra9l\n"
"3M4YAH4JKQlTG1be7ixruBRlCTh3YiSs66fKMeJeUYoxA2HPhvbGFEjQFAxunEYg\n"
"-/LBKv1mQWa+Q34P5GBjK8kQdLCN+yJAiUErmWNQG3GPninrxsC9tY5jcWvHeP1k\n"
"V7N3MLnNqzXaCJM24mnKidC5IDadUdQ8qC8c3rjUexQ8vBz0eucH56jbqV5oOcvx\n"
"pjlW965dCPIf3OI8q6J7bIOjyY+u/PTcVlqPq3TUz/ti6RkVbKpLH0D4ll3lUTns\n"
"JQt/+gJCPxHUJphy8sccBKhW29CLELJIIafvU30E1nWn9szh2Xjq\n"
"=TB1F\n"
"-----END PGP SIGNATURE-----\n");
   EXPECT_TRUE(StartsWithGPGClearTextSignature(file.Name()));
   EXPECT_FAILED_PARSE(file.Name(), "Clearsigned file '" + file.Name() + "' contains unexpected line starting with a dash (sig)");
}
