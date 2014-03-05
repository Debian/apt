#include <config.h>

#include <apt-pkg/debfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <iostream>
#include <unistd.h>

using namespace std;

static bool ExtractMember(const char *File,const char *Member)
{
   FileFd Fd(File,FileFd::ReadOnly);
   debDebFile Deb(Fd);
   if(_error->PendingError() == true)
      return false;
   
   debDebFile::MemControlExtract Extract(Member);
   if (Extract.Read(Deb) == false)
      return false;
   
   if (Extract.Control == 0)
      return true;
   
   return write(STDOUT_FILENO,Extract.Control,Extract.Length) != -1;
}

int main(int argc, const char *argv[])
{
   if (argc < 2)
   {
      cerr << "Need two arguments, a .deb and the control member" << endl;
      return 100;
   }
   
   if (ExtractMember(argv[1],argv[2]) == false)
   {
      _error->DumpErrors();
      return 100;
   }
   
   return 0;
}
