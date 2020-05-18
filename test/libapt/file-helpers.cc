#include <apt-pkg/fileutl.h>

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

void helperCreateTemporaryDirectory(std::string const &id, std::string &dir)
{
   std::string const strtempdir = GetTempDir().append("/apt-tests-").append(id).append(".XXXXXX");
   char * tempdir = strdup(strtempdir.c_str());
   ASSERT_STREQ(tempdir, mkdtemp(tempdir));
   dir = tempdir;
   free(tempdir);
}
void helperRemoveDirectory(std::string const &dir)
{
   // basic sanity check to avoid removing random directories based on earlier failures
   if (dir.find("/apt-tests-") == std::string::npos || dir.find_first_of("*?") != std::string::npos)
      FAIL() << "Directory '" << dir << "' seems invalid. It is therefore not removed!";
   else
      ASSERT_EQ(0, system(std::string("rm -rf ").append(dir).c_str()));
}
void helperCreateFile(std::string const &dir, std::string const &name)
{
   std::string file = dir;
   file.append("/");
   file.append(name);
   int const fd = creat(file.c_str(), 0600);
   ASSERT_NE(-1, fd);
   close(fd);
}
void helperCreateDirectory(std::string const &dir, std::string const &name)
{
   std::string file = dir;
   file.append("/");
   file.append(name);
   ASSERT_TRUE(CreateDirectory(dir, file));
}
void helperCreateLink(std::string const &dir, std::string const &targetname, std::string const &linkname)
{
   std::string target = dir;
   target.append("/");
   target.append(targetname);
   std::string link = dir;
   link.append("/");
   link.append(linkname);
   ASSERT_EQ(0, symlink(target.c_str(), link.c_str()));
}

void openTemporaryFile(std::string const &id, FileFd &fd, char const * const content, bool const ImmediateUnlink)
{
   EXPECT_NE(nullptr, GetTempFile("apt-" + id, ImmediateUnlink, &fd));
   EXPECT_TRUE(ImmediateUnlink || not fd.Name().empty());
   if (content != nullptr)
   {
      EXPECT_TRUE(fd.Write(content, strlen(content)));
      EXPECT_TRUE(fd.Sync());
      fd.Seek(0);
   }
}
ScopedFileDeleter::ScopedFileDeleter(std::string const &filename) : _filename{filename} {}
ScopedFileDeleter::ScopedFileDeleter(ScopedFileDeleter &&sfd) = default;
ScopedFileDeleter& ScopedFileDeleter::operator=(ScopedFileDeleter &&sfd) = default;
ScopedFileDeleter::~ScopedFileDeleter() {
   if (not _filename.empty())
      RemoveFile("ScopedFileDeleter", _filename.c_str());
}
[[nodiscard]] ScopedFileDeleter createTemporaryFile(std::string const &id, char const * const content)
{
   FileFd fd;
   openTemporaryFile(id, fd, content, false);
   EXPECT_TRUE(fd.IsOpen());
   EXPECT_TRUE(fd.Close());
   EXPECT_FALSE(fd.Name().empty());
   return ScopedFileDeleter{fd.Name()};
}
