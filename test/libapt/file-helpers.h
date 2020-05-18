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

class ScopedFileDeleter {
   std::string _filename;
public:
   ScopedFileDeleter(std::string const &filename);
   ScopedFileDeleter(ScopedFileDeleter const &) = delete;
   ScopedFileDeleter(ScopedFileDeleter &&);
   ScopedFileDeleter& operator=(ScopedFileDeleter const &) = delete;
   ScopedFileDeleter& operator=(ScopedFileDeleter &&);
   ~ScopedFileDeleter();

   std::string Name() const { return _filename; }
};
void openTemporaryFile(std::string const &id, FileFd &fd, char const * const content = nullptr, bool const ImmediateUnlink = true);
ScopedFileDeleter createTemporaryFile(std::string const &id, char const * const content = nullptr);

#endif
