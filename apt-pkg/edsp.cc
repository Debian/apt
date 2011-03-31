// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>

#include <apti18n.h>
#include <limits>

#include <stdio.h>
									/*}}}*/

// EDSP::WriteScenario - to the given file descriptor			/*{{{*/
bool EDSP::WriteScenario(pkgDepCache &Cache, FILE* output)
{
   // we could use pkgCache::DepType and ::Priority, but these would be lokalized strings…
   const char * const PrioMap[] = {0, "important", "required", "standard",
				   "optional", "extra"};
   const char * const DepMap[] = {"", "Depends", "PreDepends", "Suggests",
				  "Recommends" , "Conflicts", "Replaces",
				  "Obsoletes", "Breaks", "Enhances"};

   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
      {
	 fprintf(output, "Package: %s\n", Pkg.Name());
	 fprintf(output, "Architecture: %s\n", Ver.Arch());
	 fprintf(output, "Version: %s\n", Ver.VerStr());
	 if (Pkg.CurrentVer() == Ver)
	    fprintf(output, "Installed: yes\n");
	 if (Pkg->SelectedState == pkgCache::State::Hold)
	    fprintf(output, "Hold: yes\n");
	 fprintf(output, "APT-ID: %u\n", Ver->ID);
	 fprintf(output, "Priority: %s\n", PrioMap[Ver->Priority]);
	 if ((Pkg->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    fprintf(output, "Essential: yes\n");
	 fprintf(output, "Section: %s\n", Ver.Section());
	 if (Ver->MultiArch == pkgCache::Version::Allowed || Ver->MultiArch == pkgCache::Version::AllAllowed)
	    fprintf(output, "Multi-Arch: allowed\n");
	 else if (Ver->MultiArch == pkgCache::Version::Foreign || Ver->MultiArch == pkgCache::Version::AllForeign)
	    fprintf(output, "Multi-Arch: foreign\n");
	 else if (Ver->MultiArch == pkgCache::Version::Same)
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
	 std::string dependencies[pkgCache::Dep::Enhances + 1];
	 bool orGroup = false;
	 for (pkgCache::DepIterator Dep = Ver.DependsList(); Dep.end() == false; ++Dep)
	 {
	    // Ignore implicit dependencies for multiarch here
	    if (strcmp(Pkg.Arch(), Dep.TargetPkg().Arch()) != 0)
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
	    // Ignore implicit provides for multiarch here
	    if (strcmp(Pkg.Arch(), Prv.ParentPkg().Arch()) != 0 || strcmp(Pkg.Name(),Prv.Name()) == 0)
	       continue;
	    provides.append(", ").append(Prv.Name());
	 }
	 if (provides.empty() == false)
	    fprintf(output, "Provides: %s\n", provides.c_str()+2);


	 fprintf(output, "\n");
      }
   }
   return true;
}
									/*}}}*/
// EDSP::WriteRequest - to the given file descriptor			/*{{{*/
bool EDSP::WriteRequest(pkgDepCache &Cache, FILE* output)
{
   string del, inst, upgrade;
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      string* req;
      if (Cache[Pkg].Delete() == true)
	 req = &del;
      else if (Cache[Pkg].NewInstall() == true)
	 req = &inst;
      else if (Cache[Pkg].Upgrade() == true)
	 req = &upgrade;
      else
	 continue;
      req->append(", ").append(Pkg.FullName());
   }
   if (del.empty() == false)
      fprintf(output, "Remove: %s\n", del.c_str()+2);
   if (inst.empty() == false)
      fprintf(output, "Install: %s\n", inst.c_str()+2);
   if (upgrade.empty() == false)
      fprintf(output, "Upgrade: %s\n", upgrade.c_str()+2);

   return true;
}
									/*}}}*/
bool EDSP::ReadResponse(FILE* input, pkgDepCache &Cache) { return false; }

bool EDSP::ReadRequest(FILE* input, std::list<std::string> &install,
			std::list<std::string> &remove)
{ return false; }
bool EDSP::ApplyRequest(std::list<std::string> const &install,
			 std::list<std::string> const &remove,
			 pkgDepCache &Cache)
{ return false; }
// EDSP::WriteSolution - to the given file descriptor			/*{{{*/
bool EDSP::WriteSolution(pkgDepCache &Cache, FILE* output)
{
   bool const Debug = _config->FindB("Debug::EDSPWriter::WriteSolution", false);
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      if (Cache[Pkg].Delete() == true)
	 fprintf(output, "Remove: %d\n", Cache.GetCandidateVer(Pkg)->ID);
      else if (Cache[Pkg].NewInstall() == true || Cache[Pkg].Upgrade() == true)
	 fprintf(output, "Install: %d\n", Cache.GetCandidateVer(Pkg)->ID);
      else
	 continue;
      if (Debug == true)
	 fprintf(output, "Package: %s\nVersion: %s\n", Pkg.FullName().c_str(), Cache.GetCandidateVer(Pkg).VerStr());
      fprintf(output, "\n");
   }

   return true;
}
									/*}}}*/
bool EDSP::WriteError(std::string const &message, FILE* output) { return false; }
