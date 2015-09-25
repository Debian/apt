#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/debsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/statechanges.h>

#include <algorithm>
#include <memory>

namespace APT
{

class StateChanges::Private
{
public:
   APT::VersionVector hold;
   APT::VersionVector install;
   APT::VersionVector error;
};

void StateChanges::Hold(pkgCache::VerIterator const &Ver)
{
   d->hold.push_back(Ver);
}
APT::VersionVector& StateChanges::Hold()
{
   return d->hold;
}
void StateChanges::Unhold(pkgCache::VerIterator const &Ver)
{
   d->install.push_back(Ver);
}
APT::VersionVector& StateChanges::Unhold()
{
   return d->install;
}
APT::VersionVector& StateChanges::Error()
{
   return d->error;
}

void StateChanges::Discard()
{
   d->hold.clear();
   d->install.clear();
   d->error.clear();
}

bool StateChanges::Save(bool const DiscardOutput)
{
   d->error.clear();
   if (d->hold.empty() && d->install.empty())
      return true;

   std::vector<std::string> Args = debSystem::GetDpkgBaseCommand();
   // ensure dpkg knows about the package so that it keeps the status we set
   {
      APT::VersionVector makeDpkgAvailable;
      auto const notInstalled = [](pkgCache::VerIterator const &V) { return V.ParentPkg()->CurrentVer == 0; };
      std::copy_if(d->hold.begin(), d->hold.end(), std::back_inserter(makeDpkgAvailable), notInstalled);
      std::copy_if(d->install.begin(), d->install.end(), std::back_inserter(makeDpkgAvailable), notInstalled);

      if (makeDpkgAvailable.empty() == false)
      {
	 auto const BaseArgs = Args.size();
	 Args.push_back("--merge-avail");
	 // FIXME: supported only since 1.17.7 in dpkg
	 Args.push_back("-");
	 int dummyAvail = -1;
	 pid_t const dpkgMergeAvail = debSystem::ExecDpkg(Args, &dummyAvail, nullptr, true);

	 FILE* dpkg = fdopen(dummyAvail, "w");
	 for (auto const &V: makeDpkgAvailable)
	    fprintf(dpkg, "Package: %s\nVersion: 0~\nArchitecture: %s\nMaintainer: Dummy Example <dummy@example.org>\n"
		  "Description: dummy package record\n A record is needed to put a package on hold, so here it is.\n\n", V.ParentPkg().Name(), V.Arch());
	 fclose(dpkg);

	 ExecWait(dpkgMergeAvail, "dpkg --merge-avail", true);
	 Args.erase(Args.begin() + BaseArgs, Args.end());
      }
   }
   bool const dpkgMultiArch = _system->MultiArchSupported();

   Args.push_back("--set-selections");
   int selections = -1;
   pid_t const dpkgSelections = debSystem::ExecDpkg(Args, &selections, nullptr, DiscardOutput);

   FILE* dpkg = fdopen(selections, "w");
   std::string state;
   auto const dpkgName = [&](pkgCache::VerIterator const &V) {
      pkgCache::PkgIterator P = V.ParentPkg();
      if (dpkgMultiArch == false)
	 fprintf(dpkg, "%s %s\n", P.FullName(true).c_str(), state.c_str());
      else
	 fprintf(dpkg, "%s:%s %s\n", P.Name(), V.Arch(), state.c_str());
   };
   if (d->hold.empty() == false)
   {
      state = "hold";
      std::for_each(d->hold.begin(), d->hold.end(), dpkgName);
   }
   if (d->install.empty() == false)
   {
      state = "install";
      std::for_each(d->install.begin(), d->install.end(), dpkgName);
   }
   fclose(dpkg);

   if (ExecWait(dpkgSelections, "dpkg --set-selections") == false)
   {
      if (d->hold.empty())
	 std::swap(d->install, d->error);
      else if (d->install.empty())
	 std::swap(d->hold, d->error);
      else
      {
	 std::swap(d->hold, d->error);
	 std::move(d->install.begin(), d->install.end(), std::back_inserter(d->error));
	 d->install.clear();
      }
   }
   return d->error.empty();
}

StateChanges::StateChanges() : d(new StateChanges::Private()) {}
StateChanges::StateChanges(StateChanges&&) = default;
StateChanges& StateChanges::operator=(StateChanges&&) = default;
StateChanges::~StateChanges() = default;

}
