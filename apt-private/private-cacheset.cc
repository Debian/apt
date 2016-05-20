#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cacheset.h>

#include <stddef.h>

#include <apti18n.h>

bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,		/*{{{*/
                                 APT::VersionContainerInterface * const vci,
                                 OpProgress * const progress)
{
    Matcher null_matcher = Matcher();
    return GetLocalitySortedVersionSet(CacheFile, vci,
                                       null_matcher, progress);
}
bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,
                                 APT::VersionContainerInterface * const vci,
                                 Matcher &matcher,
                                 OpProgress * const progress)
{
   pkgCache * const Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == nullptr))
      return false;
   if (progress != nullptr)
      progress->SubProgress(Cache->Head().PackageCount, _("Sorting"));

   pkgDepCache * const DepCache = CacheFile.GetDepCache();
   if (unlikely(DepCache == nullptr))
      return false;
   APT::CacheSetHelper helper(false);

   int Done=0;

   bool const insertCurrentVer = _config->FindB("APT::Cmd::Installed", false);
   bool const insertUpgradable = _config->FindB("APT::Cmd::Upgradable", false);
   bool const insertManualInstalled = _config->FindB("APT::Cmd::Manual-Installed", false);

   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
   {
      if (progress != NULL)
      {
	 if (Done % 500 == 0)
	    progress->Progress(Done);
	 ++Done;
      }

      // exclude virtual pkgs
      if (P->VersionList == 0)
	 continue;

      if ((matcher)(P) == false)
	 continue;

      pkgDepCache::StateCache &state = (*DepCache)[P];
      if (insertCurrentVer == true)
      {
	 if (P->CurrentVer != 0)
	    vci->FromPackage(vci, CacheFile, P, APT::CacheSetHelper::INSTALLED, helper);
      }
      else if (insertUpgradable == true)
      {
	 if(P.CurrentVer() && state.Upgradable())
	    vci->FromPackage(vci, CacheFile, P, APT::CacheSetHelper::CANDIDATE, helper);
      }
      else if (insertManualInstalled == true)
      {
	 if (P.CurrentVer() &&
	       ((*DepCache)[P].Flags & pkgCache::Flag::Auto) == false)
	    vci->FromPackage(vci, CacheFile, P, APT::CacheSetHelper::CANDIDATE, helper);
      }
      else
      {
         if (vci->FromPackage(vci, CacheFile, P, APT::CacheSetHelper::CANDIDATE, helper) == false)
	 {
	    // no candidate, this may happen for packages in
	    // dpkg "deinstall ok config-file" state - we pick the first ver
	    // (which should be the only one)
	    vci->insert(P.VersionList());
	 }
      }
   }
   if (progress != NULL)
      progress->Done();
   return true;
}
									/*}}}*/

// CacheSetHelper saving virtual packages				/*{{{*/
pkgCache::VerIterator CacheSetHelperVirtuals::canNotGetVersion(
      enum CacheSetHelper::VerSelector const select,
      pkgCacheFile &Cache,
      pkgCache::PkgIterator const &Pkg)
{
   if (select == NEWEST || select == CANDIDATE || select == ALL)
      virtualPkgs.insert(Pkg);
   return CacheSetHelper::canNotGetVersion(select, Cache, Pkg);
}
void CacheSetHelperVirtuals::canNotFindVersion(
      enum CacheSetHelper::VerSelector const select,
      APT::VersionContainerInterface * vci,
      pkgCacheFile &Cache,
      pkgCache::PkgIterator const &Pkg)
{
   if (select == NEWEST || select == CANDIDATE || select == ALL)
      virtualPkgs.insert(Pkg);
   return CacheSetHelper::canNotFindVersion(select, vci, Cache, Pkg);
}
static pkgCache::PkgIterator canNotFindPkgName_impl(pkgCacheFile &Cache, std::string const &str)
{
   std::string pkg = str;
   size_t const archfound = pkg.find_last_of(':');
   std::string arch;
   if (archfound != std::string::npos) {
      arch = pkg.substr(archfound+1);
      pkg.erase(archfound);
      if (arch == "all" || arch == "native")
	 arch = _config->Find("APT::Architecture");
   }

   // If we don't find 'foo:amd64' look for 'foo:amd64:any'.
   // Note: we prepare for an error here as if foo:amd64 does not exist,
   // but foo:amd64:any it means that this package is only referenced in a
   // (architecture specific) dependency. We do not add to virtualPkgs directly
   // as we can't decide from here which error message has to be printed.
   // FIXME: This doesn't match 'barbarian' architectures
   pkgCache::PkgIterator Pkg(Cache, 0);
   std::vector<std::string> const archs = APT::Configuration::getArchitectures();
   if (archfound == std::string::npos)
   {
      for (auto const &a : archs)
      {
	 Pkg = Cache.GetPkgCache()->FindPkg(pkg + ':' + a, "any");
	 if (Pkg.end() == false && Pkg->ProvidesList != 0)
	    break;
      }
      if (Pkg.end() == true)
	 for (auto const &a : archs)
	 {
	    Pkg = Cache.GetPkgCache()->FindPkg(pkg + ':' + a, "any");
	    if (Pkg.end() == false)
	       break;
	 }
   }
   else
   {
      Pkg = Cache.GetPkgCache()->FindPkg(pkg + ':' + arch, "any");
      if (Pkg.end() == true)
      {
	 APT::CacheFilter::PackageArchitectureMatchesSpecification pams(arch);
	 for (auto const &a : archs)
	 {
	    if (pams(a.c_str()) == false)
	       continue;
	    Pkg = Cache.GetPkgCache()->FindPkg(pkg + ':' + a, "any");
	    if (Pkg.end() == false)
	       break;
	 }
      }
   }
   return Pkg;
}
pkgCache::PkgIterator CacheSetHelperVirtuals::canNotFindPkgName(pkgCacheFile &Cache, std::string const &str)
{
   pkgCache::PkgIterator const Pkg = canNotFindPkgName_impl(Cache, str);
   if (Pkg.end())
      return APT::CacheSetHelper::canNotFindPkgName(Cache, str);
   return Pkg;
}
CacheSetHelperVirtuals::CacheSetHelperVirtuals(bool const ShowErrors, GlobalError::MsgType const &ErrorType) :
   CacheSetHelper{ShowErrors, ErrorType}
{}
									/*}}}*/

// CacheSetHelperAPTGet - responsible for message telling from the CacheSets/*{{{*/
CacheSetHelperAPTGet::CacheSetHelperAPTGet(std::ostream &pout) :
   APT::CacheSetHelper{true}, out(pout)
{
   explicitlyNamed = true;
}
void CacheSetHelperAPTGet::showTaskSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern)
{
   ioprintf(out, _("Note, selecting '%s' for task '%s'\n"),
	 Pkg.FullName(true).c_str(), pattern.c_str());
   explicitlyNamed = false;
}
void CacheSetHelperAPTGet::showFnmatchSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern)
{
   ioprintf(out, _("Note, selecting '%s' for glob '%s'\n"),
	 Pkg.FullName(true).c_str(), pattern.c_str());
   explicitlyNamed = false;
}
void CacheSetHelperAPTGet::showRegExSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern)
{
   ioprintf(out, _("Note, selecting '%s' for regex '%s'\n"),
	 Pkg.FullName(true).c_str(), pattern.c_str());
   explicitlyNamed = false;
}
void CacheSetHelperAPTGet::showSelectedVersion(pkgCache::PkgIterator const &/*Pkg*/, pkgCache::VerIterator const Ver,
      std::string const &ver, bool const /*verIsRel*/)
{
   if (ver == Ver.VerStr())
      return;
   selectedByRelease.push_back(make_pair(Ver, ver));
}
bool CacheSetHelperAPTGet::showVirtualPackageErrors(pkgCacheFile &Cache)
{
   if (virtualPkgs.empty() == true)
      return true;
   for (APT::PackageSet::const_iterator Pkg = virtualPkgs.begin();
	 Pkg != virtualPkgs.end(); ++Pkg) {
      if (Pkg->ProvidesList != 0) {
	 ioprintf(c1out,_("Package %s is a virtual package provided by:\n"),
	       Pkg.FullName(true).c_str());

	 pkgCache::PrvIterator I = Pkg.ProvidesList();
	 unsigned short provider = 0;
	 for (; I.end() == false; ++I) {
	    pkgCache::PkgIterator Pkg = I.OwnerPkg();

	    if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer()) {
	       c1out << "  " << Pkg.FullName(true) << " " << I.OwnerVer().VerStr();
	       if (Cache[Pkg].Install() == true && Cache[Pkg].NewInstall() == false)
		  c1out << _(" [Installed]");
	       c1out << std::endl;
	       ++provider;
	    }
	 }
	 // if we found no candidate which provide this package, show non-candidates
	 if (provider == 0)
	    for (I = Pkg.ProvidesList(); I.end() == false; ++I)
	       c1out << "  " << I.OwnerPkg().FullName(true) << " " << I.OwnerVer().VerStr()
		  << _(" [Not candidate version]") << std::endl;
	 else
	    out << _("You should explicitly select one to install.") << std::endl;
      } else {
	 ioprintf(c1out,
	       _("Package %s is not available, but is referred to by another package.\n"
		  "This may mean that the package is missing, has been obsoleted, or\n"
		  "is only available from another source\n"),Pkg.FullName(true).c_str());

	 std::vector<bool> Seen(Cache.GetPkgCache()->Head().PackageCount, false);
	 APT::PackageList pkglist;
	 for (pkgCache::DepIterator Dep = Pkg.RevDependsList();
	       Dep.end() == false; ++Dep) {
	    if (Dep->Type != pkgCache::Dep::Replaces)
	       continue;
	    pkgCache::PkgIterator const DP = Dep.ParentPkg();
	    if (Seen[DP->ID] == true)
	       continue;
	    Seen[DP->ID] = true;
	    pkglist.insert(DP);
	 }
	 ShowList(c1out, _("However the following packages replace it:"), pkglist,
	       &AlwaysTrue, &PrettyFullName, &EmptyString);
      }
      c1out << std::endl;
   }
   return false;
}
pkgCache::VerIterator CacheSetHelperAPTGet::canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg)
{
   APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, CacheSetHelper::CANDIDATE);
   if (verset.empty() == false)
      return *(verset.begin());
   else if (ShowError == true) {
      _error->Error(_("Package '%s' has no installation candidate"),Pkg.FullName(true).c_str());
      virtualPkgs.insert(Pkg);
   }
   return pkgCache::VerIterator(Cache, 0);
}
pkgCache::VerIterator CacheSetHelperAPTGet::canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg)
{
   if (Pkg->ProvidesList != 0)
   {
      APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, CacheSetHelper::NEWEST);
      if (verset.empty() == false)
	 return *(verset.begin());
      if (ShowError == true)
	 ioprintf(out, _("Virtual packages like '%s' can't be removed\n"), Pkg.FullName(true).c_str());
   }
   else
   {
      pkgCache::GrpIterator Grp = Pkg.Group();
      pkgCache::PkgIterator P = Grp.PackageList();
      for (; P.end() != true; P = Grp.NextPkg(P))
      {
	 if (P == Pkg)
	    continue;
	 if (P->CurrentVer != 0) {
	    // TRANSLATORS: Note, this is not an interactive question
	    ioprintf(c1out,_("Package '%s' is not installed, so not removed. Did you mean '%s'?\n"),
		  Pkg.FullName(true).c_str(), P.FullName(true).c_str());
	    break;
	 }
      }
      if (P.end() == true)
	 ioprintf(c1out,_("Package '%s' is not installed, so not removed\n"),Pkg.FullName(true).c_str());
   }
   return pkgCache::VerIterator(Cache, 0);
}
APT::VersionSet CacheSetHelperAPTGet::tryVirtualPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg,
      CacheSetHelper::VerSelector const select)
{
   /* This is a pure virtual package and there is a single available
      candidate providing it. */
   if (unlikely(Cache[Pkg].CandidateVer != 0) || Pkg->ProvidesList == 0)
      return APT::VersionSet();

   pkgCache::PkgIterator Prov;
   bool found_one = false;
   for (pkgCache::PrvIterator P = Pkg.ProvidesList(); P; ++P) {
      pkgCache::VerIterator const PVer = P.OwnerVer();
      pkgCache::PkgIterator const PPkg = PVer.ParentPkg();

      /* Ignore versions that are not a candidate. */
      if (Cache[PPkg].CandidateVer != PVer)
	 continue;

      if (found_one == false) {
	 Prov = PPkg;
	 found_one = true;
      } else if (PPkg != Prov) {
	 // same group, so it's a foreign package
	 if (PPkg->Group == Prov->Group) {
	    // do we already have the requested arch?
	    if (strcmp(Pkg.Arch(), Prov.Arch()) == 0 ||
		  strcmp(Prov.Arch(), "all") == 0 ||
		  unlikely(strcmp(PPkg.Arch(), Prov.Arch()) == 0)) // packages have only on candidate, but just to be sure
	       continue;
	    // see which architecture we prefer more and switch to it
	    std::vector<std::string> archs = APT::Configuration::getArchitectures();
	    if (std::find(archs.begin(), archs.end(), PPkg.Arch()) < std::find(archs.begin(), archs.end(), Prov.Arch()))
	       Prov = PPkg;
	    continue;
	 }
	 found_one = false; // we found at least two
	 break;
      }
   }

   if (found_one == true) {
      ioprintf(out, _("Note, selecting '%s' instead of '%s'\n"),
	    Prov.FullName(true).c_str(), Pkg.FullName(true).c_str());
      return APT::VersionSet::FromPackage(Cache, Prov, select, *this);
   }
   return APT::VersionSet();
}
pkgCache::PkgIterator CacheSetHelperAPTGet::canNotFindPkgName(pkgCacheFile &Cache, std::string const &str)
{
   pkgCache::PkgIterator const Pkg = canNotFindPkgName_impl(Cache, str);
   if (Pkg.end())
      return APT::CacheSetHelper::canNotFindPkgName(Cache, str);
   return Pkg;
}
									/*}}}*/
