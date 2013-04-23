#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>

#include "assert.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *tempfile = NULL;
int tempfile_fd = -1;

void remove_tmpfile(void)
{
   if (tempfile_fd > 0)
      close(tempfile_fd);
   if (tempfile != NULL) {
      unlink(tempfile);
      free(tempfile);
   }
}

int main(int argc, char *argv[])
{
   FileFd fd;
   const char contents[] = "FieldA-12345678: the value of the field";
   atexit(remove_tmpfile);
   tempfile = strdup("apt-test.XXXXXXXX");
   tempfile_fd = mkstemp(tempfile);

   /* (Re-)Open (as FileFd), write and seek to start of the temp file */
   equals(fd.OpenDescriptor(tempfile_fd, FileFd::ReadWrite), true);
   equals(fd.Write(contents, strlen(contents)), true);
   equals(fd.Seek(0), true);

   pkgTagFile tfile(&fd);
   pkgTagSection section;
   equals(tfile.Step(section), true);
  
   /* It has one field */
   equals(section.Count(), 1);

   /* ... and it is called FieldA-12345678 */
   equals(section.Exists("FieldA-12345678"), true);

   /* its value is correct */
   equals(section.FindS("FieldA-12345678"), std::string("the value of the field"));
   /* A non-existent field has an empty string as value */
   equals(section.FindS("FieldB-12345678"), std::string());

   /* ... and Exists does not lie about missing fields... */
   equalsNot(section.Exists("FieldB-12345678"), true); 

   /* There is only one section in this tag file */
   equals(tfile.Step(section), false);

   /* clean up handled by atexit handler, so just return here */
   return 0;
}
