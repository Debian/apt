#include <apt-pkg/acquire-item.h>
#include <apt-pkg/init.h>
#include <apt-pkg/error.h>
#include <signal.h>
#include <strutl.h>

int main(int argc,char *argv[])
{
   signal(SIGPIPE,SIG_IGN);

   URI Foo(argv[1]);
   cout << Foo.Access << '\'' << endl;
   cout << Foo.Host << '\'' << endl;
   cout << Foo.Path << '\'' << endl;
   cout << Foo.User << '\'' << endl;
   cout << Foo.Password << '\'' << endl;
   cout << Foo.Port << endl;
   
   return 0;

   pkgInitialize(*_config);
   
   pkgSourceList List;
   pkgAcquire Fetcher;
   List.ReadMainList();
   
   pkgSourceList::const_iterator I;
   for (I = List.begin(); I != List.end(); I++)
   {
      new pkgAcqIndex(&Fetcher,I);
      if (_error->PendingError() == true)
	 break;
   }

   Fetcher.Run();
   
   _error->DumpErrors();
}
