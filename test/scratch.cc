#include <apt-pkg/tagfile.h>

#include <signal.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
   FileFd F(argv[1],FileFd::ReadOnly);
   pkgTagFile Reader(F);
   
   pkgTagSection Sect;
   while (Reader.Step(Sect) == true)
   {
      Sect.FindS("Package");
      Sect.FindS("Section");
      Sect.FindS("Version");
      Sect.FindI("Size");
   };
   
   return 0;
}
