#ifndef APT_TESTS_FILE_HELPERS
#define APT_TESTS_FILE_HELPERS

#include <string>

#include <gtest/gtest.h>

class FileFd;

#define createTemporaryDirectory(id, dir) \
   ASSERT_NO_FATAL_FAILURE(helperCreateTemporaryDirectory(id, dir))
void helperCreateTemporaryDirectory(std::string const &id, std::string &dir);
#define removeDirectory(dir) \
   ASSERT_NO_FATAL_FAILURE(helperRemoveDirectory(dir))
void helperRemoveDirectory(std::string const &dir);
#define createFile(dir, name) \
   ASSERT_NO_FATAL_FAILURE(helperCreateFile(dir, name))
void helperCreateFile(std::string const &dir, std::string const &name);
#define createDirectory(dir, name) \
   ASSERT_NO_FATAL_FAILURE(helperCreateDirectory(dir, name))
void helperCreateDirectory(std::string const &dir, std::string const &name);
#define createLink(dir, targetname, linkname) \
   ASSERT_NO_FATAL_FAILURE(helperCreateLink(dir, targetname, linkname))
void helperCreateLink(std::string const &dir, std::string const &targetname, std::string const &linkname);
#define createTemporaryFile(id, fd, filename, content) \
   ASSERT_NO_FATAL_FAILURE(helperCreateTemporaryFile(id, fd, filename, content))
void helperCreateTemporaryFile(std::string const &id, FileFd &fd, std::string * const filename, char const * const content);

#endif
