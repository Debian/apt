#ifndef APT_PRETTYPRINTERS_H
#define APT_PRETTYPRINTERS_H
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

class pkgDepCache;

namespace APT {

/** helper to format PkgIterator for easier printing in debug messages.
 *
 * The actual text generated is subject to change without prior notice
 * and should NOT be used as part of a general user interface.
 */
struct PrettyPkg
{
   pkgDepCache * const DepCache;
   pkgCache::PkgIterator const Pkg;
   PrettyPkg(pkgDepCache * const depcache, pkgCache::PkgIterator const &pkg) APT_NONNULL(2) : DepCache(depcache), Pkg(pkg) {}
};
/** helper to format DepIterator for easier printing in debug messages.
 *
 * The actual text generated is subject to change without prior notice
 * and should NOT be used as part of a general user interface.
 */
struct PrettyDep
{
   pkgDepCache * const DepCache;
   pkgCache::DepIterator const Dep;
   PrettyDep(pkgDepCache * const depcache, pkgCache::DepIterator const &dep) APT_NONNULL(2) : DepCache(depcache), Dep(dep) {}
};

}
APT_PUBLIC std::ostream& operator<<(std::ostream& os, const APT::PrettyPkg& pp);
APT_PUBLIC std::ostream& operator<<(std::ostream& os, const APT::PrettyDep& pd);

#endif
