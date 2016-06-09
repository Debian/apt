#include <apt-pkg/fileutl.h>

#include <string>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

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
void helperCreateTemporaryFile(std::string const &id, FileFd &fd, std::string * const filename, char const * const content)
{
   std::string name("apt-test-");
   name.append(id);
   size_t const giventmp = name.find(".XXXXXX.");
   if (giventmp == std::string::npos)
      name.append(".XXXXXX");
   char * tempfile = strdup(name.c_str());
   ASSERT_STRNE(NULL, tempfile);
   int tempfile_fd;
   if (giventmp == std::string::npos)
      tempfile_fd = mkstemp(tempfile);
   else
      tempfile_fd = mkstemps(tempfile, name.length() - (giventmp + 7));
   ASSERT_NE(-1, tempfile_fd);
   if (filename != NULL)
      *filename = tempfile;
   else
      unlink(tempfile);
   free(tempfile);

   EXPECT_TRUE(fd.OpenDescriptor(tempfile_fd, FileFd::ReadWrite, true));
   if (content != NULL)
   {
      ASSERT_TRUE(fd.Write(content, strlen(content)));
      fd.Seek(0);
   }
}
