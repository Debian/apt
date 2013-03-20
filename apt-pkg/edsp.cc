// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/progress.h>

#include <limits>
#include <stdio.h>

#include <string>

#include <apti18n.h>
									/*}}}*/

using std::string;

// we could use pkgCache::DepType and ::Priority, but these would be localized strings…
const char * const EDSP::PrioMap[] = {0, "important", "required", "standard",
				      "optional", "extra"};
const char * const EDSP::DepMap[] = {"", "Depends", "Pre-Depends", "Suggests",
				     "Recommends" , "Conflicts", "Replaces",
				     "Obsoletes", "Breaks", "Enhances"};

// EDSP::WriteScenario - to the given file descriptor			/*{{{*/
bool EDSP::WriteScenario(pkgDepCache &Cache, FILE* output, OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().VersionCount, _("Send scenario to solver"));
   unsigned long p = 0;
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
      for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false; ++Ver, ++p)
      {
	 WriteScenarioVersion(Cache, output, Pkg, Ver);
	 WriteScenarioDependency(Cache, output, Pkg, Ver);
	 fprintf(output, "\n");
	 if (Progress != NULL && p % 100 == 0)
	    Progress->Progress(p);
      }
   return true;
}
									/*}}}*/
// EDSP::WriteLimitedScenario - to the given file descriptor		/*{{{*/
bool EDSP::WriteLimitedScenario(pkgDepCache &Cache, FILE* output,
				APT::PackageSet const &pkgset,
				OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().VersionCount, _("Send scenario to solver"));
   unsigned long p  = 0;
   for (APT::PackageSet::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg, ++p)
      for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
      {
	 WriteScenarioVersion(Cache, output, Pkg, Ver);
	 WriteScenarioLimitedDependency(Cache, output, Pkg, Ver, pkgset);
	 fprintf(output, "\n");
	 if (Progress != NULL && p % 100 == 0)
	    Progress->Progress(p);
      }
   if (Progress != NULL)
      Progress->Done();
   return true;
}
									/*}}}*/
// EDSP::WriteScenarioVersion						/*{{{*/
void EDSP::WriteScenarioVersion(pkgDepCache &Cache, FILE* output, pkgCache::PkgIterator const &Pkg,
				pkgCache::VerIterator const &Ver)
{
   fprintf(output, "Package: %s\n", Pkg.Name());
   fprintf(output, "Architecture: %s\n", Ver.Arch());
   fprintf(output, "Version: %s\n", Ver.VerStr());
   if (Pkg.CurrentVer() == Ver)
      fprintf(output, "Installed: yes\n");
   if (Pkg->SelectedState == pkgCache::State::Hold ||
       (Cache[Pkg].Keep() == true && Cache[Pkg].Protect() == true))
      fprintf(output, "Hold: yes\n");
   fprintf(output, "APT-ID: %d\n", Ver->ID);
   fprintf(output, "Priority: %s\n", PrioMap[Ver->Priority]);
   if ((Pkg->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
      fprintf(output, "Essential: yes\n");
   fprintf(output, "Section: %s\n", Ver.Section());
   if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
      fprintf(output, "Multi-Arch: allowed\n");
   else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
      fprintf(output, "Multi-Arch: foreign\n");
   else if ((Ver->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
      fprintf(output, "Multi-Arch: same\n");
   signed short Pin = std::numeric_limits<signed short>::min();
   for (pkgCache::VerFileIterator File = Ver.FileList(); File.end() == false; ++File) {
      signed short const p = Cache.GetPolicy().GetPriority(File.File());
      if (Pin < p)
	 Pin = p;
   }
   fprintf(output, "APT-Pin: %d\n", Pin);
   if (Cache.GetCandidateVer(Pkg) == Ver)
      fprintf(output, "APT-Candidate: yes\n");
   if ((Cache[Pkg].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
      fprintf(output, "APT-Automatic: yes\n");
}
									/*}}}*/
// EDSP::WriteScenarioDependency					/*{{{*/
void EDSP::WriteScenarioDependency(pkgDepCache &Cache, FILE* output, pkgCache::PkgIterator const &Pkg,
				pkgCache::VerIterator const &Ver)
{
   std::string dependencies[pkgCache::Dep::Enhances + 1];
   bool orGroup = false;
   for (pkgCache::DepIterator Dep = Ver.DependsList(); Dep.end() == false; ++Dep)
   {
      if (Dep.IsMultiArchImplicit() == true)
	 continue;
      if (orGroup == false)
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
   for (int i = 1; i < pkgCache::Dep::Enhances + 1; ++i)
      if (dependencies[i].empty() == false)
	 fprintf(output, "%s: %s\n", DepMap[i], dependencies[i].c_str()+2);
   string provides;
   for (pkgCache::PrvIterator Prv = Ver.ProvidesList(); Prv.end() == false; ++Prv)
   {
      if (Prv.IsMultiArchImplicit() == true)
	 continue;
      provides.append(", ").append(Prv.Name());
   }
   if (provides.empty() == false)
      fprintf(output, "Provides: %s\n", provides.c_str()+2);
}
									/*}}}*/
// EDSP::WriteScenarioLimitedDependency					/*{{{*/
void EDSP::WriteScenarioLimitedDependency(pkgDepCache &Cache, FILE* output,
					  pkgCache::PkgIterator const &Pkg,
					  pkgCache::VerIterator const &Ver,
					  APT::PackageSet const &pkgset)
{
   std::string dependencies[pkgCache::Dep::Enhances + 1];
   bool orGroup = false;
   for (pkgCache::DepIterator Dep = Ver.DependsList(); Dep.end() == false; ++Dep)
   {
      if (Dep.IsMultiArchImplicit() == true)
	 continue;
      if (orGroup == false)
      {
	 if (pkgset.find(Dep.TargetPkg()) == pkgset.end())
	    continue;
	 dependencies[Dep->Type].append(", ");
      }
      else if (pkgset.find(Dep.TargetPkg()) == pkgset.end())
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
   for (int i = 1; i < pkgCache::Dep::Enhances + 1; ++i)
      if (dependencies[i].empty() == false)
	 fprintf(output, "%s: %s\n", DepMap[i], dependencies[i].c_str()+2);
   string provides;
   for (pkgCache::PrvIterator Prv = Ver.ProvidesList(); Prv.end() == false; ++Prv)
   {
      if (Prv.IsMultiArchImplicit() == true)
	 continue;
      if (pkgset.find(Prv.ParentPkg()) == pkgset.end())
	 continue;
      provides.append(", ").append(Prv.Name());
   }
   if (provides.empty() == false)
      fprintf(output, "Provides: %s\n", provides.c_str()+2);
}
									/*}}}*/
// EDSP::WriteRequest - to the given file descriptor			/*{{{*/
bool EDSP::WriteRequest(pkgDepCache &Cache, FILE* output, bool const Upgrade,
			bool const DistUpgrade, bool const AutoRemove,
			OpProgress *Progress)
{
   if (Progress != NULL)
      Progress->SubProgress(Cache.Head().PackageCount, _("Send request to solver"));
   unsigned long p = 0;
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
   fprintf(output, "Request: EDSP 0.4\n");
   if (del.empty() == false)
      fprintf(output, "Remove: %s\n", del.c_str()+1);
   if (inst.empty() == false)
      fprintf(output, "Install: %s\n", inst.c_str()+1);
   if (Upgrade == true)
      fprintf(output, "Upgrade: yes\n");
   if (DistUpgrade == true)
      fprintf(output, "Dist-Upgrade: yes\n");
   if (AutoRemove == true)
      fprintf(output, "Autoremove: yes\n");
   if (_config->FindB("APT::Solver::Strict-Pinning", true) == false)
      fprintf(output, "Strict-Pinning: no\n");
   string solverpref("APT::Solver::");
   solverpref.append(_config->Find("APT::Solver", "internal")).append("::Preferences");
   if (_config->Exists(solverpref) == true)
      fprintf(output, "Preferences: %s\n", _config->Find(solverpref,"").c_str());
   fprintf(output, "\n");

   return true;
}
									/*}}}*/
// EDSP::ReadResponse - from the given file descriptor			/*{{{*/
bool EDSP::ReadResponse(int const input, pkgDepCache &Cache, OpProgress *Progress) {
	/* We build an map id to mmap offset here
	   In theory we could use the offset as ID, but then VersionCount
	   couldn't be used to create other versionmappings anymore and it
	   would be too easy for a (buggy) solver to segfault APT… */
	unsigned long long const VersionCount = Cache.Head().VersionCount;
	unsigned long VerIdx[VersionCount];
	for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; ++P) {
		for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
			VerIdx[V->ID] = V.Index();
		Cache[P].Marked = true;
		Cache[P].Garbage = false;
	}

	FileFd in;
	in.OpenDescriptor(input, FileFd::ReadOnly);
	pkgTagFile response(&in, 100);
	pkgTagSection section;

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
			std::string msg = SubstVar(SubstVar(section.FindS("Message"), "\n .\n", "\n\n"), "\n ", "\n");
			if (msg.empty() == true) {
				msg = _("External solver failed without a proper error message");
				_error->Error("%s", msg.c_str());
			} else
				_error->Error("External solver failed with: %s", msg.substr(0,msg.find('\n')).c_str());
			if (Progress != NULL)
				Progress->Done();
			std::cerr << "The solver encountered an error of type: " << section.FindS("Error") << std::endl;
			std::cerr << "The following information might help you to understand what is wrong:" << std::endl;
			std::cerr << msg << std::endl << std::endl;
			return false;
		} else if (section.Exists("Autoremove") == true)
			type = "Autoremove";
		else
			continue;

		size_t const id = section.FindULL(type.c_str(), VersionCount);
		if (id == VersionCount) {
			_error->Warning("Unable to parse %s request with id value '%s'!", type.c_str(), section.FindS(type.c_str()).c_str());
			continue;
		} else if (id > Cache.Head().VersionCount) {
			_error->Warning("ID value '%s' in %s request stanza is to high to refer to a known version!", section.FindS(type.c_str()).c_str(), type.c_str());
			continue;
		}

		pkgCache::VerIterator Ver(Cache.GetCache(), Cache.GetCache().VerP + VerIdx[id]);
		Cache.SetCandidateVersion(Ver);
		if (type == "Install")
			Cache.MarkInstall(Ver.ParentPkg(), false, 0, false);
		else if (type == "Remove")
			Cache.MarkDelete(Ver.ParentPkg(), false);
		else if (type == "Autoremove") {
			Cache[Ver.ParentPkg()].Marked = false;
			Cache[Ver.ParentPkg()].Garbage = true;
		}
	}
	return true;
}
									/*}}}*/
// EDSP::ReadLine - first line from the given file descriptor		/*{{{*/
// ---------------------------------------------------------------------
/* Little helper method to read a complete line into a string. Similar to
   fgets but we need to use the low-level read() here as otherwise the
   listparser will be confused later on as mixing of fgets and read isn't
   a supported action according to the manpages and results are undefined */
bool EDSP::ReadLine(int const input, std::string &line) {
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
// EDSP::StringToBool - convert yes/no to bool				/*{{{*/
// ---------------------------------------------------------------------
/* we are not as lazy as we are in the global StringToBool as we really
   only accept yes/no here - but we will ignore leading spaces */
bool EDSP::StringToBool(char const *answer, bool const defValue) {
   for (; isspace(*answer) != 0; ++answer);
   if (strncasecmp(answer, "yes", 3) == 0)
      return true;
   else if (strncasecmp(answer, "no", 2) == 0)
      return false;
   else
      _error->Warning("Value '%s' is not a boolean 'yes' or 'no'!", answer);
   return defValue;
}
									/*}}}*/
// EDSP::ReadRequest - first stanza from the given file descriptor	/*{{{*/
bool EDSP::ReadRequest(int const input, std::list<std::string> &install,
			std::list<std::string> &remove, bool &upgrade,
			bool &distUpgrade, bool &autoRemove)
{
   install.clear();
   remove.clear();
   upgrade = false;
   distUpgrade = false;
   autoRemove = false;
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

	 std::list<std::string> *request = NULL;
	 if (line.compare(0, 8, "Install:") == 0)
	 {
	    line.erase(0, 8);
	    request = &install;
	 }
	 else if (line.compare(0, 7, "Remove:") == 0)
	 {
	    line.erase(0, 7);
	    request = &remove;
	 }
	 else if (line.compare(0, 8, "Upgrade:") == 0)
	    upgrade = EDSP::StringToBool(line.c_str() + 9, false);
	 else if (line.compare(0, 13, "Dist-Upgrade:") == 0)
	    distUpgrade = EDSP::StringToBool(line.c_str() + 14, false);
	 else if (line.compare(0, 11, "Autoremove:") == 0)
	    autoRemove = EDSP::StringToBool(line.c_str() + 12, false);
	 else
	    _error->Warning("Unknown line in EDSP Request stanza: %s", line.c_str());

	 if (request == NULL)
	    continue;
	 size_t end = line.length();
	 do {
	    size_t begin = line.rfind(' ');
	    if (begin == std::string::npos)
	    {
	       request->push_back(line.substr(0, end));
	       break;
	    }
	    else if (begin < end)
	       request->push_back(line.substr(begin + 1, end));
	    line.erase(begin);
	    end = line.find_last_not_of(' ');
	 } while (end != std::string::npos);
      }
   }
   return false;
}
									/*}}}*/
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
// EDSP::WriteSolution - to the given file descriptor			/*{{{*/
bool EDSP::WriteSolution(pkgDepCache &Cache, FILE* output)
{
   bool const Debug = _config->FindB("Debug::EDSP::WriteSolution", false);
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      if (Cache[Pkg].Delete() == true)
      {
	 fprintf(output, "Remove: %d\n", Pkg.CurrentVer()->ID);
	 if (Debug == true)
	    fprintf(output, "Package: %s\nVersion: %s\n", Pkg.FullName().c_str(), Pkg.CurrentVer().VerStr());
      }
      else if (Cache[Pkg].NewInstall() == true || Cache[Pkg].Upgrade() == true)
      {
	 fprintf(output, "Install: %d\n", Cache.GetCandidateVer(Pkg)->ID);
	 if (Debug == true)
	    fprintf(output, "Package: %s\nVersion: %s\n", Pkg.FullName().c_str(), Cache.GetCandidateVer(Pkg).VerStr());
      }
      else if (Cache[Pkg].Garbage == true)
      {
	 fprintf(output, "Autoremove: %d\n", Pkg.CurrentVer()->ID);
	 if (Debug == true)
	    fprintf(output, "Package: %s\nVersion: %s\n", Pkg.FullName().c_str(), Pkg.CurrentVer().VerStr());
      }
      else
	 continue;
      fprintf(output, "\n");
   }

   return true;
}
									/*}}}*/
// EDSP::WriteProgess - pulse to the given file descriptor		/*{{{*/
bool EDSP::WriteProgress(unsigned short const percent, const char* const message, FILE* output) {
	fprintf(output, "Progress: %s\n", TimeRFC1123(time(NULL)).c_str());
	fprintf(output, "Percentage: %d\n", percent);
	fprintf(output, "Message: %s\n\n", message);
	fflush(output);
	return true;
}
									/*}}}*/
// EDSP::WriteError - format an error message to be send to file descriptor /*{{{*/
bool EDSP::WriteError(char const * const uuid, std::string const &message, FILE* output) {
	fprintf(output, "Error: %s\n", uuid);
	fprintf(output, "Message: %s\n\n", SubstVar(SubstVar(message, "\n\n", "\n.\n"), "\n", "\n ").c_str());
	return true;
}
									/*}}}*/
// EDSP::ExecuteSolver - fork requested solver and setup ipc pipes	{{{*/
bool EDSP::ExecuteSolver(const char* const solver, int *solver_in, int *solver_out) {
	std::vector<std::string> const solverDirs = _config->FindVector("Dir::Bin::Solvers");
	std::string file;
	for (std::vector<std::string>::const_iterator dir = solverDirs.begin();
	     dir != solverDirs.end(); ++dir) {
		file = flCombine(*dir, solver);
		if (RealFileExists(file.c_str()) == true)
			break;
		file.clear();
	}

	if (file.empty() == true)
		return _error->Error("Can't call external solver '%s' as it is not in a configured directory!", solver);
	int external[4] = {-1, -1, -1, -1};
	if (pipe(external) != 0 || pipe(external + 2) != 0)
		return _error->Errno("Resolve", "Can't create needed IPC pipes for EDSP");
	for (int i = 0; i < 4; ++i)
		SetCloseExec(external[i], true);

	pid_t Solver = ExecFork();
	if (Solver == 0) {
		dup2(external[0], STDIN_FILENO);
		dup2(external[3], STDOUT_FILENO);
		const char* calling[2] = { file.c_str(), 0 };
		execv(calling[0], (char**) calling);
		std::cerr << "Failed to execute solver '" << solver << "'!" << std::endl;
		_exit(100);
	}
	close(external[0]);
	close(external[3]);

	if (WaitFd(external[1], true, 5) == false)
		return _error->Errno("Resolve", "Timed out while Waiting on availability of solver stdin");

	*solver_in = external[1];
	*solver_out = external[2];
	return true;
}
									/*}}}*/
// EDSP::ResolveExternal - resolve problems by asking external for help	{{{*/
bool EDSP::ResolveExternal(const char* const solver, pkgDepCache &Cache,
			 bool const upgrade, bool const distUpgrade,
			 bool const autoRemove, OpProgress *Progress) {
	int solver_in, solver_out;
	if (EDSP::ExecuteSolver(solver, &solver_in, &solver_out) == false)
		return false;

	FILE* output = fdopen(solver_in, "w");
	if (output == NULL)
		return _error->Errno("Resolve", "fdopen on solver stdin failed");

	if (Progress != NULL)
		Progress->OverallProgress(0, 100, 5, _("Execute external solver"));
	EDSP::WriteRequest(Cache, output, upgrade, distUpgrade, autoRemove, Progress);
	if (Progress != NULL)
		Progress->OverallProgress(5, 100, 20, _("Execute external solver"));
	EDSP::WriteScenario(Cache, output, Progress);
	fclose(output);

	if (Progress != NULL)
		Progress->OverallProgress(25, 100, 75, _("Execute external solver"));
	if (EDSP::ReadResponse(solver_out, Cache, Progress) == false)
		return false;

	return true;
}
									/*}}}*/
