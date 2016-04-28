// Description								/*{{{*/
/* ######################################################################

   Provide pretty printers for pkgCache structs like PkgIterator

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/depcache.h>
#include <apt-pkg/prettyprinters.h>

#include <ostream>
#include <string>

									/*}}}*/

std::ostream& operator<<(std::ostream& os, const APT::PrettyPkg& pp)	/*{{{*/
{
   if (pp.Pkg.end() == true)
      return os << "invalid package";

   std::string current = (pp.Pkg.CurVersion() == 0 ? "none" : pp.Pkg.CurVersion());
   std::string candidate = (*pp.DepCache)[pp.Pkg].CandVersion;
   std::string newest = (pp.Pkg.VersionList().end() ? "none" : pp.Pkg.VersionList().VerStr());

   os << pp.Pkg.Name() << " [ " << pp.Pkg.Arch() << " ] < " << current;
   if (current != candidate)
      os << " -> " << candidate;
   if ( newest != "none" && candidate != newest)
      os << " | " << newest;
   if (pp.Pkg->VersionList == 0)
      os << " > ( none )";
   else
      os << " > ( " << (pp.Pkg.VersionList().Section()==0?"unknown":pp.Pkg.VersionList().Section()) << " )";
   return os;
}
									/*}}}*/
std::ostream& operator<<(std::ostream& os, const APT::PrettyDep& pd)	/*{{{*/
{
   if (unlikely(pd.Dep.end() == true))
      return os << "invalid dependency";

   pkgCache::PkgIterator P = pd.Dep.ParentPkg();
   pkgCache::PkgIterator T = pd.Dep.TargetPkg();

   os << (P.end() ? "invalid pkg" : P.FullName(false)) << " " << pd.Dep.DepType()
	<< " on " << APT::PrettyPkg(pd.DepCache, T);

   if (pd.Dep->Version != 0)
      os << " (" << pd.Dep.CompType() << " " << pd.Dep.TargetVer() << ")";

   return os;
}
									/*}}}*/
