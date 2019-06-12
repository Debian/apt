#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/debsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/statechanges.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <memory>

namespace APT
{

class StateChanges::Private
{
public:
   APT::VersionVector hold;
   APT::VersionVector unhold;
   APT::VersionVector install;
   APT::VersionVector deinstall;
   APT::VersionVector purge;
   APT::VersionVector error;
};

#define APT_GETTERSETTER(Name, Container) \
void StateChanges::Name(pkgCache::VerIterator const &Ver) \
{ \
   if (Ver.end() == false) \
      Container.push_back(Ver); \
}\
APT::VersionVector& StateChanges::Name() \
{ \
   return Container; \
}
APT_GETTERSETTER(Hold, d->hold)
APT_GETTERSETTER(Unhold, d->unhold)
APT_GETTERSETTER(Install, d->install)
APT_GETTERSETTER(Remove, d->deinstall)
APT_GETTERSETTER(Purge, d->purge)
#undef APT_GETTERSETTER
APT::VersionVector& StateChanges::Error()
{
   return d->error;
}

void StateChanges::clear()
{
   d->hold.clear();
   d->unhold.clear();
   d->install.clear();
   d->deinstall.clear();
   d->purge.clear();
   d->error.clear();
}

bool StateChanges::empty() const
{
   return d->hold.empty() &&
      d->unhold.empty() &&
      d->install.empty() &&
      d->deinstall.empty() &&
      d->purge.empty() &&
      d->error.empty();
}

bool StateChanges::Save(bool const DiscardOutput)
{
   bool const Debug = _config->FindB("Debug::pkgDpkgPm", false);
   d->error.clear();
   if (d->hold.empty() && d->unhold.empty() && d->install.empty() && d->deinstall.empty() && d->purge.empty())
      return true;

   std::vector<std::string> Args = debSystem::GetDpkgBaseCommand();
   // ensure dpkg knows about the package so that it keeps the status we set
   if (d->hold.empty() == false || d->install.empty() == false)
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
	 if (Debug)
	 {
	    for (auto const &V: makeDpkgAvailable)
	    {
	       std::clog << "echo 'Dummy record for " << V.ParentPkg().FullName(false) << "' | ";
	       std::copy(Args.begin(), Args.end(), std::ostream_iterator<std::string>(std::clog, " "));
	       std::clog << std::endl;
	    }
	 }
	 else
	 {
	    pid_t const dpkgMergeAvail = debSystem::ExecDpkg(Args, &dummyAvail, nullptr, true);

	    FILE* dpkg = fdopen(dummyAvail, "w");
	    for (auto const &V: makeDpkgAvailable)
	       fprintf(dpkg, "Package: %s\nVersion: 0~\nArchitecture: %s\nMaintainer: Dummy Example <dummy@example.org>\n"
		     "Description: dummy package record\n A record is needed to put a package on hold, so here it is.\n\n", V.ParentPkg().Name(), V.Arch());
	    fclose(dpkg);

	    ExecWait(dpkgMergeAvail, "dpkg --merge-avail", true);
	 }
	 Args.erase(Args.begin() + BaseArgs, Args.end());
      }
   }
   bool const dpkgMultiArch = _system->MultiArchSupported();

   Args.push_back("--set-selections");
   if (Debug)
   {
      std::string state;
      auto const dpkgName = [&](pkgCache::VerIterator const &V) {
	 pkgCache::PkgIterator P = V.ParentPkg();
	 if (strcmp(V.Arch(), "none") == 0)
	    ioprintf(std::clog, "echo '%s %s' | ", P.Name(), state.c_str());
	 else if (dpkgMultiArch == false)
	    ioprintf(std::clog, "echo '%s %s' | ", P.FullName(true).c_str(), state.c_str());
	 else
	    ioprintf(std::clog, "echo '%s:%s %s' | ", P.Name(), V.Arch(), state.c_str());
	 std::copy(Args.begin(), Args.end(), std::ostream_iterator<std::string>(std::clog, " "));
	 std::clog << std::endl;
      };
      for (auto const &V: d->unhold)
      {
	 if (V.ParentPkg()->CurrentVer != 0)
	    state = "install";
	 else
	    state = "deinstall";
	 dpkgName(V);
      }
      if (d->purge.empty() == false)
      {
	 state = "purge";
	 std::for_each(d->purge.begin(), d->purge.end(), dpkgName);
      }
      if (d->deinstall.empty() == false)
      {
	 state = "deinstall";
	 std::for_each(d->deinstall.begin(), d->deinstall.end(), dpkgName);
      }
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
   }
   else
   {
      int selections = -1;
      pid_t const dpkgSelections = debSystem::ExecDpkg(Args, &selections, nullptr, DiscardOutput);

      FILE* dpkg = fdopen(selections, "w");
      std::string state;
      auto const dpkgName = [&](pkgCache::VerIterator const &V) {
	 pkgCache::PkgIterator P = V.ParentPkg();
	 if (strcmp(V.Arch(), "none") == 0)
	    fprintf(dpkg, "%s %s\n", P.Name(), state.c_str());
	 else if (dpkgMultiArch == false)
	    fprintf(dpkg, "%s %s\n", P.FullName(true).c_str(), state.c_str());
	 else
	    fprintf(dpkg, "%s:%s %s\n", P.Name(), V.Arch(), state.c_str());
      };
      for (auto const &V: d->unhold)
      {
	 if (V.ParentPkg()->CurrentVer != 0)
	    state = "install";
	 else
	    state = "deinstall";
	 dpkgName(V);
      }
      if (d->purge.empty() == false)
      {
	 state = "purge";
	 std::for_each(d->purge.begin(), d->purge.end(), dpkgName);
      }
      if (d->deinstall.empty() == false)
      {
	 state = "deinstall";
	 std::for_each(d->deinstall.begin(), d->deinstall.end(), dpkgName);
      }
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
	 std::move(d->purge.begin(), d->purge.end(), std::back_inserter(d->error));
	 d->purge.clear();
	 std::move(d->deinstall.begin(), d->deinstall.end(), std::back_inserter(d->error));
	 d->deinstall.clear();
	 std::move(d->hold.begin(), d->hold.end(), std::back_inserter(d->error));
	 d->hold.clear();
	 std::move(d->unhold.begin(), d->unhold.end(), std::back_inserter(d->error));
	 d->unhold.clear();
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
