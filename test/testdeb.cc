#include <apt-pkg/dirstream.h>
#include <apt-pkg/debfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/extracttar.h>

class NullStream : public pkgDirStream
{
   public:
   virtual bool DoItem(Item &Itm,int &Fd) {return true;};
};

bool Test(const char *File)
{
   FileFd Fd(File,FileFd::ReadOnly);
   debDebFile Deb(Fd);
   
   if (_error->PendingError() == true)
      return false;
   
   // Get the archive member and positition the file 
   const ARArchive::Member *Member = Deb.GotoMember("data.tar.gz");
   if (Member == 0)
      return false;
      
   // Extract it.
   ExtractTar Tar(Deb.GetFile(),Member->Size, "gzip");
   NullStream Dir;
   if (Tar.Go(Dir) == false)
      return false;   
   
   return true;
}

int main(int argc, const char *argv[])
{
   Test(argv[1]);
   _error->DumpErrors();
   return 0;
}
