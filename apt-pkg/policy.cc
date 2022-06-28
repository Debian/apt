   // -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Version Policy implementation

   This is just a really simple wrapper around pkgVersionMatch with
   some added goodies to manage the list of things..

   See man apt_preferences for what value means what.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/version.h>
#include <apt-pkg/versionmatch.h>

#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

constexpr short NEVER_PIN = std::numeric_limits<short>::min();

struct pkgPolicy::Private
{
   std::string machineID;
};

// Policy::Init - Startup and bind to a cache				/*{{{*/
// ---------------------------------------------------------------------
/* Set the defaults for operation. The default mode with no loaded policy
   file matches the V0 policy engine. */
pkgPolicy::pkgPolicy(pkgCache *Owner) : VerPins(nullptr),
					PFPriority(nullptr), Cache(Owner), d(new Private)
{
   if (Owner == 0)
      return;
   PFPriority = new signed short[Owner->Head().PackageFileCount];
   VerPins = new Pin[Owner->Head().VersionCount];

   auto VersionCount = Owner->Head().VersionCount;
   for (decltype(VersionCount) I = 0; I != VersionCount; ++I)
      VerPins[I].Type = pkgVersionMatch::None;

   // The config file has a master override.
   string DefRel = _config->Find("APT::Default-Release");
   if (DefRel.empty() == false)
   {
      bool found = false;
      // FIXME: make ExpressionMatches static to use it here easily
      pkgVersionMatch vm("", pkgVersionMatch::None);
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); ++F)
      {
	 if (vm.ExpressionMatches(DefRel, F.Archive()) ||
	     vm.ExpressionMatches(DefRel, F.Codename()) ||
	     vm.ExpressionMatches(DefRel, F.Version()) ||
	     (DefRel.length() > 2 && DefRel[1] == '='))
	    found = true;
      }
      if (found == false)
	 _error->Error(_("The value '%s' is invalid for APT::Default-Release as such a release is not available in the sources"), DefRel.c_str());
      else
	 CreatePin(pkgVersionMatch::Release,"",DefRel,990);
   }
   InitDefaults();

   d->machineID = APT::Configuration::getMachineID();
}
									/*}}}*/
// Policy::InitDefaults - Compute the default selections		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgPolicy::InitDefaults()
{
   // Initialize the priorities based on the status of the package file
   for (pkgCache::PkgFileIterator I = Cache->FileBegin(); I != Cache->FileEnd(); ++I)
   {
      PFPriority[I->ID] = 500;
      if (I.Flagged(pkgCache::Flag::NotSource))
	 PFPriority[I->ID] = 100;
      else if (I.Flagged(pkgCache::Flag::ButAutomaticUpgrades))
	 PFPriority[I->ID] = 100;
      else if (I.Flagged(pkgCache::Flag::NotAutomatic))
	 PFPriority[I->ID] = 1;
   }

   // Apply the defaults..
   std::unique_ptr<bool[]> Fixed(new bool[Cache->HeaderP->PackageFileCount]);
   memset(Fixed.get(),0,sizeof(Fixed[0])*Cache->HeaderP->PackageFileCount);
   StatusOverride = false;
   for (vector<Pin>::const_iterator I = Defaults.begin(); I != Defaults.end(); ++I)
   {
      pkgVersionMatch Match(I->Data,I->Type);
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); ++F)
      {
	 if ((Fixed[F->ID] == false || I->Priority == NEVER_PIN) && PFPriority[F->ID] != NEVER_PIN && Match.FileMatch(F) == true)
	 {
	    PFPriority[F->ID] = I->Priority;

	    if (PFPriority[F->ID] >= 1000)
	       StatusOverride = true;

	    Fixed[F->ID] = true;
	 }
      }
   }

   if (_config->FindB("Debug::pkgPolicy",false) == true)
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); ++F)
	 std::clog << "Prio of " << F.FileName() << ' ' << PFPriority[F->ID] << std::endl;

   return true;
}
									/*}}}*/
// Policy::GetCandidateVer - Get the candidate install version		/*{{{*/
// ---------------------------------------------------------------------
/* Evaluate the package pins and the default list to determine what the
   best package is. */
pkgCache::VerIterator pkgPolicy::GetCandidateVer(pkgCache::PkgIterator const &Pkg)
{
   pkgCache::VerIterator cand;
   pkgCache::VerIterator cur = Pkg.CurrentVer();
   int candPriority = -1;
   pkgVersioningSystem *vs = Cache->VS;

   for (pkgCache::VerIterator ver = Pkg.VersionList(); ver.end() == false; ++ver) {
      int priority = GetPriority(ver, true);

      if (priority == 0 || priority <= candPriority)
	 continue;

      // TODO: Maybe optimize to not compare versions
      if (!cur.end() && priority < 1000
	  && (vs->CmpVersion(ver.VerStr(), cur.VerStr()) < 0))
	 continue;

      candPriority = priority;
      cand = ver;
   }

   return cand;
}
									/*}}}*/
// Policy::CreatePin - Create an entry in the pin table..		/*{{{*/
// ---------------------------------------------------------------------
/* For performance we have 3 tables, the default table, the main cache
   table (hashed to the cache). A blank package name indicates the pin
   belongs to the default table. Order of insertion matters here, the
   earlier defaults override later ones. */
void pkgPolicy::CreatePin(pkgVersionMatch::MatchType Type,string Name,
			  string Data,signed short Priority)
{
   if (Name.empty() == true)
   {
      Pin *P = &*Defaults.insert(Defaults.end(),Pin());
      P->Type = Type;
      P->Priority = Priority;
      P->Data = Data;
      return;
   }

   bool IsSourcePin = APT::String::Startswith(Name, "src:");
   if (IsSourcePin) {
      Name = Name.substr(sizeof("src:") - 1);
   }

   size_t found = Name.rfind(':');
   string Arch;
   if (found != string::npos) {
      Arch = Name.substr(found+1);
      Name.erase(found);
   }

   // Allow pinning by wildcards - beware of package names looking like wildcards!
   // TODO: Maybe we should always prefer specific pins over non-specific ones.
   if ((Name[0] == '/' && Name[Name.length() - 1] == '/') || Name.find_first_of("*[?") != string::npos)
   {
      pkgVersionMatch match(Data, Type);
      for (pkgCache::GrpIterator G = Cache->GrpBegin(); G.end() != true; ++G)
	 if (Name != G.Name() && match.ExpressionMatches(Name, G.Name()))
	 {
	    auto NameToPinFor = IsSourcePin ? string("src:").append(G.Name()) : string(G.Name());
	    if (Arch.empty() == false)
	       CreatePin(Type, NameToPinFor.append(":").append(Arch), Data, Priority);
	    else
	       CreatePin(Type, NameToPinFor, Data, Priority);
	 }
      return;
   }

   // find the package (group) this pin applies to
   pkgCache::GrpIterator Grp = Cache->FindGrp(Name);
   bool matched = false;
   if (Grp.end() == false)
   {
      std::string MatchingArch;
      if (Arch.empty() == true)
	 MatchingArch = Cache->NativeArch();
      else
	 MatchingArch = Arch;
      APT::CacheFilter::PackageArchitectureMatchesSpecification pams(MatchingArch);

      if (IsSourcePin) {
	 for (pkgCache::VerIterator Ver = Grp.VersionsInSource(); not Ver.end(); Ver = Ver.NextInSource())
	 {
	    if (pams(Ver.ParentPkg().Arch()) == false)
	       continue;

	    PkgPin P(Ver.ParentPkg().FullName());
	    P.Type = Type;
	    P.Priority = Priority;
	    P.Data = Data;
	    // Find matching version(s) and copy the pin into it
	    pkgVersionMatch Match(P.Data,P.Type);
	    if (Match.VersionMatches(Ver)) {
	       Pin *VP = VerPins + Ver->ID;
	       if (VP->Type == pkgVersionMatch::None) {
		  *VP = P;
		   matched = true;
	       }
	    }
	 }
      } else {
	 for (pkgCache::PkgIterator Pkg = Grp.PackageList(); Pkg.end() != true; Pkg = Grp.NextPkg(Pkg))
	 {
	    if (pams(Pkg.Arch()) == false)
	       continue;

	    PkgPin P(Pkg.FullName());
	    P.Type = Type;
	    P.Priority = Priority;
	    P.Data = Data;

	    // Find matching version(s) and copy the pin into it
	    pkgVersionMatch Match(P.Data,P.Type);
	    for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() != true; ++Ver)
	    {
	       if (Match.VersionMatches(Ver)) {
		  Pin *VP = VerPins + Ver->ID;
		  if (VP->Type == pkgVersionMatch::None) {
		     *VP = P;
		      matched = true;
		  }
	       }
	    }
	 }
      }
   }

   if (matched == false)
   {
      PkgPin *P = &*Unmatched.insert(Unmatched.end(),PkgPin(Name));
      if (Arch.empty() == false)
	 P->Pkg.append(":").append(Arch);
      P->Type = Type;
      P->Priority = Priority;
      P->Data = Data;
      return;
   }
}
									/*}}}*/
// Policy::GetPriority - Get the priority of the package pin		/*{{{*/
// ---------------------------------------------------------------------
/* */
// Returns true if this update is excluded by phasing.
static inline bool ExcludePhased(std::string machineID, pkgCache::VerIterator const &Ver)
{
   if (Ver.PhasedUpdatePercentage() == 100)
      return false;

   // FIXME: We have migrated to a legacy implementation until LP: #1929082 is fixed
   if (not _config->FindB("APT::Get::Phase-Policy", false))
      return false;

   // The order and fallbacks for the always/never checks come from update-manager and exist
   // to preserve compatibility.
   if (_config->FindB("APT::Get::Always-Include-Phased-Updates",
		      _config->FindB("Update-Manager::Always-Include-Phased-Updates", false)))
      return false;

   if (_config->FindB("APT::Get::Never-Include-Phased-Updates",
		      _config->FindB("Update-Manager::Never-Include-Phased-Updates", false)))
      return true;

   if (machineID.empty()			 // no machine-id
       || getenv("SOURCE_DATE_EPOCH") != nullptr // reproducible build - always include
       || APT::Configuration::isChroot())
      return false;

   std::string seedStr = std::string(Ver.SourcePkgName()) + "-" + Ver.SourceVerStr() + "-" + machineID;
   std::seed_seq seed(seedStr.begin(), seedStr.end());
   std::minstd_rand rand(seed);
   std::uniform_int_distribution<unsigned int> dist(0, 100);

   return dist(rand) > Ver.PhasedUpdatePercentage();
}
APT_PURE signed short pkgPolicy::GetPriority(pkgCache::VerIterator const &Ver, bool ConsiderFiles)
{
   auto ceiling = std::numeric_limits<signed int>::max();
   if (ExcludePhased(d->machineID, Ver))
      ceiling = 1;
   if (VerPins[Ver->ID].Type != pkgVersionMatch::None)
   {
      // If all sources are never pins, the never pin wins.
      if (VerPins[Ver->ID].Priority == NEVER_PIN)
	 return NEVER_PIN;
      for (pkgCache::VerFileIterator file = Ver.FileList(); file.end() == false; file++)
	 if (GetPriority(file.File()) != NEVER_PIN)
	    return std::min((int)VerPins[Ver->ID].Priority, ceiling);
   }
   if (!ConsiderFiles)
      return std::min(0, ceiling);

   // priorities are short ints, but we want to pick a value outside the valid range here
   auto priority = std::numeric_limits<signed int>::min();
   for (pkgCache::VerFileIterator file = Ver.FileList(); file.end() == false; file++)
   {
      /* If this is the status file, and the current version is not the
         version in the status file (ie it is not installed, or somesuch)
         then it is not a candidate for installation, ever. This weeds
         out bogus entries that may be due to config-file states, or
         other. */
      if (file.File().Flagged(pkgCache::Flag::NotSource) && Ver.ParentPkg().CurrentVer() != Ver)
	 priority = std::max<decltype(priority)>(priority, -1);
      else
	 priority = std::max<decltype(priority)>(priority, GetPriority(file.File()));
   }

   return std::min(priority == std::numeric_limits<decltype(priority)>::min() ? 0 : priority, ceiling);
}
APT_PURE signed short pkgPolicy::GetPriority(pkgCache::PkgFileIterator const &File)
{
   return PFPriority[File->ID];
}
									/*}}}*/
// SetPriority - Directly set priority					/*{{{*/
// ---------------------------------------------------------------------
void pkgPolicy::SetPriority(pkgCache::VerIterator const &Ver, signed short Priority)
{
   Pin pin;
   pin.Data = "pkgPolicy::SetPriority";
   pin.Priority = Priority;
   VerPins[Ver->ID] = pin;
}
void pkgPolicy::SetPriority(pkgCache::PkgFileIterator const &File, signed short Priority)
{
   PFPriority[File->ID] = Priority;
}

									/*}}}*/
// ReadPinDir - Load the pin files from this dir into a Policy		/*{{{*/
// ---------------------------------------------------------------------
/* This will load each pin file in the given dir into a Policy. If the
   given dir is empty the dir set in Dir::Etc::PreferencesParts is used.
   Note also that this method will issue a warning if the dir does not
   exists but it will return true in this case! */
bool ReadPinDir(pkgPolicy &Plcy,string Dir)
{
   if (Dir.empty() == true)
      Dir = _config->FindDir("Dir::Etc::PreferencesParts", "/dev/null");

   if (DirectoryExists(Dir) == false)
   {
      if (APT::String::Endswith(Dir, "/dev/null") == false)
	 _error->WarningE("DirectoryExists",_("Unable to read %s"),Dir.c_str());
      return true;
   }

   _error->PushToStack();
   vector<string> const List = GetListOfFilesInDir(Dir, "pref", true, true);
   bool const PendingErrors = _error->PendingError();
   _error->MergeWithStack();
   if (PendingErrors)
      return false;

   // Read the files
   bool good = true;
   for (vector<string>::const_iterator I = List.begin(); I != List.end(); ++I)
      good = ReadPinFile(Plcy, *I) && good;
   return good;
}
									/*}}}*/
// ReadPinFile - Load the pin file into a Policy			/*{{{*/
// ---------------------------------------------------------------------
/* I'd like to see the preferences file store more than just pin information
   but right now that is the only stuff I have to store. Later there will
   have to be some kind of combined super parser to get the data into all
   the right classes.. */
bool ReadPinFile(pkgPolicy &Plcy,string File)
{
   if (File.empty() == true)
      File = _config->FindFile("Dir::Etc::Preferences");

   if (RealFileExists(File) == false)
      return true;

   FileFd Fd;
   if (OpenConfigurationFileFd(File, Fd) == false)
      return false;

   pkgTagFile TF(&Fd, pkgTagFile::SUPPORT_COMMENTS);
   if (Fd.IsOpen() == false || Fd.Failed())
      return false;

   pkgTagSection Tags;
   while (TF.Step(Tags) == true)
   {
      // can happen when there are only comments in a record
      if (Tags.Count() == 0)
         continue;

      string Name = Tags.FindS("Package");
      if (Name.empty() == true)
	 return _error->Error(_("Invalid record in the preferences file %s, no Package header"), File.c_str());
      if (Name == "*")
	 Name = string();
      
      const char *Start;
      const char *End;
      if (Tags.Find("Pin",Start,End) == false)
	 continue;
	 
      const char *Word = Start;
      for (; Word != End && isspace(*Word) == 0; Word++);

      // Parse the type..
      pkgVersionMatch::MatchType Type;
      if (stringcasecmp(Start,Word,"version") == 0 && Name.empty() == false)
	 Type = pkgVersionMatch::Version;
      else if (stringcasecmp(Start,Word,"release") == 0)
	 Type = pkgVersionMatch::Release;
      else if (stringcasecmp(Start,Word,"origin") == 0)
	 Type = pkgVersionMatch::Origin;
      else
      {
	 _error->Warning(_("Did not understand pin type %s"),string(Start,Word).c_str());
	 continue;
      }
      for (; Word != End && isspace(*Word) != 0; Word++);

      _error->PushToStack();
      std::string sPriority = Tags.FindS("Pin-Priority");
      int priority = sPriority == "never" ? NEVER_PIN : Tags.FindI("Pin-Priority", 0);
      bool const newError = _error->PendingError();
      _error->MergeWithStack();

      if (sPriority == "never" && not Name.empty())
	 return _error->Error(_("%s: The special 'Pin-Priority: %s' can only be used for 'Package: *' records"), File.c_str(), "never");

      // Silently clamp the never pin to never pin + 1
      if (priority == NEVER_PIN && sPriority != "never")
	 priority = NEVER_PIN + 1;
      if (priority < std::numeric_limits<short>::min() ||
          priority > std::numeric_limits<short>::max() ||
	  newError) {
	 return _error->Error(_("%s: Value %s is outside the range of valid pin priorities (%d to %d)"),
			      File.c_str(), Tags.FindS("Pin-Priority").c_str(),
			      std::numeric_limits<short>::min(),
			      std::numeric_limits<short>::max());
      }
      if (priority == 0)
      {
         return _error->Error(_("No priority (or zero) specified for pin"));
      }

      istringstream s(Name);
      string pkg;
      while(!s.eof())
      {
	 s >> pkg;
         Plcy.CreatePin(Type, pkg, string(Word,End),priority);
      };
   }

   Plcy.InitDefaults();
   return true;
}
									/*}}}*/

pkgPolicy::~pkgPolicy()
{
   delete[] PFPriority;
   delete[] VerPins;
   delete d;
}
