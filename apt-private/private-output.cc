// Include files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-output.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <langinfo.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <sstream>

#include <apti18n.h>
									/*}}}*/

using namespace std;
using APT::Configuration::color;

std::ostream c0out(0);
std::ostream c1out(0);
std::ostream c2out(0);
std::ofstream devnull("/dev/null");


unsigned int ScreenWidth = 80 - 1; /* - 1 for the cursor */

// SigWinch - Window size change signal handler				/*{{{*/
// ---------------------------------------------------------------------
/* */
static void SigWinch(int)
{
   // Riped from GNU ls
#ifdef TIOCGWINSZ
   struct winsize ws;
  
   if (ioctl(1, TIOCGWINSZ, &ws) != -1 && ws.ws_col >= 5)
      ScreenWidth = ws.ws_col - 1;
#endif
}
									/*}}}*/
bool InitOutput(std::basic_streambuf<char> * const out)			/*{{{*/
{
   if (!isatty(STDOUT_FILENO) && _config->FindI("quiet", -1) == -1)
      _config->Set("quiet","1");

   c0out.rdbuf(out);
   c1out.rdbuf(out);
   c2out.rdbuf(out);
   if (_config->FindI("quiet",0) > 0)
      c0out.rdbuf(devnull.rdbuf());
   if (_config->FindI("quiet",0) > 1)
      c1out.rdbuf(devnull.rdbuf());

   // deal with window size changes
   auto cols = getenv("COLUMNS");
   if (cols != nullptr)
   {
      char * colends;
      auto const sw = strtoul(cols, &colends, 10);
      if (*colends != '\0' || sw == 0)
      {
	 _error->Warning("Environment variable COLUMNS was ignored as it has an invalid value: \"%s\"", cols);
	 cols = nullptr;
      }
      else
	 ScreenWidth = sw;
   }
   if (cols == nullptr)
   {
      signal(SIGWINCH,SigWinch);
      SigWinch(0);
   }

   if (isatty(STDOUT_FILENO) == 0 || not _config->FindB("APT::Color", true) || getenv("NO_COLOR") != nullptr || getenv("APT_NO_COLOR") != nullptr)
   {
      _config->Set("APT::Color", false);
      _config->Set("APT::Color::Highlight", "");
      _config->Set("APT::Color::Neutral", "");
   } else {
      // Colors
      _config->CndSet("APT::Color::Highlight", "\x1B[32m");
      _config->CndSet("APT::Color::Bold", "\x1B[1m");
      _config->CndSet("APT::Color::Neutral", "\x1B[0m");
      
      _config->CndSet("APT::Color::Red", "\x1B[31m");
      _config->CndSet("APT::Color::Green", "\x1B[32m");
      _config->CndSet("APT::Color::Yellow", "\x1B[33m");
      _config->CndSet("APT::Color::Blue", "\x1B[34m");
      _config->CndSet("APT::Color::Magenta", "\x1B[35m");
      _config->CndSet("APT::Color::Cyan", "\x1B[36m");
      _config->CndSet("APT::Color::White", "\x1B[37m");

      _config->CndSet("APT::Color::Action::Upgrade", "green");
      _config->CndSet("APT::Color::Action::Install", "green");
      _config->CndSet("APT::Color::Action::Install-Dependencies", "green");
      _config->CndSet("APT::Color::Action::Downgrade", "yellow");
      _config->CndSet("APT::Color::Action::Remove", "red");
      _config->CndSet("APT::Color::Show::Field", "\x1B[1m");
      _config->CndSet("APT::Color::Show::Package", "\x1B[32m");
   }

   return true;
}
									/*}}}*/
static std::string GetArchiveSuite(pkgCacheFile &/*CacheFile*/, pkgCache::VerIterator ver) /*{{{*/
{
   std::string suite = "";
   if (ver && ver.FileList())
   {
      pkgCache::VerFileIterator VF = ver.FileList();
      for (; VF.end() == false ; ++VF)
      {
         if(VF.File() == NULL || VF.File().Archive() == NULL)
            suite = suite + "," + _("unknown");
         else
            suite = suite + "," + VF.File().Archive();
         //suite = VF.File().Archive();
      }
      suite = suite.erase(0, 1);
   }
   return suite;
}
									/*}}}*/
static std::string GetFlagsStr(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   pkgDepCache::StateCache &state = (*DepCache)[P];

   std::string flags_str;
   if (state.NowBroken())
      flags_str = "B";
   if (P.CurrentVer() && state.Upgradable() && state.CandidateVer != NULL)
      flags_str = "g";
   else if (P.CurrentVer() != NULL)
      flags_str = "i";
   else
      flags_str = "-";
   return flags_str;
}
									/*}}}*/
static std::string GetCandidateVersion(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgPolicy *policy = CacheFile.GetPolicy();
   pkgCache::VerIterator cand = policy->GetCandidateVer(P);

   return cand ? cand.VerStr() : "(none)";
}
									/*}}}*/
static std::string GetInstalledVersion(pkgCacheFile &/*CacheFile*/, pkgCache::PkgIterator P)/*{{{*/
{
   pkgCache::VerIterator inst = P.CurrentVer();

   return inst ? inst.VerStr() : "(none)";
}
									/*}}}*/
static std::string GetVersion(pkgCacheFile &/*CacheFile*/, pkgCache::VerIterator V)/*{{{*/
{
   pkgCache::PkgIterator P = V.ParentPkg();
   if (V == P.CurrentVer())
   {
      std::string inst_str = DeNull(V.VerStr());
#if 0 // FIXME: do we want this or something like this?
      pkgDepCache *DepCache = CacheFile.GetDepCache();
      pkgDepCache::StateCache &state = (*DepCache)[P];
      if (state.Upgradable())
         return "**"+inst_str;
#endif
      return inst_str;
   }

   if(V)
      return DeNull(V.VerStr());
   return "(none)";
}
									/*}}}*/
static std::string GetArchitecture(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   if (P->CurrentVer == 0)
   {
      pkgDepCache * const DepCache = CacheFile.GetDepCache();
      pkgDepCache::StateCache const &state = (*DepCache)[P];
      if (state.CandidateVer != NULL)
      {
	 pkgCache::VerIterator const CandV(CacheFile, state.CandidateVer);
	 return CandV.Arch();
      }
      else
      {
	 pkgCache::VerIterator const V = P.VersionList();
	 if (V.end() == false)
	    return V.Arch();
	 else
	    return P.Arch();
      }
   }
   else
      return P.CurrentVer().Arch();
}
									/*}}}*/
static std::string GetShortDescription(pkgCacheFile &CacheFile, pkgRecords &records, pkgCache::PkgIterator P)/*{{{*/
{
   pkgPolicy *policy = CacheFile.GetPolicy();

   pkgCache::VerIterator ver;
   if (P.CurrentVer())
      ver = P.CurrentVer();
   else
      ver = policy->GetCandidateVer(P);

   std::string ShortDescription = "(none)";
   if(ver)
   {
      pkgCache::DescIterator const Desc = ver.TranslatedDescription();
      if (Desc.end() == false)
      {
	 pkgRecords::Parser & parser = records.Lookup(Desc.FileList());
	 ShortDescription = parser.ShortDesc();
      }
   }
   return ShortDescription;
}
									/*}}}*/
static std::string GetLongDescription(pkgCacheFile &CacheFile, pkgRecords &records, pkgCache::PkgIterator P)/*{{{*/
{
   pkgPolicy *policy = CacheFile.GetPolicy();

   pkgCache::VerIterator ver;
   if (P->CurrentVer != 0)
      ver = P.CurrentVer();
   else
      ver = policy->GetCandidateVer(P);

   std::string const EmptyDescription = "(none)";
   if(ver.end() == true)
      return EmptyDescription;

   pkgCache::DescIterator const Desc = ver.TranslatedDescription();
   if (Desc.end() == false)
   {
      pkgRecords::Parser & parser = records.Lookup(Desc.FileList());
      std::string const longdesc = parser.LongDesc();
      if (longdesc.empty() == false)
	 return SubstVar(longdesc, "\n ", "\n  ");
   }
   return EmptyDescription;
}
									/*}}}*/
void ListSingleVersion(pkgCacheFile &CacheFile, pkgRecords &records,	/*{{{*/
                       pkgCache::VerIterator const &V, std::ostream &out,
                       std::string const &format)
{
   pkgCache::PkgIterator const P = V.ParentPkg();
   pkgDepCache * const DepCache = CacheFile.GetDepCache();
   pkgDepCache::StateCache const &state = (*DepCache)[P];

   std::string output;
   if (_config->FindB("APT::Cmd::use-format", false))
      output = _config->Find("APT::Cmd::format", "${db::Status-Abbrev} ${Package} ${Version} ${Origin} ${Description}");
   else
      output = format;

   // FIXME: some of these names are really icky – and all is nowhere documented
   output = SubstVar(output, "${db::Status-Abbrev}", GetFlagsStr(CacheFile, P));
   output = SubstVar(output, "${Package}", P.Name());
   std::string const ArchStr = GetArchitecture(CacheFile, P);
   output = SubstVar(output, "${Architecture}", ArchStr);
   std::string const InstalledVerStr = GetInstalledVersion(CacheFile, P);
   output = SubstVar(output, "${installed:Version}", InstalledVerStr);
   std::string const CandidateVerStr = GetCandidateVersion(CacheFile, P);
   output = SubstVar(output, "${candidate:Version}", CandidateVerStr);
   std::string const VersionStr = GetVersion(CacheFile, V);
   output = SubstVar(output, "${Version}", VersionStr);
   output = SubstVar(output, "${Origin}", GetArchiveSuite(CacheFile, V));

   std::string StatusStr = "";
   if (P->CurrentVer != 0)
   {
      if (P.CurrentVer() == V)
      {
	 if (state.Upgradable() && state.CandidateVer != NULL)
	    strprintf(StatusStr, _("[installed,upgradable to: %s]"),
		  CandidateVerStr.c_str());
	 else if (V.Downloadable() == false)
	    StatusStr = _("[installed,local]");
	 else if(V.Automatic() == true && state.Garbage == true)
	    StatusStr = _("[installed,auto-removable]");
	 else if ((state.Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
	    StatusStr = _("[installed,automatic]");
	 else
	    StatusStr = _("[installed]");
      }
      else if (state.CandidateVer == V && state.Upgradable())
	 strprintf(StatusStr, _("[upgradable from: %s]"),
	       InstalledVerStr.c_str());
   }
   else if (V.ParentPkg()->CurrentState == pkgCache::State::ConfigFiles)
      StatusStr = _("[residual-config]");
   output = SubstVar(output, "${apt:Status}", StatusStr);
   output = SubstVar(output, "${color:highlight}", color("Highlight"));
   output = SubstVar(output, "${color:neutral}", color("Neutral"));
   output = SubstVar(output, "${Description}", GetShortDescription(CacheFile, records, P));
   if (output.find("${LongDescription}") != string::npos)
      output = SubstVar(output, "${LongDescription}", GetLongDescription(CacheFile, records, P));
   output = SubstVar(output, "${ }${ }", "${ }");
   output = SubstVar(output, "${ }\n", "\n");
   output = SubstVar(output, "${ }", " ");
   if (APT::String::Endswith(output, " ") == true)
      output.erase(output.length() - 1);

   out << output;
}
									/*}}}*/
// ShowWithColumns - Show a list in the style of ls			/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a vector of strings with the given indent and in as
   many columns as will fit the screen width.
   
   The output looks like:
  abiword                debootstrap                  gir1.2-upowerglib-1.0
  abiword-common         dh-make                      google-chrome-beta
  abiword-plugin-grammar dmeventd                     gstreamer1.0-clutter-3.0
  binfmt-support         dmsetup                      hostname
  console-setup          evolution-data-server        iproute2
  console-setup-linux    evolution-data-server-common
  coreutils              ffmpeg
 */
struct columnInfo
{
   bool ValidLen;
   size_t LineWidth;
   vector<size_t> RemainingWidths;
};
void ShowWithColumns(ostream &out, vector<string> const &List, size_t Indent, size_t ScreenWidth)
{
   constexpr size_t MinColumnWidth = 2;
   constexpr size_t ColumnSpace = 2;

   size_t const ListSize = List.size();
   size_t const MaxScreenCols = (ScreenWidth - Indent) /
         MinColumnWidth;
   size_t const MaxNumCols = min(MaxScreenCols, ListSize);

   vector<columnInfo> ColumnInfo(MaxNumCols);
   for (size_t I = 0; I < MaxNumCols; ++I) {
      ColumnInfo[I].ValidLen = true;
      ColumnInfo[I].LineWidth = (I + 1) * MinColumnWidth;
      ColumnInfo[I].RemainingWidths.resize(I + 1, MinColumnWidth);
   }

   for (size_t I = 0; I < ListSize; ++I) {
      for (size_t J = 0; J < MaxNumCols; ++J) {
         auto& Col = ColumnInfo[J];
         if (!Col.ValidLen)
            continue;

         size_t Idx = I / ((ListSize + J) / (J + 1));
         size_t RealColLen = List[I].size() + (Idx == J ? 0 : ColumnSpace);
         if (Col.RemainingWidths[Idx] < RealColLen) {
            Col.LineWidth += RealColLen - Col.RemainingWidths[Idx];
            Col.RemainingWidths[Idx] = RealColLen;
            Col.ValidLen = Col.LineWidth < ScreenWidth;
         }
      }
   }
   size_t NumCols = MaxNumCols;
   while (NumCols > 1 && !ColumnInfo[NumCols - 1].ValidLen)
      --NumCols;

   size_t NumRows = ListSize / NumCols + (ListSize % NumCols != 0);
   auto const &LineFormat = ColumnInfo[NumCols - 1];
   for (size_t Row = 0; Row < NumRows; ++Row) {
      size_t Col = 0;
      size_t I = Row;
      out << string(Indent, ' ');
      while (true) {
         out << List[I];

         size_t CurLen = List[I].size();
         size_t MaxLen = LineFormat.RemainingWidths[Col++];
         I += NumRows;
         if (I >= ListSize)
            break;

         out << string(MaxLen - CurLen, ' ');
      }
      out << endl;
   }
}
									/*}}}*/
// ShowBroken - Debugging aide						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
   with the name of each broken dependency and a quite version
   description.

   The output looks like:
 The following packages have unmet dependencies:
     exim: Depends: libc6 (>= 2.1.94) but 2.1.3-10 is to be installed
           Depends: libldap2 (>= 2.0.2-2) but it is not going to be installed
           Depends: libsasl7 but it is not going to be installed
 */
static void ShowBrokenPackage(ostream &out, pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg, bool const Now)
{
   if (Now == true)
   {
      if ((*Cache)[Pkg].NowBroken() == false)
	 return;
   }
   else
   {
      if ((*Cache)[Pkg].InstBroken() == false)
	 return;
   }

   // Print out each package and the failed dependencies
   out << " " << Pkg.FullName(true) << " :";
   unsigned const Indent = Pkg.FullName(true).size() + 3;
   bool First = true;
   pkgCache::VerIterator Ver;

   if (Now == true)
      Ver = Pkg.CurrentVer();
   else
      Ver = (*Cache)[Pkg].InstVerIter(*Cache);

   if (Ver.end() == true)
   {
      out << endl;
      return;
   }

   for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false;)
   {
      // Compute a single dependency element (glob or)
      pkgCache::DepIterator Start;
      pkgCache::DepIterator End;
      D.GlobOr(Start,End); // advances D

      if ((*Cache)->IsImportantDep(End) == false)
	 continue;

      if (Now == true)
      {
	 if (((*Cache)[End] & pkgDepCache::DepGNow) == pkgDepCache::DepGNow)
	    continue;
      }
      else
      {
	 if (((*Cache)[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    continue;
      }

      bool FirstOr = true;
      while (1)
      {
	 if (First == false)
	    for (unsigned J = 0; J != Indent; J++)
	       out << ' ';
	 First = false;

	 if (FirstOr == false)
	 {
	    for (unsigned J = 0; J != strlen(End.DepType()) + 3; J++)
	       out << ' ';
	 }
	 else
	    out << ' ' << End.DepType() << ": ";
	 FirstOr = false;

	 out << Start.TargetPkg().FullName(true);

	 // Show a quick summary of the version requirements
	 if (Start.TargetVer() != 0)
	    out << " (" << Start.CompType() << " " << Start.TargetVer() << ")";

	 /* Show a summary of the target package if possible. In the case
	    of virtual packages we show nothing */
	 pkgCache::PkgIterator Targ = Start.TargetPkg();
	 if (Targ->ProvidesList == 0)
	 {
	    out << ' ';
	    pkgCache::VerIterator Ver = (*Cache)[Targ].InstVerIter(*Cache);
	    if (Now == true)
	       Ver = Targ.CurrentVer();

	    if (Ver.end() == false)
	    {
	       if (Now == true)
		  ioprintf(out,_("but %s is installed"),Ver.VerStr());
	       else
		  ioprintf(out,_("but %s is to be installed"),Ver.VerStr());
	    }
	    else
	    {
	       if ((*Cache)[Targ].CandidateVerIter(*Cache).end() == true)
	       {
		  if (Targ->ProvidesList == 0)
		     out << _("but it is not installable");
		  else
		     out << _("but it is a virtual package");
	       }
	       else
		  out << (Now?_("but it is not installed"):_("but it is not going to be installed"));
	    }
	 }

	 if (Start != End)
	    out << _(" or");
	 out << endl;

	 if (Start == End)
	    break;
	 ++Start;
      }
   }
}
void ShowBroken(ostream &out, CacheFile &Cache, bool const Now)
{
   if (Cache->BrokenCount() == 0)
      return;
   if (_config->FindI("APT::Output-Version") < 30)
      out << _("The following packages have unmet dependencies:") << endl;
   else
      out << _("Unsatisfied dependencies:") << endl;
   SortedPackageUniverse Universe(Cache);
   for (auto const &Pkg: Universe)
      ShowBrokenPackage(out, &Cache, Pkg, Now);
}
void ShowBroken(ostream &out, pkgCacheFile &Cache, bool const Now)
{
   if (Cache->BrokenCount() == 0)
      return;

   if (_config->FindI("APT::Output-Version") < 30)
      out << _("The following packages have unmet dependencies:") << endl;
   else
      out << _("Unsatisfied dependencies:") << endl;
   APT::PackageUniverse Universe(Cache);
   for (auto const &Pkg: Universe)
      ShowBrokenPackage(out, &Cache, Pkg, Now);
}
									/*}}}*/
// ShowNew - Show packages to newly install				/*{{{*/
void ShowNew(ostream &out,CacheFile &Cache)
{
   SortedPackageUniverse Universe(Cache);
   if (_config->FindI("APT::Output-Version") < 30) {
      ShowList(out,_("The following NEW packages will be installed:"), Universe,
	    [&Cache](pkgCache::PkgIterator const &Pkg) { return Cache[Pkg].NewInstall(); },
	    &PrettyFullName,
	    CandidateVersion(&Cache),
	    "action::install");
      return;
   }

   ShowList(out,_("Installing:"), Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg) { return Cache[Pkg].NewInstall() && (Cache[Pkg].Flags & pkgCache::Flag::Auto) == 0; },
	 &PrettyFullName,
	 CandidateVersion(&Cache),
	 "action::install");
   ShowList(out,_("Installing dependencies:"), Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg) { return Cache[Pkg].NewInstall() && Cache[Pkg].Flags & pkgCache::Flag::Auto;},
	 &PrettyFullName,
	 CandidateVersion(&Cache),
	 "action::install-dependencies");
}
									/*}}}*/
// ShowDel - Show packages to delete					/*{{{*/
void ShowDel(ostream &out,CacheFile &Cache)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30 ? _("The following packages will be REMOVED:") : _("REMOVING:");
   ShowList(out,title, Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg) { return Cache[Pkg].Delete(); },
	 [&Cache](pkgCache::PkgIterator const &Pkg)
	 {
	    std::string str = PrettyFullName(Pkg);
	    if (((*Cache)[Pkg].iFlags & pkgDepCache::Purge) == pkgDepCache::Purge)
	       str.append("*");
	    return str;
	 },
	 CurrentVersion(&Cache),
	 "action::remove");
}
									/*}}}*/
// ShowPhasing - Show packages kept due to phasing			/*{{{*/
void ShowPhasing(ostream &out, CacheFile &Cache, APT::PackageVector const &HeldBackPackages)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30
	       ? _("The following upgrades have been deferred due to phasing:")
	       : _("Not upgrading yet due to phasing:");
   ShowList(out, title, HeldBackPackages,
	    &AlwaysTrue,
	    &PrettyFullName,
	    CurrentToCandidateVersion(&Cache));
}
									/*}}}*/
// ShowKept - Show kept packages					/*{{{*/
void ShowKept(ostream &out,CacheFile &Cache, APT::PackageVector const &HeldBackPackages)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30 ? _("The following packages have been kept back:") : _("Not upgrading:");
   ShowList(out, title, HeldBackPackages,
	 &AlwaysTrue,
	 &PrettyFullName,
	 CurrentToCandidateVersion(&Cache));
}
									/*}}}*/
// ShowUpgraded - Show upgraded packages				/*{{{*/
void ShowUpgraded(ostream &out,CacheFile &Cache)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30 ? _("The following packages will be upgraded:") : _("Upgrading:");
   ShowList(out, title, Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg)
	 {
	    return Cache[Pkg].Upgrade() == true && Cache[Pkg].NewInstall() == false;
	 },
	 &PrettyFullName,
	 CurrentToCandidateVersion(&Cache),
	 "action::upgrade");
}
									/*}}}*/
// ShowDowngraded - Show downgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowDowngraded(ostream &out,CacheFile &Cache)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30 ? _("The following packages will be DOWNGRADED:") : _("DOWNGRADING:");
   return ShowList(out, title, Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg)
	 {
	    return Cache[Pkg].Downgrade() == true && Cache[Pkg].NewInstall() == false;
	 },
	 &PrettyFullName,
	 CurrentToCandidateVersion(&Cache),
	 "action::downgrade");
}
									/*}}}*/
// ShowHold - Show held but changed packages				/*{{{*/
bool ShowHold(ostream &out,CacheFile &Cache)
{
   SortedPackageUniverse Universe(Cache);
   auto title = _config->FindI("APT::Output-Version") < 30 ? _("The following held packages will be changed:") : _("Changing held packages:");
   return ShowList(out, title, Universe,
	 [&Cache](pkgCache::PkgIterator const &Pkg)
	 {
	    return Pkg->SelectedState == pkgCache::State::Hold &&
		   Cache[Pkg].InstallVer != (pkgCache::Version *)Pkg.CurrentVer();
	 },
	 &PrettyFullName,
	 CurrentToCandidateVersion(&Cache));
}
									/*}}}*/
// ShowEssential - Show an essential package warning			/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a warning message that is not to be ignored. It shows
   all essential packages and their dependents that are to be removed.
   It is insanely risky to remove the dependents of an essential package! */
struct APT_HIDDEN PrettyFullNameWithDue {
   std::map<unsigned long long, pkgCache::PkgIterator> due;
   PrettyFullNameWithDue() {}
   std::string operator() (pkgCache::PkgIterator const &Pkg)
   {
      std::string const A = PrettyFullName(Pkg);
      std::map<unsigned long long, pkgCache::PkgIterator>::const_iterator d = due.find(Pkg->ID);
      if (d == due.end())
        return A;

      std::string const B = PrettyFullName(d->second);
      std::ostringstream outstr;
      ioprintf(outstr, _("%s (due to %s)"), A.c_str(), B.c_str());
      return outstr.str();
   }
};
bool ShowEssential(ostream &out,CacheFile &Cache)
{
   std::vector<bool> Added(Cache->Head().PackageCount, false);
   APT::PackageDeque pkglist;
   PrettyFullNameWithDue withdue;

   SortedPackageUniverse Universe(Cache);
   for (pkgCache::PkgIterator const &I: Universe)
   {
      if ((I->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential &&
	  (I->Flags & pkgCache::Flag::Important) != pkgCache::Flag::Important)
	 continue;

      // The essential package is being removed
      if (Cache[I].Delete() == false)
	 continue;

      if (Added[I->ID] == false)
      {
	 Added[I->ID] = true;
	 pkglist.insert(I);
      }

      if (I->CurrentVer == 0)
	 continue;

      // Print out any essential package depenendents that are to be removed
      for (pkgCache::DepIterator D = I.CurrentVer().DependsList(); D.end() == false; ++D)
      {
	 // Skip everything but depends
	 if (D->Type != pkgCache::Dep::PreDepends &&
	     D->Type != pkgCache::Dep::Depends)
	    continue;

	 pkgCache::PkgIterator P = D.SmartTargetPkg();
	 if (Cache[P].Delete() == true)
	 {
	    if (Added[P->ID] == true)
	       continue;
	    Added[P->ID] = true;

	    pkglist.insert(P);
	    withdue.due[P->ID] = I;
	 }
      }
   }
   return ShowList(out,_("WARNING: The following essential packages will be removed.\n"
			 "This should NOT be done unless you know exactly what you are doing!"),
	 pkglist, &AlwaysTrue, withdue, &EmptyString, "action::remove");
}
									/*}}}*/
// Stats - Show some statistics						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Stats(ostream &out, pkgDepCache &Dep, APT::PackageVector const &HeldBackPackages)
{
   unsigned long Upgrade = 0;
   unsigned long Downgrade = 0;
   unsigned long Install = 0;
   unsigned long ReInstall = 0;
   auto outVer = _config->FindI("APT::Output-Version");
   for (pkgCache::PkgIterator I = Dep.PkgBegin(); I.end() == false; ++I)
   {
      if (Dep[I].NewInstall() == true)
	 Install++;
      else
      {
	 if (Dep[I].Upgrade() == true)
	    Upgrade++;
	 else
	    if (Dep[I].Downgrade() == true)
	       Downgrade++;
      }
      
      if (Dep[I].Delete() == false && (Dep[I].iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall)
	 ReInstall++;
   }   
   if (outVer >= 30) {
      ioprintf(out, _("Summary:"));
      ioprintf(out, "\n  ");
   }
   ioprintf(out,outVer < 30 ? _("%lu upgraded, %lu newly installed, ") : _("Upgrading: %lu, Installing: %lu, "),
	    Upgrade,Install);
   
   if (ReInstall != 0)
      ioprintf(out,outVer < 30 ? _("%lu reinstalled, ") : _("Reinstalling: %lu, "),ReInstall);
   if (Downgrade != 0)
      ioprintf(out,outVer < 30 ? _("%lu downgraded, ") : _("Downgrading: %lu, "),Downgrade);

   ioprintf(out, outVer < 30 ? _("%lu to remove and %lu not upgraded.\n") : _("Removing: %lu, Not Upgrading: %lu\n"),
	    Dep.DelCount(), HeldBackPackages.size());

   if (Dep.BadCount() != 0) {
      if (outVer >= 30)
	 ioprintf(out, "  ");
      ioprintf(out,_("%lu not fully installed or removed.\n"),
	       Dep.BadCount());
   }
}
									/*}}}*/
// YnPrompt - Yes No Prompt.						/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool YnPrompt(char const * const Question, bool const Default, bool const ShowGlobalErrors, std::ostream &c1o, std::ostream &c2o)
{
   auto const AssumeYes = _config->FindB("APT::Get::Assume-Yes",false);
   auto const AssumeNo = _config->FindB("APT::Get::Assume-No",false);
   // if we ask interactively, show warnings/notices before the question
   if (ShowGlobalErrors == true && AssumeYes == false && AssumeNo == false)
   {
      if (_config->FindB("APT::Audit"))
	 _error->DumpErrors(c2o, GlobalError::AUDIT);
      else if (_config->FindI("quiet",0) > 0)
	 _error->DumpErrors(c2o);
      else
	 _error->DumpErrors(c2o, GlobalError::NOTICE);
   }

   c2o << Question << std::flush;

   /* nl_langinfo does not support LANGUAGE setting, so we unset it here
      to have the help-message (hopefully) match the expected characters */
   char * language = getenv("LANGUAGE");
   if (language != NULL)
      language = strdup(language);
   if (language != NULL)
      unsetenv("LANGUAGE");

   if (Default == true)
      // TRANSLATOR: Yes/No question help-text: defaulting to Y[es]
      //             e.g. "Do you want to continue? [Y/n] "
      //             The user has to answer with an input matching the
      //             YESEXPR/NOEXPR defined in your l10n.
      c2o << " " << _("[Y/n]") << " " << std::flush;
   else
      // TRANSLATOR: Yes/No question help-text: defaulting to N[o]
      //             e.g. "Should this file be removed? [y/N] "
      //             The user has to answer with an input matching the
      //             YESEXPR/NOEXPR defined in your l10n.
      c2o << " " << _("[y/N]") << " " << std::flush;

   if (language != NULL)
   {
      setenv("LANGUAGE", language, 0);
      free(language);
   }

   if (AssumeYes)
   {
      // TRANSLATOR: "Yes" answer printed for a yes/no question if --assume-yes is set
      c1o << _("Y") << std::endl;
      return true;
   }
   else if (AssumeNo)
   {
      // TRANSLATOR: "No" answer printed for a yes/no question if --assume-no is set
      c1o << _("N") << std::endl;
      return false;
   }

   char response[1024] = "";
   std::cin.getline(response, sizeof(response));

   if (!std::cin)
      return false;

   if (strlen(response) == 0)
      return Default;

   regex_t Pattern;
   int Res;

   Res = regcomp(&Pattern, nl_langinfo(YESEXPR),
                 REG_EXTENDED|REG_ICASE|REG_NOSUB);

   if (Res != 0) {
      char Error[300];
      regerror(Res,&Pattern,Error,sizeof(Error));
      return _error->Error(_("Regex compilation error - %s"),Error);
   }

   Res = regexec(&Pattern, response, 0, NULL, 0);
   if (Res == 0)
      return true;
   return false;
}
bool YnPrompt(char const * const Question, bool const Default)
{
   return YnPrompt(Question, Default, true, c1out, c2out);
}
									/*}}}*/

std::string PrettyFullName(pkgCache::PkgIterator const &Pkg)
{
   return Pkg.FullName(true);
}
std::string CurrentVersion(pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg)
{
   return (*Cache)[Pkg].CurVersion;
}
std::function<std::string(pkgCache::PkgIterator const &)> CurrentVersion(pkgCacheFile * const Cache)
{
   return std::bind(static_cast<std::string(*)(pkgCacheFile * const, pkgCache::PkgIterator const&)>(&CurrentVersion), Cache, std::placeholders::_1);
}
std::string CandidateVersion(pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg)
{
   return (*Cache)[Pkg].CandVersion;
}
std::function<std::string(pkgCache::PkgIterator const &)> CandidateVersion(pkgCacheFile * const Cache)
{
   return std::bind(static_cast<std::string(*)(pkgCacheFile * const, pkgCache::PkgIterator const&)>(&CandidateVersion), Cache, std::placeholders::_1);
}
std::string CurrentToCandidateVersion(pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg)
{
   std::string const CurVer = (*Cache)[Pkg].CurVersion;
   std::string CandVer = (*Cache)[Pkg].CandVersion;
   if (CurVer == CandVer)
   {
      auto const CandVerIter = Cache->GetPolicy()->GetCandidateVer(Pkg);
      if (not CandVerIter.end())
	 CandVer = CandVerIter.VerStr();
   }
   return  CurVer + " => " + CandVer;
}
std::function<std::string(pkgCache::PkgIterator const &)> CurrentToCandidateVersion(pkgCacheFile * const Cache)
{
   return std::bind(static_cast<std::string(*)(pkgCacheFile * const, pkgCache::PkgIterator const&)>(&CurrentToCandidateVersion), Cache, std::placeholders::_1);
}
bool AlwaysTrue(pkgCache::PkgIterator const &)
{
      return true;
}
std::string EmptyString(pkgCache::PkgIterator const &)
{
   return std::string();
}
