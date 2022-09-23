// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <string>

#include <apti18n.h>
									/*}}}*/

using std::string;

// we could use pkgCache::DepType and ::Priority, but these would be localized strings…
constexpr char const * const PrioMap[] = {
   nullptr, "important", "required", "standard",
   "optional", "extra"
};
constexpr char const * const DepMap[] = {
   nullptr, "Depends", "Pre-Depends", "Suggests",
   "Recommends" , "Conflicts", "Replaces",
   "Obsoletes", "Breaks", "Enhances"
};

// WriteOkay - varaidic helper to easily Write to a FileFd		/*{{{*/
static bool WriteOkay_fn(FileFd &) { return true; }
template<typename... Tail> static bool WriteOkay_fn(FileFd &output, APT::StringView data, Tail... more_data)
{
   return likely(output.Write(data.data(), data.length()) && WriteOkay_fn(output, more_data...));
}
template<typename... Tail> static bool WriteOkay_fn(FileFd &output, unsigned int data, Tail... more_data)
{
   std::string number;
   strprintf(number, "%d", data);
   return likely(output.Write(number.data(), number.length()) && WriteOkay_fn(output, more_data...));
}
template<typename... Data> static bool WriteOkay(bool &Okay, FileFd &output, Data&&... data)
{
   Okay = likely(Okay && WriteOkay_fn(output, std::forward<Data>(data)...));
   return Okay;
}
template<typename... Data> static bool WriteOkay(FileFd &output, Data&&... data)
{
   bool Okay = likely(output.Failed() == false);
   return WriteOkay(Okay, output, std::forward<Data>(data)...);
}
									/*}}}*/
// WriteScenarioVersion							/*{{{*/
static bool WriteScenarioVersion(FileFd &output, pkgCache::PkgIterator const &Pkg,
				pkgCache::VerIterator const &Ver)
{
   bool Okay = WriteOkay(output, "Package: ", Pkg.Name(),
	 "\nArchitecture: ", Ver.Arch(),
	 "\nVersion: ", Ver.VerStr());
   WriteOkay(Okay, output, "\nAPT-ID: ", Ver->ID);
   if (Ver.PhasedUpdatePercentage() != 100)
      WriteOkay(Okay, output, "\nPhased-Update-Percentage: ", Ver.PhasedUpdatePercentage());
   if ((Pkg->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
      WriteOkay(Okay, output, "\nEssential: yes");
   if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
      WriteOkay(Okay, output, "\nMulti-Arch: allowed");
   else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
      WriteOkay(Okay, output, "\nMulti-Arch: foreign");
   else if ((Ver->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
      WriteOkay(Okay, output, "\nMulti-Arch: same");
   return Okay;
}
									/*}}}*/
// WriteScenarioDependency						/*{{{*/
static bool WriteScenarioDependency(FileFd &output, pkgCache::VerIterator const &Ver, bool const OnlyCritical)
{
   std::array<std::string, APT_ARRAY_SIZE(DepMap)> dependencies;
   bool orGroup = false;
   for (pkgCache::DepIterator Dep = Ver.DependsList(); Dep.end() == false; ++Dep)
   {
      if (Dep.IsImplicit() == true)
	 continue;
      if (OnlyCritical && Dep.IsCritical() == false)
	 continue;
      if (orGroup == false && dependencies[Dep->Type].empty() == false)
	 dependencies[Dep->Type].append(", ");
      dependencies[Dep->Type].append(Dep.TargetPkg().Name());
      if (Dep->Version != 0)
	 dependencies[Dep->Type].append(" (").append(pkgCache::CompTypeDeb(Dep->CompareOp)).append(" ").append(Dep.TargetVer()).append(")");
      if ((Dep->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or)
      {
	 dependencies[Dep->Type].append(" | ");
	 orGroup = true;
      }
      else
	 orGroup = false;
   }
   bool Okay = output.Failed() == false;
   for (size_t i = 1; i < dependencies.size(); ++i)
      if (dependencies[i].empty() == false)
	 WriteOkay(Okay, output, "\n", DepMap[i], ": ", dependencies[i]);
   std::vector<std::string> provides;
   for (auto Prv = Ver.ProvidesList(); not Prv.end(); ++Prv)
   {
      if (Prv.IsMultiArchImplicit())
	 continue;
      std::string provide = Prv.Name();
      if (Prv->ProvideVersion != 0)
	 provide.append(" (= ").append(Prv.ProvideVersion()).append(")");
      if ((Ver->MultiArch & pkgCache::Version::Foreign) != 0 && std::find(provides.cbegin(), provides.cend(), provide) != provides.cend())
	 continue;
      provides.emplace_back(std::move(provide));
   }
   if (not provides.empty())
   {
      std::ostringstream out;
      std::copy(provides.begin(), provides.end() - 1, std::ostream_iterator<std::string>(out, ", "));
      out << provides.back();
      WriteOkay(Okay, output, "\nProvides: ", out.str());
   }
   return WriteOkay(Okay, output, "\n");
}
									/*}}}*/
// WriteScenarioLimitedDependency					/*{{{*/
static bool WriteScenarioLimitedDependency(FileFd &output,
					  pkgCache::VerIterator const &Ver,
					  std::vector<bool> const &pkgset,
					  bool const OnlyCritical)
{
   std::array<std::string, APT_ARRAY_SIZE(DepMap)> dependencies;
   bool orGroup = false;
   for (pkgCache::DepIterator Dep = Ver.DependsList(); Dep.end() == false; ++Dep)
   {
      if (Dep.IsImplicit() == true)
	 continue;
      if (OnlyCritical && Dep.IsCritical() == false)
	 continue;
      if (orGroup == false)
      {
	 if (pkgset[Dep.TargetPkg()->ID] == false)
	    continue;
	 if (dependencies[Dep->Type].empty() == false)
	    dependencies[Dep->Type].append(", ");
      }
      else if (pkgset[Dep.TargetPkg()->ID] == false)
      {
	 if ((Dep->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or)
	    continue;
	 dependencies[Dep->Type].erase(dependencies[Dep->Type].end()-3, dependencies[Dep->Type].end());
	 orGroup = false;
	 continue;
      }
      dependencies[Dep->Type].append(Dep.TargetPkg().Name());
      if (Dep->Version != 0)
	 dependencies[Dep->Type].append(" (").append(pkgCache::CompTypeDeb(Dep->CompareOp)).append(" ").append(Dep.TargetVer()).append(")");
      if ((Dep->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or)
      {
	 dependencies[Dep->Type].append(" | ");
	 orGroup = true;
      }
      else
	 orGroup = false;
   }
   bool Okay = output.Failed() == false;
   for (size_t i = 1; i < dependencies.size(); ++i)
      if (dependencies[i].empty() == false)
	 WriteOkay(Okay, output, "\n", DepMap[i], ": ", dependencies[i]);
   string provides;
   for (pkgCache::PrvIterator Prv = Ver.ProvidesList(); Prv.end() == false; ++Prv)
   {
      if (Prv.IsMultiArchImplicit() == true)
	 continue;
      if (pkgset[Prv.ParentPkg()->ID] == false)
	 continue;
      if (provides.empty() == false)
	 provides.append(", ");
      provides.append(Prv.Name());
      if (Prv->ProvideVersion != 0)
	 provides.append(" (= ").append(Prv.ProvideVersion()).append(")");
   }
   if (provides.empty() == false)
      WriteOkay(Okay, output, "\nProvides: ", provides);
   return WriteOkay(Okay, output, "\n");
}
									/*}}}*/
static bool checkKnownArchitecture(std::string const &arch)		/*{{{*/
{
   if (APT::Configuration::checkArchitecture(arch))
      return true;
   static auto const veryforeign = _config->FindVector("APT::BarbarianArchitectures");
   return std::find(veryforeign.begin(), veryforeign.end(), arch) != veryforeign.end();
}
									/*}}}*/
static bool WriteGenericRequestHeaders(FileFd &output, APT::StringView const head)/*{{{*/
{
   bool Okay = WriteOkay(output, head, "Architecture: ", _config->Find("APT::Architecture"), "\n",
	 "Architectures:");
   for (auto const &a : APT::Configuration::getArchitectures())
       WriteOkay(Okay, output, " ", a);
   for (auto const &a : _config->FindVector("APT::BarbarianArchitectures"))
       WriteOkay(Okay, output, " ", a);
   return WriteOkay(Okay, output, "\n");
}
									/*}}}*/
static bool SkipUnavailableVersions(pkgDepCache &Cache, pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const &Ver)/*{{{*/
{
   /* versions which aren't current and aren't available in
      any "online" source file are bad, expect if they are the chosen
      candidate: The exception is for build-dep implementation as it creates
      such pseudo (package) versions and removes them later on again.
      We filter out versions at all so packages in 'rc' state only available
      in dpkg/status aren't passed to solvers as they can't be installed. */
   if (Pkg->CurrentVer != 0)
      return false;
   if (Cache.GetCandidateVersion(Pkg) == Ver)
      return false;
   for (pkgCache::VerFileIterator I = Ver.FileList(); I.end() == false; ++I)
      if (I.File().Flagged(pkgCache::Flag::NotSource) == false)
	 return false;
   return true;
}
									/*}}}*/
static bool WriteScenarioEDSPVersion(pkgDepCache &Cache, FileFd &output, pkgCache::PkgIterator const &Pkg,/*{{{*/
				pkgCache::VerIterator const &Ver)
{
   bool Okay = WriteOkay(output, "\nSource: ", Ver.SourcePkgName(),
	 "\nSource-Version: ", Ver.SourceVerStr());
   if (PrioMap[Ver->Priority] != nullptr)
      WriteOkay(Okay, output, "\nPriority: ", PrioMap[Ver->Priority]);
   if (Ver->Section != 0)
      WriteOkay(Okay, output, "\nSection: ", Ver.Section());
   if (Pkg.CurrentVer() == Ver)
      WriteOkay(Okay, output, "\nInstalled: yes");
   if (Pkg->SelectedState == pkgCache::State::Hold ||
       (Cache[Pkg].Keep() == true && Cache[Pkg].Protect() == true))
      WriteOkay(Okay, output, "\nHold: yes");
   std::set<string> Releases;
   for (pkgCache::VerFileIterator I = Ver.FileList(); I.end() == false; ++I) {
      pkgCache::PkgFileIterator File = I.File();
      if (File.Flagged(pkgCache::Flag::NotSource) == false) {
	 string Release = File.RelStr();
	 if (!Release.empty())
	    Releases.insert(Release);
      }
   }
   if (!Releases.empty()) {
       WriteOkay(Okay, output, "\nAPT-Release:");
       for (std::set<string>::iterator R = Releases.begin(); R != Releases.end(); ++R)
	   WriteOkay(Okay, output, "\n ", *R);
   }
   WriteOkay(Okay, output, "\nAPT-Pin: ", Cache.GetPolicy().GetPriority(Ver));
   if (Cache.GetCandidateVersion(Pkg) == Ver)
      WriteOkay(Okay, output, "\nAPT-Candidate: yes");
   if ((Cache[Pkg].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
      WriteOkay(Okay, output, "\nAPT-Automatic: yes");
   return Okay;
}
									/*}}}*/
// EDSP::WriteScenario - to the given file descriptor			/*{{{*/
bool EDSP::WriteScenario(pkgDepCache &Cache, FileFd &output, OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().VersionCount, _("Send scenario to solver"));
   decltype(Cache.Head().VersionCount) p = 0;
   bool Okay = output.Failed() == false;
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false && likely(Okay); ++Pkg)
   {
      if (Pkg->CurrentVer == 0 && not checkKnownArchitecture(Pkg.Arch()))
	 continue;
      for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false && likely(Okay); ++Ver, ++p)
      {
	 if (SkipUnavailableVersions(Cache, Pkg, Ver))
	    continue;
	 Okay &= WriteScenarioVersion(output, Pkg, Ver);
	 Okay &= WriteScenarioEDSPVersion(Cache, output, Pkg, Ver);
	 Okay &= WriteScenarioDependency(output, Ver, false);
	 WriteOkay(Okay, output, "\n");
	 if (Progress != NULL && p % 100 == 0)
	    Progress->Progress(p);
      }
   }
   return Okay;
}
									/*}}}*/
// EDSP::WriteLimitedScenario - to the given file descriptor		/*{{{*/
bool EDSP::WriteLimitedScenario(pkgDepCache &Cache, FileFd &output,
				std::vector<bool> const &pkgset,
				OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().VersionCount, _("Send scenario to solver"));
   decltype(Cache.Head().PackageCount) p = 0;
   bool Okay = output.Failed() == false;
   for (auto Pkg = Cache.PkgBegin(); Pkg.end() == false && likely(Okay); ++Pkg, ++p)
   {
      if (pkgset[Pkg->ID] == false)
	 continue;
      for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false && likely(Okay); ++Ver)
      {
	 if (SkipUnavailableVersions(Cache, Pkg, Ver))
	    continue;
	 Okay &= WriteScenarioVersion(output, Pkg, Ver);
	 Okay &= WriteScenarioEDSPVersion(Cache, output, Pkg, Ver);
	 Okay &= WriteScenarioLimitedDependency(output, Ver, pkgset, false);
	 WriteOkay(Okay, output, "\n");
	 if (Progress != NULL && p % 100 == 0)
	    Progress->Progress(p);
      }
   }
   if (Progress != NULL)
      Progress->Done();
   return Okay;
}
									/*}}}*/
// EDSP::WriteRequest - to the given file descriptor			/*{{{*/
bool EDSP::WriteRequest(pkgDepCache &Cache, FileFd &output,
			unsigned int const flags,
			OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().PackageCount, _("Send request to solver"));
   decltype(Cache.Head().PackageCount) p = 0;
   string del, inst;
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg, ++p)
   {
      if (Progress != NULL && p % 100 == 0)
         Progress->Progress(p);
      string* req;
      pkgDepCache::StateCache &P = Cache[Pkg];
      if (P.Delete() == true)
	 req = &del;
      else if (P.NewInstall() == true || P.Upgrade() == true || P.ReInstall() == true ||
	       (P.Mode == pkgDepCache::ModeKeep && (P.iFlags & pkgDepCache::Protected) == pkgDepCache::Protected))
	 req = &inst;
      else
	 continue;
      req->append(" ").append(Pkg.FullName());
   }

   bool Okay = WriteGenericRequestHeaders(output, "Request: EDSP 0.5\n");
   string machineID = APT::Configuration::getMachineID();
   if (not machineID.empty())
      WriteOkay(Okay, output, "Machine-ID: ", machineID, "\n");
   if (del.empty() == false)
      WriteOkay(Okay, output, "Remove:", del, "\n");
   if (inst.empty() == false)
      WriteOkay(Okay, output, "Install:", inst, "\n");
   if (flags & Request::AUTOREMOVE)
      WriteOkay(Okay, output, "Autoremove: yes\n");
   if (flags & Request::UPGRADE_ALL)
   {
      WriteOkay(Okay, output, "Upgrade-All: yes\n");
      if (flags & (Request::FORBID_NEW_INSTALL | Request::FORBID_REMOVE))
	 WriteOkay(Okay, output, "Upgrade: yes\n");
      else
	 WriteOkay(Okay, output, "Dist-Upgrade: yes\n");
   }
   if (flags & Request::FORBID_NEW_INSTALL)
      WriteOkay(Okay, output, "Forbid-New-Install: yes\n");
   if (flags & Request::FORBID_REMOVE)
      WriteOkay(Okay, output, "Forbid-Remove: yes\n");
   auto const solver = _config->Find("APT::Solver", "internal");
   WriteOkay(Okay, output, "Solver: ", solver, "\n");
   if (_config->FindB("APT::Solver::Strict-Pinning", true) == false)
      WriteOkay(Okay, output, "Strict-Pinning: no\n");
   string solverpref("APT::Solver::");
   solverpref.append(solver).append("::Preferences");
   if (_config->Exists(solverpref) == true)
      WriteOkay(Okay, output, "Preferences: ", _config->Find(solverpref,""), "\n");
   return WriteOkay(Okay, output, "\n");
}
									/*}}}*/
// EDSP::ReadResponse - from the given file descriptor			/*{{{*/
bool EDSP::ReadResponse(int const input, pkgDepCache &Cache, OpProgress *Progress) {
	/* We build an map id to mmap offset here
	   In theory we could use the offset as ID, but then VersionCount
	   couldn't be used to create other versionmappings anymore and it
	   would be too easy for a (buggy) solver to segfault APT… */
	auto VersionCount = Cache.Head().VersionCount;
	decltype(VersionCount) VerIdx[VersionCount];
	for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; ++P) {
		for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
			VerIdx[V->ID] = V.Index();
		Cache[P].Marked = true;
		Cache[P].Garbage = false;
	}

	FileFd in;
	in.OpenDescriptor(input, FileFd::ReadOnly, true);
	pkgTagFile response(&in, 100);
	pkgTagSection section;

	std::set<decltype(Cache.PkgBegin()->ID)> seenOnce;
	while (response.Step(section) == true) {
		std::string type;
		if (section.Exists("Install") == true)
			type = "Install";
		else if (section.Exists("Remove") == true)
			type = "Remove";
		else if (section.Exists("Progress") == true) {
			if (Progress != NULL) {
				string msg = section.FindS("Message");
				if (msg.empty() == true)
					msg = _("Prepare for receiving solution");
				Progress->SubProgress(100, msg, section.FindI("Percentage", 0));
			}
			continue;
		} else if (section.Exists("Error") == true) {
			if (_error->PendingError()) {
				if (Progress != nullptr)
					Progress->Done();
				Progress = nullptr;
				_error->DumpErrors(std::cerr, GlobalError::DEBUG, false);
			}
			std::string msg = SubstVar(SubstVar(section.FindS("Message"), "\n .\n", "\n\n"), "\n ", "\n");
			if (msg.empty() == true) {
				msg = _("External solver failed without a proper error message");
				_error->Error("%s", msg.c_str());
			} else
				_error->Error("External solver failed with: %s", msg.substr(0,msg.find('\n')).c_str());
			if (Progress != nullptr)
				Progress->Done();
			std::cerr << "The solver encountered an error of type: " << section.FindS("Error") << std::endl;
			std::cerr << "The following information might help you to understand what is wrong:" << std::endl;
			std::cerr << msg << std::endl << std::endl;
			return false;
		} else if (section.Exists("Autoremove") == true)
			type = "Autoremove";
		else {
			char const *Start, *End;
			section.GetSection(Start, End);
			_error->Warning("Encountered an unexpected section with %d fields: %s", section.Count(), std::string(Start, End).c_str());
			continue;
		}

		decltype(VersionCount) const id = section.FindULL(type.c_str(), VersionCount);
		if (id == VersionCount) {
			_error->Warning("Unable to parse %s request with id value '%s'!", type.c_str(), section.FindS(type.c_str()).c_str());
			continue;
		} else if (id > VersionCount) {
			_error->Warning("ID value '%s' in %s request stanza is to high to refer to a known version!", section.FindS(type.c_str()).c_str(), type.c_str());
			continue;
		}

		pkgCache::VerIterator Ver(Cache.GetCache(), Cache.GetCache().VerP + VerIdx[id]);
		auto const Pkg = Ver.ParentPkg();
		if (type == "Autoremove") {
			Cache[Pkg].Marked = false;
			Cache[Pkg].Garbage = true;
		} else if (seenOnce.emplace(Pkg->ID).second == false) {
			_error->Warning("Ignoring %s stanza received for package %s which already had a previous stanza effecting it!", type.c_str(), Pkg.FullName(false).c_str());
		} else if (type == "Install") {
			if (Pkg.CurrentVer() == Ver) {
				_error->Warning("Ignoring Install stanza received for version %s of package %s which is already installed!",
				      Ver.VerStr(), Pkg.FullName(false).c_str());
			} else {
				Cache.SetCandidateVersion(Ver);
				Cache.MarkInstall(Pkg, false, 0, false);
			}
		} else if (type == "Remove") {
			if (Pkg->CurrentVer == 0)
				_error->Warning("Ignoring Remove stanza received for version %s of package %s which isn't installed!",
				      Ver.VerStr(), Pkg.FullName(false).c_str());
			else if (Pkg.CurrentVer() != Ver)
				_error->Warning("Ignoring Remove stanza received for version %s of package %s which isn't the installed version %s!",
				      Ver.VerStr(), Pkg.FullName(false).c_str(), Pkg.CurrentVer().VerStr());
			else
				Cache.MarkDelete(Ver.ParentPkg(), false);
		}
	}
	return true;
}
									/*}}}*/
// ReadLine - first line from the given file descriptor			/*{{{*/
// ---------------------------------------------------------------------
/* Little helper method to read a complete line into a string. Similar to
   fgets but we need to use the low-level read() here as otherwise the
   listparser will be confused later on as mixing of fgets and read isn't
   a supported action according to the manpages and results are undefined */
static bool ReadLine(int const input, std::string &line) {
	char one;
	ssize_t data = 0;
	line.erase();
	line.reserve(100);
	while ((data = read(input, &one, sizeof(one))) != -1) {
		if (data != 1)
			continue;
		if (one == '\n')
			return true;
		if (one == '\r')
			continue;
		if (line.empty() == true && isblank(one) != 0)
			continue;
		line += one;
	}
	return false;
}
									/*}}}*/
// StringToBool - convert yes/no to bool				/*{{{*/
// ---------------------------------------------------------------------
/* we are not as lazy as we are in the global StringToBool as we really
   only accept yes/no here */
static bool localStringToBool(std::string answer, bool const defValue) {
   std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
   if (answer == "yes")
      return true;
   else if (answer == "no")
      return false;
   else
      _error->Warning("Value '%s' is not a boolean 'yes' or 'no'!", answer.c_str());
   return defValue;
}
									/*}}}*/
static bool LineStartsWithAndStrip(std::string &line, APT::StringView const with)/*{{{*/
{
   if (line.compare(0, with.size(), with.data()) != 0)
      return false;
   line = APT::String::Strip(line.substr(with.length()));
   return true;
}
									/*}}}*/
static bool ReadFlag(unsigned int &flags, std::string &line, APT::StringView const name, unsigned int const setflag)/*{{{*/
{
   if (LineStartsWithAndStrip(line, name) == false)
      return false;
   if (localStringToBool(line, false))
      flags |= setflag;
   else
      flags &= ~setflag;
   return true;
}
									/*}}}*/
// EDSP::ReadRequest - first stanza from the given file descriptor	/*{{{*/
bool EDSP::ReadRequest(int const input, std::list<std::string> &install,
			std::list<std::string> &remove, unsigned int &flags)
{
   install.clear();
   remove.clear();
   flags = 0;
   std::string line;
   while (ReadLine(input, line) == true)
   {
      // Skip empty lines before request
      if (line.empty() == true)
	 continue;
      // The first Tag must be a request, so search for it
      if (LineStartsWithAndStrip(line, "Request:"))
	 continue;

      while (ReadLine(input, line) == true)
      {
	 // empty lines are the end of the request
	 if (line.empty() == true)
	    return true;

	 std::list<std::string> *request = NULL;
	 if (LineStartsWithAndStrip(line, "Install:"))
	    request = &install;
	 else if (LineStartsWithAndStrip(line, "Remove:"))
	    request = &remove;
	 else if (ReadFlag(flags, line, "Upgrade:", (Request::UPGRADE_ALL | Request::FORBID_REMOVE | Request::FORBID_NEW_INSTALL)) ||
	       ReadFlag(flags, line, "Dist-Upgrade:", Request::UPGRADE_ALL) ||
	       ReadFlag(flags, line, "Upgrade-All:", Request::UPGRADE_ALL) ||
	       ReadFlag(flags, line, "Forbid-New-Install:", Request::FORBID_NEW_INSTALL) ||
	       ReadFlag(flags, line, "Forbid-Remove:", Request::FORBID_REMOVE) ||
	       ReadFlag(flags, line, "Autoremove:", Request::AUTOREMOVE))
	    ;
	 else if (LineStartsWithAndStrip(line, "Architecture:"))
	    _config->Set("APT::Architecture", line);
	 else if (LineStartsWithAndStrip(line, "Architectures:"))
	    _config->Set("APT::Architectures", SubstVar(line, " ", ","));
	 else if (LineStartsWithAndStrip(line, "Machine-ID"))
	    _config->Set("APT::Machine-ID", line);
	 else if (LineStartsWithAndStrip(line, "Solver:"))
	    ; // purely informational line
	 else
	    _error->Warning("Unknown line in EDSP Request stanza: %s", line.c_str());

	 if (request == NULL)
	    continue;
	 auto const pkgs = VectorizeString(line, ' ');
	 std::move(pkgs.begin(), pkgs.end(), std::back_inserter(*request));
      }
   }
   return false;
}									/*}}}*/
// EDSP::ApplyRequest - first stanza from the given file descriptor	/*{{{*/
bool EDSP::ApplyRequest(std::list<std::string> const &install,
			 std::list<std::string> const &remove,
			 pkgDepCache &Cache)
{
	for (std::list<std::string>::const_iterator i = install.begin();
	     i != install.end(); ++i) {
		pkgCache::PkgIterator P = Cache.FindPkg(*i);
		if (P.end() == true)
			_error->Warning("Package %s is not known, so can't be installed", i->c_str());
		else
			Cache.MarkInstall(P, false);
	}

	for (std::list<std::string>::const_iterator i = remove.begin();
	     i != remove.end(); ++i) {
		pkgCache::PkgIterator P = Cache.FindPkg(*i);
		if (P.end() == true)
			_error->Warning("Package %s is not known, so can't be installed", i->c_str());
		else
			Cache.MarkDelete(P);
	}
	return true;
}
									/*}}}*/
// EDSP::WriteSolutionStanza - to the given file descriptor		/*{{{*/
bool EDSP::WriteSolutionStanza(FileFd &output, char const * const Type, pkgCache::VerIterator const &Ver)
{
   bool Okay = output.Failed() == false;
   WriteOkay(Okay, output, Type, ": ", _system->GetVersionMapping(Ver->ID));
   if (_config->FindB("Debug::EDSP::WriteSolution", false) == true)
      WriteOkay(Okay, output, "\nPackage: ", Ver.ParentPkg().FullName(), "\nVersion: ", Ver.VerStr());
   return WriteOkay(Okay, output, "\n\n");
}
									/*}}}*/
// EDSP::WriteProgess - pulse to the given file descriptor		/*{{{*/
bool EDSP::WriteProgress(unsigned short const percent, const char* const message, FileFd &output) {
	return WriteOkay(output, "Progress: ", TimeRFC1123(time(NULL), true), "\n",
	      "Percentage: ", percent, "\n",
	      "Message: ", message, "\n\n") && output.Flush();
}
									/*}}}*/
// EDSP::WriteError - format an error message to be send to file descriptor /*{{{*/
static std::string formatMessage(std::string const &msg)
{
	return SubstVar(SubstVar(APT::String::Strip(msg), "\n\n", "\n.\n"), "\n", "\n ");
}
bool EDSP::WriteError(char const * const uuid, std::string const &message, FileFd &output) {
	return WriteOkay(output, "Error: ", uuid, "\n",
	      "Message: ", formatMessage(message),
	      "\n\n");
}
									/*}}}*/
static std::string findExecutable(std::vector<std::string> const &dirs, char const * const binary) {/*{{{*/
	for (auto && dir : dirs) {
		std::string const file = flCombine(dir, binary);
		if (RealFileExists(file) == true)
			return file;
	}
	return "";
}
									/*}}}*/
static pid_t ExecuteExternal(char const* const type, char const * const binary, char const * const configdir, int * const solver_in, int * const solver_out) {/*{{{*/
	auto const solverDirs = _config->FindVector(configdir);
	auto const file = findExecutable(solverDirs, binary);
	std::string dumper;
	{
		dumper = findExecutable(solverDirs, "apt-dump-solver");
		if (dumper.empty())
			dumper = findExecutable(solverDirs, "dump");
	}

	if (file.empty() == true)
	{
		_error->Error("Can't call external %s '%s' as it is not in a configured directory!", type, binary);
		return 0;
	}
	int external[4] = {-1, -1, -1, -1};
	if (pipe(external) != 0 || pipe(external + 2) != 0)
	{
		_error->Errno("Resolve", "Can't create needed IPC pipes for EDSP");
		return 0;
	}
	for (int i = 0; i < 4; ++i)
		SetCloseExec(external[i], true);

	pid_t Solver = ExecFork();
	if (Solver == 0) {
		dup2(external[0], STDIN_FILENO);
		dup2(external[3], STDOUT_FILENO);
		auto const dumpfile = _config->FindFile((std::string("Dir::Log::") + type).c_str());
		auto const dumpdir = flNotFile(dumpfile);
		auto const runasuser = _config->Find(std::string("APT::") + type + "::" + binary + "::RunAsUser",
		      _config->Find(std::string("APT::") + type + "::RunAsUser",
			 _config->Find("APT::Sandbox::User")));
		if (dumper.empty() || dumpfile.empty() || dumper == file || CreateAPTDirectoryIfNeeded(dumpdir, dumpdir) == false)
		{
		   _config->Set("APT::Sandbox::User", runasuser);
		   DropPrivileges();
		   char const * const calling[] = { file.c_str(), nullptr };
		   execv(calling[0], const_cast<char**>(calling));
		}
		else
		{
		   char const * const calling[] = { dumper.c_str(), "--user", runasuser.c_str(), dumpfile.c_str(), file.c_str(), nullptr };
		   execv(calling[0], const_cast<char**>(calling));
		}
		std::cerr << "Failed to execute " << type << " '" << binary << "'!" << std::endl;
		_exit(100);
	}
	close(external[0]);
	close(external[3]);

	if (WaitFd(external[1], true, 5) == false)
	{
		_error->Errno("Resolve", "Timed out while Waiting on availability of %s stdin", type);
		return 0;
	}

	*solver_in = external[1];
	*solver_out = external[2];
	return Solver;
}
									/*}}}*/
// EDSP::ExecuteSolver - fork requested solver and setup ipc pipes	{{{*/
pid_t EDSP::ExecuteSolver(const char* const solver, int * const solver_in, int * const solver_out, bool) {
	return ExecuteExternal("solver", solver, "Dir::Bin::Solvers", solver_in, solver_out);
}
									/*}}}*/
static bool CreateDumpFile(char const * const id, char const * const type, FileFd &output)/*{{{*/
{
	auto const dumpfile = _config->FindFile((std::string("Dir::Log::") + type).c_str());
	if (dumpfile.empty())
		return false;
	auto const dumpdir = flNotFile(dumpfile);
	_error->PushToStack();
	bool errored_out = CreateAPTDirectoryIfNeeded(dumpdir, dumpdir) == false ||
	   output.Open(dumpfile, FileFd::WriteOnly | FileFd::Exclusive | FileFd::Create, FileFd::Extension, 0644) == false;
	std::vector<std::string> downgrademsgs;
	while (_error->empty() == false)
	{
		std::string msg;
		_error->PopMessage(msg);
		downgrademsgs.emplace_back(std::move(msg));
	}
	_error->RevertToStack();
	for (auto && msg : downgrademsgs)
	   _error->Warning("%s", msg.c_str());
	if (errored_out)
		return _error->WarningE(id, _("Could not open file '%s'"), dumpfile.c_str());
	return true;
}
									/*}}}*/
// EDSP::ResolveExternal - resolve problems by asking external for help	{{{*/
bool EDSP::ResolveExternal(const char* const solver, pkgDepCache &Cache,
			 unsigned int const flags, OpProgress *Progress) {
	if (strcmp(solver, "internal") == 0)
	{
		FileFd output;
		bool Okay = CreateDumpFile("EDSP::Resolve", "solver", output);
		Okay &= EDSP::WriteRequest(Cache, output, flags, nullptr);
		return Okay && EDSP::WriteScenario(Cache, output, nullptr);
	}
	_error->PushToStack();
	int solver_in, solver_out;
	pid_t const solver_pid = ExecuteSolver(solver, &solver_in, &solver_out, true);
	if (solver_pid == 0)
		return false;

	FileFd output;
	if (output.OpenDescriptor(solver_in, FileFd::WriteOnly | FileFd::BufferedWrite, true) == false)
		return _error->Errno("ResolveExternal", "Opening solver %s stdin on fd %d for writing failed", solver, solver_in);

	bool Okay = output.Failed() == false;
	if (Okay && Progress != NULL)
		Progress->OverallProgress(0, 100, 5, _("Execute external solver"));
	Okay &= EDSP::WriteRequest(Cache, output, flags, Progress);
	if (Okay && Progress != NULL)
		Progress->OverallProgress(5, 100, 20, _("Execute external solver"));
	Okay &= EDSP::WriteScenario(Cache, output, Progress);
	output.Close();

	if (Okay && Progress != NULL)
		Progress->OverallProgress(25, 100, 75, _("Execute external solver"));
	bool const ret = EDSP::ReadResponse(solver_out, Cache, Progress);
	_error->MergeWithStack();
	if (ExecWait(solver_pid, solver))
		return ret;
	return false;
}									/*}}}*/

bool EIPP::OrderInstall(char const * const solver, pkgPackageManager * const PM,	/*{{{*/
			 unsigned int const flags, OpProgress * const Progress)
{
   if (strcmp(solver, "internal") == 0)
   {
      FileFd output;
      _error->PushToStack();
      bool Okay = CreateDumpFile("EIPP::OrderInstall", "planner", output);
      if (Okay == false && dynamic_cast<pkgSimulate*>(PM) != nullptr)
      {
	 _error->RevertToStack();
	 return false;
      }
      _error->MergeWithStack();
      Okay &= EIPP::WriteRequest(PM->Cache, output, flags, nullptr);
      return Okay && EIPP::WriteScenario(PM->Cache, output, nullptr);
   }
   _error->PushToStack();
   int solver_in, solver_out;
   pid_t const solver_pid = ExecuteExternal("planner", solver, "Dir::Bin::Planners", &solver_in, &solver_out);
   if (solver_pid == 0)
      return false;

   FileFd output;
   if (output.OpenDescriptor(solver_in, FileFd::WriteOnly | FileFd::BufferedWrite, true) == false)
      return _error->Errno("EIPP::OrderInstall", "Opening planner %s stdin on fd %d for writing failed", solver, solver_in);

   bool Okay = output.Failed() == false;
   if (Okay && Progress != NULL)
      Progress->OverallProgress(0, 100, 5, _("Execute external planner"));
   Okay &= EIPP::WriteRequest(PM->Cache, output, flags, Progress);
   if (Okay && Progress != NULL)
      Progress->OverallProgress(5, 100, 20, _("Execute external planner"));
   Okay &= EIPP::WriteScenario(PM->Cache, output, Progress);
   output.Close();

   if (Okay)
   {
      if (Progress != nullptr)
	 Progress->OverallProgress(25, 100, 75, _("Execute external planner"));

      // we don't tell the external planners about boring things
      for (auto Pkg = PM->Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
      {
	 if (Pkg->CurrentState == pkgCache::State::ConfigFiles && PM->Cache[Pkg].Purge() == true)
	    PM->Remove(Pkg, true);
      }
   }
   bool const ret = EIPP::ReadResponse(solver_out, PM, Progress);
   _error->MergeWithStack();
   if (ExecWait(solver_pid, solver))
      return ret;
   return false;
}
									/*}}}*/
bool EIPP::WriteRequest(pkgDepCache &Cache, FileFd &output,		/*{{{*/
			unsigned int const flags,
			OpProgress * const Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().PackageCount, _("Send request to planner"));
   decltype(Cache.Head().PackageCount) p = 0;
   string del, inst, reinst;
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg, ++p)
   {
      if (Progress != NULL && p % 100 == 0)
         Progress->Progress(p);
      string* req;
      pkgDepCache::StateCache &P = Cache[Pkg];
      if (P.Purge() == true && Pkg->CurrentState == pkgCache::State::ConfigFiles)
	 continue;
      if (P.Delete() == true)
	 req = &del;
      else if (P.NewInstall() == true || P.Upgrade() == true || P.Downgrade() == true)
	 req = &inst;
      else if (P.ReInstall() == true)
	 req = &reinst;
      else
	 continue;
      req->append(" ").append(Pkg.FullName());
   }

   bool Okay = WriteGenericRequestHeaders(output, "Request: EIPP 0.1\n");
   if (del.empty() == false)
      WriteOkay(Okay, output, "Remove:", del, "\n");
   if (inst.empty() == false)
      WriteOkay(Okay, output, "Install:", inst, "\n");
   if (reinst.empty() == false)
      WriteOkay(Okay, output, "ReInstall:", reinst, "\n");
   WriteOkay(Okay, output, "Planner: ", _config->Find("APT::Planner", "internal"), "\n");
   if ((flags & Request::IMMEDIATE_CONFIGURATION_ALL) != 0)
      WriteOkay(Okay, output, "Immediate-Configuration: yes\n");
   else if ((flags & Request::NO_IMMEDIATE_CONFIGURATION) != 0)
      WriteOkay(Okay, output, "Immediate-Configuration: no\n");
   else if ((flags & Request::ALLOW_TEMPORARY_REMOVE_OF_ESSENTIALS) != 0)
      WriteOkay(Okay, output, "Allow-Temporary-Remove-of-Essentials: yes\n");
   return WriteOkay(Okay, output, "\n");
}
									/*}}}*/
static bool WriteScenarioEIPPVersion(pkgDepCache &, FileFd &output, pkgCache::PkgIterator const &Pkg,/*{{{*/
				pkgCache::VerIterator const &Ver)
{
   bool Okay = true;
   if (Pkg.CurrentVer() == Ver)
      switch (Pkg->CurrentState)
      {
	 case pkgCache::State::NotInstalled: WriteOkay(Okay, output, "\nStatus: not-installed"); break;
	 case pkgCache::State::ConfigFiles: WriteOkay(Okay, output, "\nStatus: config-files"); break;
	 case pkgCache::State::HalfInstalled: WriteOkay(Okay, output, "\nStatus: half-installed"); break;
	 case pkgCache::State::UnPacked: WriteOkay(Okay, output, "\nStatus: unpacked"); break;
	 case pkgCache::State::HalfConfigured: WriteOkay(Okay, output, "\nStatus: half-configured"); break;
	 case pkgCache::State::TriggersAwaited: WriteOkay(Okay, output, "\nStatus: triggers-awaited"); break;
	 case pkgCache::State::TriggersPending: WriteOkay(Okay, output, "\nStatus: triggers-pending"); break;
	 case pkgCache::State::Installed: WriteOkay(Okay, output, "\nStatus: installed"); break;
      }
   return Okay;
}
									/*}}}*/
// EIPP::WriteScenario - to the given file descriptor			/*{{{*/
template<typename forVersion> void forAllInterestingVersions(pkgDepCache &Cache, pkgCache::PkgIterator const &Pkg, forVersion const &func)
{
   if (Pkg->CurrentState == pkgCache::State::NotInstalled)
   {
      auto P = Cache[Pkg];
      if (P.Install() == false)
	 return;
      func(Pkg, P.InstVerIter(Cache));
   }
   else
   {
      if (Pkg->CurrentVer != 0)
	 func(Pkg, Pkg.CurrentVer());
      auto P = Cache[Pkg];
      auto const V = P.InstVerIter(Cache);
      if (P.Delete() == false && Pkg.CurrentVer() != V)
	 func(Pkg, V);
   }
}

bool EIPP::WriteScenario(pkgDepCache &Cache, FileFd &output, OpProgress * const Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().PackageCount, _("Send scenario to planner"));
   decltype(Cache.Head().PackageCount) p = 0;
   bool Okay = output.Failed() == false;
   std::vector<bool> pkgset(Cache.Head().PackageCount, false);
   auto const MarkVersion = [&](pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const &Ver) {
      pkgset[Pkg->ID] = true;
      for (auto D = Ver.DependsList(); D.end() == false; ++D)
      {
	 if (D.IsCritical() == false)
	    continue;
	 auto const P = D.TargetPkg();
	 for (auto Prv = P.ProvidesList(); Prv.end() == false; ++Prv)
	 {
	    auto const V = Prv.OwnerVer();
	    auto const PV = V.ParentPkg();
	    if (V == PV.CurrentVer() || V == Cache[PV].InstVerIter(Cache))
	       pkgset[PV->ID] = true;
	 }
	 pkgset[P->ID] = true;
	 if (strcmp(P.Arch(), "any") == 0)
	 {
	    APT::StringView const pkgname(P.Name());
	    auto const idxColon = pkgname.find(':');
	    if (idxColon != APT::StringView::npos)
	    {
	       pkgCache::PkgIterator PA;
	       if (pkgname.substr(idxColon + 1) == "any")
	       {
		  auto const GA = Cache.FindGrp(pkgname.substr(0, idxColon).to_string());
		  for (auto PA = GA.PackageList(); PA.end() == false; PA = GA.NextPkg(PA))
		  {
		     pkgset[PA->ID] = true;
		  }
	       }
	       else
	       {
		  auto const PA = Cache.FindPkg(pkgname.to_string());
		  if (PA.end() == false)
		     pkgset[PA->ID] = true;
	       }
	    }
	 }
	 else
	 {
	    auto const PA = Cache.FindPkg(P.FullName(false), "any");
	    if (PA.end() == false)
	       pkgset[PA->ID] = true;
	 }
      }
   };
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
      forAllInterestingVersions(Cache, Pkg, MarkVersion);
   auto const WriteVersion = [&](pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const &Ver) {
      Okay &= WriteScenarioVersion(output, Pkg, Ver);
      Okay &= WriteScenarioEIPPVersion(Cache, output, Pkg, Ver);
      Okay &= WriteScenarioLimitedDependency(output, Ver, pkgset, true);
      WriteOkay(Okay, output, "\n");
      if (Progress != NULL && p % 100 == 0)
	 Progress->Progress(p);
   };
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false && likely(Okay); ++Pkg, ++p)
   {
      if (pkgset[Pkg->ID] == false || Pkg->VersionList == 0)
	 continue;
      forAllInterestingVersions(Cache, Pkg, WriteVersion);
   }
   return Okay;
}
									/*}}}*/
// EIPP::ReadResponse - from the given file descriptor			/*{{{*/
bool EIPP::ReadResponse(int const input, pkgPackageManager * const PM, OpProgress *Progress) {
   /* We build an map id to mmap offset here
      In theory we could use the offset as ID, but then VersionCount
      couldn't be used to create other versionmappings anymore and it
      would be too easy for a (buggy) solver to segfault APT… */
   auto VersionCount = PM->Cache.Head().VersionCount;
   decltype(VersionCount) VerIdx[VersionCount];
   for (pkgCache::PkgIterator P = PM->Cache.PkgBegin(); P.end() == false; ++P) {
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
	 VerIdx[V->ID] = V.Index();
   }

   FileFd in;
   in.OpenDescriptor(input, FileFd::ReadOnly);
   pkgTagFile response(&in, 100);
   pkgTagSection section;

   while (response.Step(section) == true) {
      char const * type = nullptr;
      if (section.Exists("Progress") == true) {
	 if (Progress != NULL) {
	    string msg = section.FindS("Message");
	    if (msg.empty() == true)
	       msg = _("Prepare for receiving solution");
	    Progress->SubProgress(100, msg, section.FindI("Percentage", 0));
	 }
	 continue;
      } else if (section.Exists("Error") == true) {
	 if (_error->PendingError()) {
	    if (Progress != nullptr)
	       Progress->Done();
	    Progress = nullptr;
	    _error->DumpErrors(std::cerr, GlobalError::DEBUG, false);
	 }
	 std::string msg = SubstVar(SubstVar(section.FindS("Message"), "\n .\n", "\n\n"), "\n ", "\n");
	 if (msg.empty() == true) {
	    msg = _("External planner failed without a proper error message");
	    _error->Error("%s", msg.c_str());
	 } else
	    _error->Error("External planner failed with: %s", msg.substr(0,msg.find('\n')).c_str());
	 if (Progress != nullptr)
	    Progress->Done();
	 std::cerr << "The planner encountered an error of type: " << section.FindS("Error") << std::endl;
	 std::cerr << "The following information might help you to understand what is wrong:" << std::endl;
	 std::cerr << msg << std::endl << std::endl;
	 return false;
      } else if (section.Exists("Unpack") == true)
	 type = "Unpack";
      else if (section.Exists("Configure") == true)
	 type = "Configure";
      else if (section.Exists("Remove") == true)
	 type = "Remove";
      else {
	 char const *Start, *End;
	 section.GetSection(Start, End);
	 _error->Warning("Encountered an unexpected section with %d fields: %s", section.Count(), std::string(Start, End).c_str());
	 continue;
      }

      if (type == nullptr)
	 continue;
      decltype(VersionCount) const id = section.FindULL(type, VersionCount);
      if (id == VersionCount) {
	 _error->Warning("Unable to parse %s request with id value '%s'!", type, section.FindS(type).c_str());
	 continue;
      } else if (id > VersionCount) {
	 _error->Warning("ID value '%s' in %s request stanza is to high to refer to a known version!", section.FindS(type).c_str(), type);
	 continue;
      }

      pkgCache::VerIterator Ver(PM->Cache.GetCache(), PM->Cache.GetCache().VerP + VerIdx[id]);
      auto const Pkg = Ver.ParentPkg();
      if (strcmp(type, "Unpack") == 0)
	 PM->Install(Pkg, PM->FileNames[Pkg->ID]);
      else if (strcmp(type, "Configure") == 0)
	 PM->Configure(Pkg);
      else if (strcmp(type, "Remove") == 0)
	 PM->Remove(Pkg, PM->Cache[Pkg].Purge());
   }
   return in.Failed() == false;
}
									/*}}}*/
bool EIPP::ReadRequest(int const input, std::list<std::pair<std::string,PKG_ACTION>> &actions,/*{{{*/
      unsigned int &flags)
{
   actions.clear();
   flags = 0;
   std::string line;
   while (ReadLine(input, line) == true)
   {
      // Skip empty lines before request
      if (line.empty() == true)
	 continue;
      // The first Tag must be a request, so search for it
      if (line.compare(0, 8, "Request:") != 0)
	 continue;

      while (ReadLine(input, line) == true)
      {
	 // empty lines are the end of the request
	 if (line.empty() == true)
	    return true;

	 PKG_ACTION pkgact = PKG_ACTION::NOOP;
	 if (LineStartsWithAndStrip(line, "Install:"))
	    pkgact = PKG_ACTION::INSTALL;
	 else if (LineStartsWithAndStrip(line, "ReInstall:"))
	    pkgact = PKG_ACTION::REINSTALL;
	 else if (LineStartsWithAndStrip(line, "Remove:"))
	    pkgact = PKG_ACTION::REMOVE;
	 else if (LineStartsWithAndStrip(line, "Architecture:"))
	    _config->Set("APT::Architecture", line);
	 else if (LineStartsWithAndStrip(line, "Architectures:"))
	    _config->Set("APT::Architectures", SubstVar(line, " ", ","));
	 else if (LineStartsWithAndStrip(line, "Planner:"))
	    ; // purely informational line
	 else if (LineStartsWithAndStrip(line, "Immediate-Configuration:"))
	 {
	    if (localStringToBool(line, true))
	       flags |= Request::IMMEDIATE_CONFIGURATION_ALL;
	    else
	       flags |= Request::NO_IMMEDIATE_CONFIGURATION;
	 }
	 else if (ReadFlag(flags, line, "Allow-Temporary-Remove-of-Essentials:", Request::ALLOW_TEMPORARY_REMOVE_OF_ESSENTIALS))
	    ;
	 else
	    _error->Warning("Unknown line in EIPP Request stanza: %s", line.c_str());

	 if (pkgact == PKG_ACTION::NOOP)
	    continue;
	 for (auto && p: VectorizeString(line, ' '))
	    actions.emplace_back(std::move(p), pkgact);
      }
   }
   return false;
}
									/*}}}*/
bool EIPP::ApplyRequest(std::list<std::pair<std::string,PKG_ACTION>> &actions,/*{{{*/
	 pkgDepCache &Cache)
{
   for (auto Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      short versions = 0;
      for (auto Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
      {
	 ++versions;
	 if (Pkg.CurrentVer() == Ver)
	    continue;
	 Cache.SetCandidateVersion(Ver);
      }
      if (unlikely(versions > 2))
	 _error->Warning("Package %s has %d versions, but should have at most 2!", Pkg.FullName().c_str(), versions);
   }
   for (auto && a: actions)
   {
      pkgCache::PkgIterator P = Cache.FindPkg(a.first);
      if (P.end() == true)
      {
	 _error->Warning("Package %s is not known, so can't be acted on", a.first.c_str());
	 continue;
      }
      switch (a.second)
      {
	 case PKG_ACTION::NOOP:
	    _error->Warning("Package %s has NOOP as action?!?", a.first.c_str());
	    break;
	 case PKG_ACTION::INSTALL:
	    Cache.MarkInstall(P, false);
	    break;
	 case PKG_ACTION::REINSTALL:
	    Cache.MarkInstall(P, false);
	    Cache.SetReInstall(P, true);
	    break;
	 case PKG_ACTION::REMOVE:
	    Cache.MarkDelete(P);
	    break;
      }
   }
   return true;
}
									/*}}}*/
