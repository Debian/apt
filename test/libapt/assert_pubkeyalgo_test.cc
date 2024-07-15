#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/gpgv.h>

#include "common.h"

TEST(AssertPubKeyAlgo_Test, test)
{
   EXPECT_TRUE(IsAssertedPubKeyAlgo("rsa2048", ">=rsa2048"));
   _error->DumpErrors();
   EXPECT_TRUE(_error->empty());

   EXPECT_TRUE(IsAssertedPubKeyAlgo("rsa2048", "another,>=rsa2048"));
   EXPECT_TRUE(_error->empty());

   EXPECT_FALSE(IsAssertedPubKeyAlgo("rsa2048", ">=rsa2049"));
   EXPECT_TRUE(_error->empty());

   EXPECT_TRUE(IsAssertedPubKeyAlgo("ed25519", ">=rsa2048,ed25519"));
   EXPECT_TRUE(_error->empty());
}

TEST(AssertPubKeyAlgo_Test, CanOnlyCompareRSA)
{
   std::string msg;
   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", ">=ed25519"));
   EXPECT_TRUE(_error->PopMessage(msg));
   EXPECT_EQ("Unrecognized public key specification '>=ed25519' in option >=ed25519", msg);
   EXPECT_TRUE(_error->empty());
}

TEST(AssertPubKeyAlgo_Test, EmptyOption)
{
   std::string msg;
   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", ""));
   EXPECT_TRUE(_error->empty());

   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", ","));
   EXPECT_TRUE(_error->PopMessage(msg));
   EXPECT_EQ("Empty item in public key assertion string option ,", msg);
   EXPECT_TRUE(_error->empty());

   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", "moo,"));
   EXPECT_TRUE(_error->empty());

   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", "moo,,"));
   EXPECT_TRUE(_error->PopMessage(msg));
   EXPECT_EQ("Empty item in public key assertion string option moo,,", msg);
   EXPECT_TRUE(_error->empty());

   EXPECT_FALSE(IsAssertedPubKeyAlgo("ed25519", ",moo"));
   EXPECT_TRUE(_error->PopMessage(msg));
   EXPECT_EQ("Empty item in public key assertion string option ,moo", msg);
   EXPECT_TRUE(_error->empty());
}
