#include <string>
/*void basic_string<char,string_char_traits<char>,alloc>::Rep::release()
{
   cout << "Release " << (void *)this << ' ' << ref << endl;
   if (--ref == 0) delete this;
}

basic_string<char,string_char_traits<char>,alloc>::~basic_string()
{
   cout << "Destroy " << (void *)this << ',' << rep()->ref << endl;
   rep ()->release ();
}*/



#include <apt-pkg/tagfile.h>
#include <apt-pkg/strutl.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>

#include <signal.h>
#include <stdio.h>
#include <malloc.h>

struct Rep
{
   size_t len, res, ref;
   bool selfish;
};

int main(int argc,char *argv[])
{
   pkgCacheFile Cache;
   OpProgress Prog;
   pkgInitialize(*_config);
   if (Cache.Open(Prog,false) == false)
   {
      _error->DumpErrors();
      return 0;
   }
   
   pkgRecords rec(*Cache);
   while (1)
   {
      pkgCache::VerIterator V = (*Cache)[Cache->PkgBegin()].CandidateVerIter(*Cache);
      pkgRecords::Parser &Parse = rec.Lookup(V.FileList());
      string Foo = Parse.ShortDesc();
      
      cout << (reinterpret_cast<Rep *>(Foo.begin()) - 1)[0].ref << endl;
      
//      cout << Foo << endl;
      
//      cout << rec.Lookup(V.FileList()).ShortDesc() << endl;
      malloc_stats();
   }
   
#if 0   
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
#endif   
   return 0;
}
