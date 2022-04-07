#include <config.h>

#include <apt-pkg/cacheset.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>
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
   switch (select)
   {
      case VERSIONNUMBER:
      case RELEASE:
      case INSTALLED:
      case CANDIDATE:
      case NEWEST:
      case ALL:
	 virtualPkgs.insert(Pkg);
	 break;
      case CANDANDINST:
      case CANDINST:
      case INSTCAND:
	 break;
   }
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
void CacheSetHelperAPTGet::showPackageSelection(pkgCache::PkgIterator const &pkg, enum PkgSelector const select,
						std::string const &pattern)
{
   switch (select)
   {
   case REGEX:
      showRegExSelection(pkg, pattern);
      break;
   case TASK:
      showTaskSelection(pkg, pattern);
      break;
   case FNMATCH:
      showFnmatchSelection(pkg, pattern);
      break;
   default:
      APT::CacheSetHelper::showPackageSelection(pkg, select, pattern);
      break;
   }
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
void CacheSetHelperAPTGet::showVersionSelection(pkgCache::PkgIterator const &Pkg,
						pkgCache::VerIterator const &Ver, enum VerSelector const select, std::string const &pattern)
{
   switch (select)
   {
   case VERSIONNUMBER:
      if (pattern == Ver.VerStr())
	 return;
      /* fall through */
   case RELEASE:
      selectedByRelease.push_back(make_pair(Ver, pattern));
      break;
   default:
      return APT::CacheSetHelper::showVersionSelection(Pkg, Ver, select, pattern);
   }
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
	    pkgCache::PkgIterator const OPkg = I.OwnerPkg();

	    if (Cache[OPkg].CandidateVerIter(Cache) == I.OwnerVer())
	    {
	       c1out << "  " << OPkg.FullName(true) << ' ' << I.OwnerVer().VerStr();
	       if (I->ProvideVersion != 0)
		  c1out << " (= " << I.ProvideVersion() << ")";
	       if (Cache[OPkg].Install() == true && Cache[OPkg].NewInstall() == false)
		  c1out << _(" [Installed]");
	       c1out << std::endl;
	       ++provider;
	    }
	 }
	 // if we found no candidate which provide this package, show non-candidates
	 if (provider == 0)
	    for (I = Pkg.ProvidesList(); I.end() == false; ++I)
	    {
	       c1out << "  " << I.OwnerPkg().FullName(true) << " " << I.OwnerVer().VerStr();
	       if (I->ProvideVersion != 0)
		  c1out << " (= " << I.ProvideVersion() << ")";
	       c1out << _(" [Not candidate version]") << std::endl;
	    }
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
pkgCache::VerIterator CacheSetHelperAPTGet::canNotGetVersion(enum VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg)
{
   switch (select)
   {
   case NEWEST:
      return canNotFindNewestVer(Cache, Pkg);
   case CANDIDATE:
      return canNotFindCandidateVer(Cache, Pkg);
   case VERSIONNUMBER:
      return canNotFindVersionNumber(Cache, Pkg, getLastVersionMatcher());
   case RELEASE:
      return canNotFindVersionRelease(Cache, Pkg, getLastVersionMatcher());
   default:
      return APT::CacheSetHelper::canNotGetVersion(select, Cache, Pkg);
   }
}
void CacheSetHelperAPTGet::canNotFindVersion(enum VerSelector const select, APT::VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg)
{
   switch (select)
   {
   case NEWEST:
      canNotFindNewestVer(Cache, Pkg);
      break;
   case CANDIDATE:
      canNotFindCandidateVer(Cache, Pkg);
      break;
   default:
      return APT::CacheSetHelper::canNotFindVersion(select, vci, Cache, Pkg);
   }
}

pkgCache::VerIterator CacheSetHelperAPTGet::canNotFindVersionNumber(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg, std::string const &verstr)
{
   APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, CacheSetHelper::VERSIONNUMBER);
   if (not verset.empty())
      return *(verset.begin());
   else if (ShowError)
   {
      auto const V = canNotGetVerFromVersionNumber(Cache, Pkg, verstr);
      if (not V.end())
	 return V;
      virtualPkgs.insert(Pkg);
   }
   return pkgCache::VerIterator(Cache, 0);
}
pkgCache::VerIterator CacheSetHelperAPTGet::canNotFindVersionRelease(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg, std::string const &verstr)
{
   APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, CacheSetHelper::RELEASE);
   if (not verset.empty())
      return *(verset.begin());
   else if (ShowError)
   {
      auto const V = canNotGetVerFromRelease(Cache, Pkg, verstr);
      if (not V.end())
	 return V;
      virtualPkgs.insert(Pkg);
   }
   return pkgCache::VerIterator(Cache, 0);
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
   /* If this is a virtual package see if we have a single matching provider
      (ignoring multiple matches from the same package due to e.g. M-A) */
   if (Pkg->ProvidesList == 0)
      return APT::VersionSet{};

   auto const oldShowError = showErrors(false);
   APT::VersionVector verset;
   auto const lastmatcher = getLastVersionMatcher();
   for (auto P = Pkg.ProvidesList(); not P.end(); ++P)
   {
      auto V = P.OwnerVer();
      switch (select)
      {
	 case RELEASE:
	    for (auto File = V.FileList(); not File.end(); ++File)
	       if ((File.File().Archive() != nullptr && lastmatcher == File.File().Archive()) ||
		   (File.File().Codename() != nullptr && lastmatcher == File.File().Codename()))
	       {
		  verset.push_back(V);
		  break;
	       }
	    break;
	 case VERSIONNUMBER:
	    if (P->ProvideVersion != 0 && lastmatcher == P.ProvideVersion())
	       verset.push_back(V);
	    break;
	 default:
	    if (Cache[V.ParentPkg()].CandidateVerIter(Cache) == V)
	       verset.push_back(V);
	    break;
      }
   }
   // do not change the candidate if we have more than one option for this package
   if (select == VERSIONNUMBER || select == RELEASE)
      for (auto const &V : verset)
	 if (std::count_if(verset.begin(), verset.end(), [Pkg = V.ParentPkg()](auto const &v) { return v.ParentPkg() == Pkg; }) == 1)
	    Cache->SetCandidateVersion(V);
   showErrors(oldShowError);

   pkgCache::VerIterator Choosen;
   for (auto const &Ver : verset)
   {
      if (Choosen.end())
	 Choosen = Ver;
      else
      {
	 auto const ChoosenPkg = Choosen.ParentPkg();
	 auto const AltPkg = Ver.ParentPkg();
	 // seeing two different packages makes it not simple anymore
	 if (ChoosenPkg->Group != AltPkg->Group)
	    return APT::VersionSet{};
	 // do we already have the requested arch?
	 if (strcmp(Pkg.Arch(), ChoosenPkg.Arch()) == 0 ||
	     strcmp(ChoosenPkg.Arch(), "all") == 0)
	    continue;
	 // see which architecture we prefer more and switch to it
	 std::vector<std::string> archs = APT::Configuration::getArchitectures();
	 if (std::find(archs.begin(), archs.end(), AltPkg.Arch()) < std::find(archs.begin(), archs.end(), ChoosenPkg.Arch()))
	    Choosen = Ver;
      }
   }
   if (Choosen.end())
      return APT::VersionSet{};

   ioprintf(out, _("Note, selecting '%s' instead of '%s'\n"),
	 Choosen.ParentPkg().FullName(true).c_str(), Pkg.FullName(true).c_str());
   return { Choosen };
}
pkgCache::PkgIterator CacheSetHelperAPTGet::canNotFindPkgName(pkgCacheFile &Cache, std::string const &str)
{
   pkgCache::PkgIterator Pkg = canNotFindPkgName_impl(Cache, str);
   if (Pkg.end())
   {
      Pkg = APT::CacheSetHelper::canNotFindPkgName(Cache, str);
      if (Pkg.end() && ShowError)
      {
	 notFound.insert(str);
      }
   }
   return Pkg;
}
									/*}}}*/
