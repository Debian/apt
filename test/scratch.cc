#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>
#include <apt-pkg/init.h>
#include <apt-pkg/error.h>
#include <strutl.h>

#include <signal.h>
#include <stdio.h>

int main(int argc,char *argv[])
{

   URI Foo(argv[1]);
   cout << Foo.Access << '\'' << endl;
   cout << Foo.Host << '\'' << endl;
   cout << Foo.Path << '\'' << endl;
   cout << Foo.User << '\'' << endl;
   cout << Foo.Password << '\'' << endl;
   cout << Foo.Port << endl;
   
   return 0;
}
