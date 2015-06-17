// Include Files                                                       /*{{{*/
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>

#include <stddef.h>

#include <string>
#include <vector>
                                                                       /*}}}*/

#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
std::string metaIndex::LocalFileName() const { return ""; }
#else
#include <apt-pkg/debmetaindex.h>
std::string metaIndex::LocalFileName() const
{
   debReleaseIndex const * deb = dynamic_cast<debReleaseIndex const*>(this);
   if (deb != NULL)
      return deb->LocalFileName();

   return "";
}
#endif

std::string metaIndex::Describe() const
{
   return "Release";
}

pkgCache::RlsFileIterator metaIndex::FindInCache(pkgCache &Cache, bool const) const
{
   return pkgCache::RlsFileIterator(Cache);
}

bool metaIndex::Merge(pkgCacheGenerator &Gen,OpProgress *) const
{
   return Gen.SelectReleaseFile("", "");
}


metaIndex::metaIndex(std::string const &URI, std::string const &Dist,
      char const * const Type)
: d(NULL), Indexes(NULL), Type(Type), URI(URI), Dist(Dist), Trusted(false)
{
   /* nothing */
}

metaIndex::~metaIndex()
{
   if (Indexes == 0)
      return;
   for (std::vector<pkgIndexFile *>::iterator I = (*Indexes).begin();
        I != (*Indexes).end(); ++I)
      delete *I;
   delete Indexes;
}
