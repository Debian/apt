#include <apt-pkg/tagfile.h>
#include <apt-pkg/strutl.h>

#include <signal.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
   URI U(argv[1]);
   cout << U.Access << endl;
   cout << U.User << endl;
   cout << U.Password << endl;
   cout << U.Host << endl;
   cout << U.Path << endl;
   cout << U.Port << endl;
      
/*   
   FileFd F(argv[1],FileFd::ReadOnly);
   pkgTagFile Reader(F);
   
   pkgTagSection Sect;
   while (Reader.Step(Sect) == true)
   {
      Sect.FindS("Package");
      Sect.FindS("Section");
      Sect.FindS("Version");
      Sect.FindI("Size");
   };*/
   
   return 0;
}
