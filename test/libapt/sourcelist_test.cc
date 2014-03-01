#include <apt-pkg/sourcelist.h>
#include <apt-pkg/tagfile.h>

#include "assert.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *tempfile = NULL;
int tempfile_fd = -1;

static void remove_tmpfile(void)
{
   if (tempfile_fd > 0)
      close(tempfile_fd);
   if (tempfile != NULL) {
      unlink(tempfile);
      free(tempfile);
   }
}

int main()
{
  _config->Set("APT::Sources::Use-Deb822", true);

   const char contents[] = ""
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: stable\n"
      "Sections: main\n"
      "Description: short\n"
      " long description that can be very long\n"
      "\n"
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: unstable\n"
      "Sections: main non-free\n"
      ;

   FileFd fd;
   atexit(remove_tmpfile);
   tempfile = strdup("apt-test.XXXXXXXX");
   tempfile_fd = mkstemp(tempfile);

   /* (Re-)Open (as FileFd), write and seek to start of the temp file */
   equals(fd.OpenDescriptor(tempfile_fd, FileFd::ReadWrite), true);
   equals(fd.Write(contents, strlen(contents)), true);
   equals(fd.Seek(0), true);

   pkgSourceList sources(tempfile);
   equals(sources.size(), 2);

   /* clean up handled by atexit handler, so just return here */
   return 0;
}
