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

   auto state = (*pp.DepCache)[pp.Pkg];
   std::string const current = (pp.Pkg.CurVersion() == 0 ? "none" : pp.Pkg.CurVersion());
   std::string candidate = state.CandVersion;
   if (candidate.empty())
      candidate = "none";
   std::string install = "none";
   if (state.InstallVer != nullptr)
      install = state.InstVerIter(*pp.DepCache).VerStr();

   os << pp.Pkg.FullName(false) << " < " << current;
   if (current != install && install != "none")
      os << " -> " << install;
   if (install != candidate && current != candidate)
      os << " | " << candidate;
   os << " @";
   switch (pp.Pkg->SelectedState)
   {
      case pkgCache::State::Unknown: os << 'u'; break;
      case pkgCache::State::Install: os << 'i'; break;
      case pkgCache::State::Hold: os << 'h'; break;
      case pkgCache::State::DeInstall: os << 'r'; break;
      case pkgCache::State::Purge: os << 'p'; break;
      default: os << 'X';
   }
   switch (pp.Pkg->InstState)
   {
      case pkgCache::State::Ok: break;
      case pkgCache::State::ReInstReq: os << 'R'; break;
      case pkgCache::State::HoldInst: os << 'H'; break;
      case pkgCache::State::HoldReInstReq: os << "HR"; break;
      default: os << 'X';
   }
   switch (pp.Pkg->CurrentState)
   {
      case pkgCache::State::NotInstalled: os << 'n'; break;
      case pkgCache::State::ConfigFiles: os << 'c'; break;
      case pkgCache::State::HalfInstalled: os << 'H'; break;
      case pkgCache::State::UnPacked: os << 'U'; break;
      case pkgCache::State::HalfConfigured: os << 'F'; break;
      case pkgCache::State::TriggersAwaited: os << 'W'; break;
      case pkgCache::State::TriggersPending: os << 'T'; break;
      case pkgCache::State::Installed: os << 'i'; break;
      default: os << 'X';
   }
   os << ' ';
   if (state.Protect())
      os << "p";
   if (state.ReInstall())
      os << "r";
   if (state.Upgradable())
      os << "u";
   if (state.Marked)
      os << "m";
   if (state.Garbage)
      os << "g";
   if (state.NewInstall())
      os << "N";
   else if (state.Upgrade())
      os << "U";
   else if (state.Downgrade())
      os << "D";
   else if (state.Install())
      os << "I";
   else if (state.Purge())
      os << "P";
   else if (state.Delete())
      os << "R";
   else if (state.Held())
      os << "H";
   else if (state.Keep())
      os << "K";
   if (state.NowBroken())
      os << " Nb";
   else if (state.NowPolicyBroken())
      os << " NPb";
   if (state.InstBroken())
      os << " Ib";
   else if (state.InstPolicyBroken())
      os << " IPb";
   os << " >";
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
