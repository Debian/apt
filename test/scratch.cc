#include <apt-pkg/acquire-item.h>
#include <apt-pkg/init.h>
#include <apt-pkg/error.h>
#include <signal.h>

int main()
{
   signal(SIGPIPE,SIG_IGN);
   
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
