#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/history.h>
#include <apt-pkg/indexcopy.h>
#include <apt-pkg/tagfile.h>

#include "common.h"

#include "file-helpers.h"

using namespace APT::History;

TEST(HistoryParseTest, SectionToEntry)
{
   FileFd fd;
   const char *entry_section =
      "Start-Date: 2025-09-01  15:22:56\n"
      "Commandline: apt install rust-coreutils\n"
      "Requested-By: user (1000)\n"
      "Error: An error occurred\n"
      "Comment: This is a comment\n"
      "Install: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "End-Date: 2025-09-01  15:22:57";

   openTemporaryFile("packagesection", fd, entry_section);

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   ASSERT_TRUE(tfile.Step(section));

   Entry entry = ParseSection(section);
   EXPECT_EQ("2025-09-01  15:22:56", entry.startDate);
   EXPECT_EQ("2025-09-01  15:22:57", entry.endDate);
   EXPECT_EQ("apt install rust-coreutils", entry.cmdLine);
   EXPECT_EQ("user (1000)", entry.requestingUser);
   EXPECT_EQ("An error occurred", entry.error);
   EXPECT_EQ("This is a comment", entry.comment);
   EXPECT_EQ("rust-coreutils:amd64", entry.changeMap[Kind::Install][0].package);
   EXPECT_EQ("0.1.0+git20250813.4af2a84-0ubuntu2", entry.changeMap[Kind::Install][0].currentVersion);
}

TEST(HistoryParseTest, EmptyOptionalFields)
{
   FileFd fd;
   const char *entry_section =
      "Start-Date: 2025-09-01  15:22:56\n"
      "Commandline: apt install rust-coreutils\n"
      "Requested-By: user (1000)\n"
      "Install: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "End-Date: 2025-09-01  15:22:57";
   openTemporaryFile("packagesection", fd, entry_section);
   pkgTagFile tfile(&fd);
   pkgTagSection section;
   ASSERT_TRUE(tfile.Step(section));

   Entry entry = ParseSection(section);
   EXPECT_EQ("", entry.error);
   EXPECT_EQ("", entry.comment);
}

TEST(HistoryParseTest, MultipleActions)
{
   FileFd fd;
   const char *entry_section =
      "Start-Date: 2025-09-01  15:22:56\n"
      "Commandline: apt install rust-coreutils\n"
      "Requested-By: user (1000)\n"
      "Error: An error occurred\n"
      "Comment: This is a comment\n"
      "Install: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "Remove: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "Downgrade: rust-coreutils:amd64 (0.1.0, 0.0.0)\n"
      "Reinstall: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "Upgrade: rust-coreutils:amd64 (0.1.0, 0.2.0)\n"
      "Purge: rust-coreutils:amd64 (0.1.0+git20250813.4af2a84-0ubuntu2)\n"
      "End-Date: 2025-09-01  15:22:57";

   openTemporaryFile("packagesection", fd, entry_section);

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   ASSERT_TRUE(tfile.Step(section));

   Entry entry = ParseSection(section);
   Change installChange = entry.changeMap[Kind::Install][0];
   Change removeChange = entry.changeMap[Kind::Remove][0];
   Change downgradeChange = entry.changeMap[Kind::Downgrade][0];
   Change reinstallChange = entry.changeMap[Kind::Reinstall][0];
   Change upgradeChange = entry.changeMap[Kind::Upgrade][0];
   Change purgeChange = entry.changeMap[Kind::Purge][0];

   EXPECT_EQ("rust-coreutils:amd64", installChange.package);
   EXPECT_EQ("0.1.0+git20250813.4af2a84-0ubuntu2", installChange.currentVersion);

   EXPECT_EQ("rust-coreutils:amd64", removeChange.package);
   EXPECT_EQ("0.1.0+git20250813.4af2a84-0ubuntu2", removeChange.currentVersion);

   EXPECT_EQ("rust-coreutils:amd64", downgradeChange.package);
   EXPECT_EQ("0.1.0", downgradeChange.currentVersion);
   EXPECT_EQ("0.0.0", downgradeChange.candidateVersion);

   EXPECT_EQ("rust-coreutils:amd64", reinstallChange.package);
   EXPECT_EQ("0.1.0+git20250813.4af2a84-0ubuntu2", reinstallChange.currentVersion);

   EXPECT_EQ("rust-coreutils:amd64", upgradeChange.package);
   EXPECT_EQ("0.1.0", upgradeChange.currentVersion);
   EXPECT_EQ("0.2.0", upgradeChange.candidateVersion);

   EXPECT_EQ("rust-coreutils:amd64", purgeChange.package);
   EXPECT_EQ("0.1.0+git20250813.4af2a84-0ubuntu2", purgeChange.currentVersion);
}
