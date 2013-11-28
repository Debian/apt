// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>

#include <iomanip>
#include <iostream>
#include <locale.h>
#include <langinfo.h>
#include <unistd.h>

#include "private-output.h"
#include "private-cachefile.h"

#include <apti18n.h>
									/*}}}*/

using namespace std;

std::ostream c0out(0);
std::ostream c1out(0);
std::ostream c2out(0);
std::ofstream devnull("/dev/null");
unsigned int ScreenWidth = 80 - 1; /* - 1 for the cursor */

bool InitOutput()							/*{{{*/
{
   c0out.rdbuf(cout.rdbuf());
   c1out.rdbuf(cout.rdbuf());
   c2out.rdbuf(cout.rdbuf());
   if (_config->FindI("quiet",0) > 0)
      c0out.rdbuf(devnull.rdbuf());
   if (_config->FindI("quiet",0) > 1)
      c1out.rdbuf(devnull.rdbuf());

   if(!isatty(1))
   {
      _config->Set("APT::Color", "false");
      _config->Set("APT::Color::Highlight", "");
      _config->Set("APT::Color::Neutral", "");
   } else {
      // Colors
      _config->CndSet("APT::Color::Highlight", "\x1B[32m");
      _config->CndSet("APT::Color::Neutral", "\x1B[0m");
      
      _config->CndSet("APT::Color::Red", "\x1B[31m");
      _config->CndSet("APT::Color::Green", "\x1B[32m");
      _config->CndSet("APT::Color::Yellow", "\x1B[33m");
      _config->CndSet("APT::Color::Blue", "\x1B[34m");
      _config->CndSet("APT::Color::Magenta", "\x1B[35m");
      _config->CndSet("APT::Color::Cyan", "\x1B[36m");
      _config->CndSet("APT::Color::White", "\x1B[37m");
   }

   return true;
}
									/*}}}*/
std::string GetArchiveSuite(pkgCacheFile &CacheFile, pkgCache::VerIterator ver) /*{{{*/
{
   std::string suite = "";
   if (ver && ver.FileList() && ver.FileList())
   {
      pkgCache::VerFileIterator VF = ver.FileList();
      for (; VF.end() == false ; ++VF)
      {
         // XXX: how to figure out the relevant suite? if its in multiple ones?
         suite = suite + "," + VF.File().Archive();
         //suite = VF.File().Archive();
      }
      suite = suite.erase(0, 1);
   }
   return suite;
}
									/*}}}*/
std::string GetFlagsStr(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   pkgDepCache::StateCache &state = (*DepCache)[P];

   std::string flags_str;
   if (state.NowBroken())
      flags_str = "B";
   if (P.CurrentVer() && state.Upgradable())
      flags_str = "g";
   else if (P.CurrentVer() != NULL)
      flags_str = "i";
   else
      flags_str = "-";
   return flags_str;
}
									/*}}}*/
std::string GetCandidateVersion(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgPolicy *policy = CacheFile.GetPolicy();
   pkgCache::VerIterator cand = policy->GetCandidateVer(P);

   return cand ? cand.VerStr() : "(none)";
}
									/*}}}*/
std::string GetInstalledVersion(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgCache::VerIterator inst = P.CurrentVer();

   return inst ? inst.VerStr() : "(none)";
}
									/*}}}*/
std::string GetVersion(pkgCacheFile &CacheFile, pkgCache::VerIterator V)/*{{{*/
{
   pkgCache::PkgIterator P = V.ParentPkg();
   if (V == P.CurrentVer())
   {
      pkgDepCache *DepCache = CacheFile.GetDepCache();
      pkgDepCache::StateCache &state = (*DepCache)[P];
      std::string inst_str = DeNull(V.VerStr());
      if (state.Upgradable())
         return "**"+inst_str;
      return inst_str;
   }

   if(V)
      return DeNull(V.VerStr());
   return "(none)";
}
									/*}}}*/
std::string GetArchitecture(pkgCacheFile &CacheFile, pkgCache::PkgIterator P)/*{{{*/
{
   pkgPolicy *policy = CacheFile.GetPolicy();
   pkgCache::VerIterator inst = P.CurrentVer();
   pkgCache::VerIterator cand = policy->GetCandidateVer(P);
   
   return inst ? inst.Arch() : cand.Arch();
}
									/*}}}*/
std::string GetShortDescription(pkgCacheFile &CacheFile, pkgRecords &records, pkgCache::PkgIterator P)/*{{{*/
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
      pkgCache::DescIterator Desc = ver.TranslatedDescription();
      pkgRecords::Parser & parser = records.Lookup(Desc.FileList());

      ShortDescription = parser.ShortDesc();
   }
   return ShortDescription;
}
									/*}}}*/
void ListSingleVersion(pkgCacheFile &CacheFile, pkgRecords &records,	/*{{{*/
                       pkgCache::VerIterator V, std::ostream &out,
                       bool include_summary)
{
   pkgCache::PkgIterator P = V.ParentPkg();

   pkgDepCache *DepCache = CacheFile.GetDepCache();
   pkgDepCache::StateCache &state = (*DepCache)[P];

   std::string suite = GetArchiveSuite(CacheFile, V);
   std::string name_str = P.Name();

   if (_config->FindB("APT::Cmd::use-format", false))
   {
      std::string format = _config->Find("APT::Cmd::format", "${db::Status-Abbrev} ${Package} ${Version} ${Origin} ${Description}");
      std::string output = format;
   
      output = SubstVar(output, "${db::Status-Abbrev}", GetFlagsStr(CacheFile, P));
      output = SubstVar(output, "${Package}", name_str);
      output = SubstVar(output, "${installed:Version}", GetInstalledVersion(CacheFile, P));
      output = SubstVar(output, "${candidate:Version}", GetCandidateVersion(CacheFile, P));
      output = SubstVar(output, "${Version}", GetVersion(CacheFile, V));
      output = SubstVar(output, "${Description}", GetShortDescription(CacheFile, records, P));
      output = SubstVar(output, "${Origin}", GetArchiveSuite(CacheFile, V));
      out << output << std::endl;
   } else {
      // raring/linux-kernel version [upradable: new-version]
      //    description
      pkgPolicy *policy = CacheFile.GetPolicy();
      out << std::setiosflags(std::ios::left)
          << _config->Find("APT::Color::Highlight", "")
          << name_str 
          << _config->Find("APT::Color::Neutral", "")
          << "/" << suite
          << " ";
      if(P.CurrentVer() == V && state.Upgradable()) {
         out << GetVersion(CacheFile, V)
             << " "
             << "[" << _("installed,upgradable to: ")
             << GetCandidateVersion(CacheFile, P) << "]";
      } else if (P.CurrentVer() == V) {
         out << GetVersion(CacheFile, V)
             << " ";
         if(!V.Downloadable())
            out << _("[installed,local]");
         else
            if(V.Automatic() && state.Garbage)
                  out << _("[installed,auto-removable]");
            else if (state.Flags & pkgCache::Flag::Auto)
               out << _("[installed,automatic]");
            else
               out << _("[installed]");
      } else if (P.CurrentVer() && 
                 policy->GetCandidateVer(P) == V && 
                 state.Upgradable()) {
         out << GetVersion(CacheFile, V)
             << " "
             << _("[upgradable from: ")
             << GetInstalledVersion(CacheFile, P) << "]";
      } else {
         if (V.ParentPkg()->CurrentState == pkgCache::State::ConfigFiles)
            out << GetVersion(CacheFile, V) 
                << " "
                << _("[residual-config]");
         else
            out << GetVersion(CacheFile, V);
      }
      out << " " << GetArchitecture(CacheFile, P) << " ";
      if (include_summary)
      {
         out << std::endl 
             << "    " << GetShortDescription(CacheFile, records, P)
             << std::endl;
      }
   }
}
									/*}}}*/
// ShowList - Show a list						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a string of space separated words with a title and 
   a two space indent line wraped to the current screen width. */
bool ShowList(ostream &out,string Title,string List,string VersionsList)
{
   if (List.empty() == true)
      return true;
   // trim trailing space
   int NonSpace = List.find_last_not_of(' ');
   if (NonSpace != -1)
   {
      List = List.erase(NonSpace + 1);
      if (List.empty() == true)
	 return true;
   }

   // Acount for the leading space
   int ScreenWidth = ::ScreenWidth - 3;
      
   out << Title << endl;
   string::size_type Start = 0;
   string::size_type VersionsStart = 0;
   while (Start < List.size())
   {
      if(_config->FindB("APT::Get::Show-Versions",false) == true &&
         VersionsList.size() > 0) {
         string::size_type End;
         string::size_type VersionsEnd;
         
         End = List.find(' ',Start);
         VersionsEnd = VersionsList.find('\n', VersionsStart);

         out << "   " << string(List,Start,End - Start) << " (" << 
            string(VersionsList,VersionsStart,VersionsEnd - VersionsStart) << 
            ")" << endl;

	 if (End == string::npos || End < Start)
	    End = Start + ScreenWidth;

         Start = End + 1;
         VersionsStart = VersionsEnd + 1;
      } else {
         string::size_type End;

         if (Start + ScreenWidth >= List.size())
            End = List.size();
         else
            End = List.rfind(' ',Start+ScreenWidth);

         if (End == string::npos || End < Start)
            End = Start + ScreenWidth;
         out << "  " << string(List,Start,End - Start) << endl;
         Start = End + 1;
      }
   }   

   return false;
}
									/*}}}*/
// ShowBroken - Debugging aide						/*{{{*/
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
   with the name of each each broken dependency and a quite version 
   description.
   
   The output looks like:
 The following packages have unmet dependencies:
     exim: Depends: libc6 (>= 2.1.94) but 2.1.3-10 is to be installed
           Depends: libldap2 (>= 2.0.2-2) but it is not going to be installed
           Depends: libsasl7 but it is not going to be installed   
 */
void ShowBroken(ostream &out,CacheFile &Cache,bool Now)
{
   if (Cache->BrokenCount() == 0)
      return;

   out << _("The following packages have unmet dependencies:") << endl;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      if (Now == true)
      {
	 if (Cache[I].NowBroken() == false)
	    continue;
      }
      else
      {
	 if (Cache[I].InstBroken() == false)
	    continue;
      }
      
      // Print out each package and the failed dependencies
      out << " " << I.FullName(true) << " :";
      unsigned const Indent = I.FullName(true).size() + 3;
      bool First = true;
      pkgCache::VerIterator Ver;
      
      if (Now == true)
	 Ver = I.CurrentVer();
      else
	 Ver = Cache[I].InstVerIter(Cache);
      
      if (Ver.end() == true)
      {
	 out << endl;
	 continue;
      }
      
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false;)
      {
	 // Compute a single dependency element (glob or)
	 pkgCache::DepIterator Start;
	 pkgCache::DepIterator End;
	 D.GlobOr(Start,End); // advances D

	 if (Cache->IsImportantDep(End) == false)
	    continue;
	 
	 if (Now == true)
	 {
	    if ((Cache[End] & pkgDepCache::DepGNow) == pkgDepCache::DepGNow)
	       continue;
	 }
	 else
	 {
	    if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
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
	       pkgCache::VerIterator Ver = Cache[Targ].InstVerIter(Cache);
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
		  if (Cache[Targ].CandidateVerIter(Cache).end() == true)
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
}
									/*}}}*/
// ShowNew - Show packages to newly install				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowNew(ostream &out,CacheFile &Cache)
{
   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].NewInstall() == true) {
         List += I.FullName(true) + " ";
         VersionsList += string(Cache[I].CandVersion) + "\n";
      }
   }
   
   ShowList(out,_("The following NEW packages will be installed:"),List,VersionsList);
}
									/*}}}*/
// ShowDel - Show packages to delete					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowDel(ostream &out,CacheFile &Cache)
{
   /* Print out a list of packages that are going to be removed extra
      to what the user asked */
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].Delete() == true)
      {
	 if ((Cache[I].iFlags & pkgDepCache::Purge) == pkgDepCache::Purge)
	    List += I.FullName(true) + "* ";
	 else
	    List += I.FullName(true) + " ";
     
     VersionsList += string(Cache[I].CandVersion)+ "\n";
      }
   }
   
   ShowList(out,_("The following packages will be REMOVED:"),List,VersionsList);
}
									/*}}}*/
// ShowKept - Show kept packages					/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowKept(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {	 
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == true || Cache[I].Upgradable() == false ||
	  I->CurrentVer == 0 || Cache[I].Delete() == true)
	 continue;
      
      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   ShowList(out,_("The following packages have been kept back:"),List,VersionsList);
}
									/*}}}*/
// ShowUpgraded - Show upgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
void ShowUpgraded(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Upgrade() == false || Cache[I].NewInstall() == true)
	 continue;

      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   ShowList(out,_("The following packages will be upgraded:"),List,VersionsList);
}
									/*}}}*/
// ShowDowngraded - Show downgraded packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowDowngraded(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      
      // Not interesting
      if (Cache[I].Downgrade() == false || Cache[I].NewInstall() == true)
	 continue;

      List += I.FullName(true) + " ";
      VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
   }
   return ShowList(out,_("The following packages will be DOWNGRADED:"),List,VersionsList);
}
									/*}}}*/
// ShowHold - Show held but changed packages				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHold(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].InstallVer != (pkgCache::Version *)I.CurrentVer() &&
          I->SelectedState == pkgCache::State::Hold) {
         List += I.FullName(true) + " ";
		 VersionsList += string(Cache[I].CurVersion) + " => " + Cache[I].CandVersion + "\n";
      }
   }

   return ShowList(out,_("The following held packages will be changed:"),List,VersionsList);
}
									/*}}}*/
// ShowEssential - Show an essential package warning			/*{{{*/
// ---------------------------------------------------------------------
/* This prints out a warning message that is not to be ignored. It shows
   all essential packages and their dependents that are to be removed. 
   It is insanely risky to remove the dependents of an essential package! */
bool ShowEssential(ostream &out,CacheFile &Cache)
{
   string List;
   string VersionsList;
   bool *Added = new bool[Cache->Head().PackageCount];
   for (unsigned int I = 0; I != Cache->Head().PackageCount; I++)
      Added[I] = false;
   
   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if ((I->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential &&
	  (I->Flags & pkgCache::Flag::Important) != pkgCache::Flag::Important)
	 continue;
      
      // The essential package is being removed
      if (Cache[I].Delete() == true)
      {
	 if (Added[I->ID] == false)
	 {
	    Added[I->ID] = true;
	    List += I.FullName(true) + " ";
        //VersionsList += string(Cache[I].CurVersion) + "\n"; ???
	 }
      }
      else
	 continue;

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
	    
	    char S[300];
	    snprintf(S,sizeof(S),_("%s (due to %s) "),P.FullName(true).c_str(),I.FullName(true).c_str());
	    List += S;
        //VersionsList += "\n"; ???
	 }	 
      }      
   }
   
   delete [] Added;
   return ShowList(out,_("WARNING: The following essential packages will be removed.\n"
			 "This should NOT be done unless you know exactly what you are doing!"),List,VersionsList);
}

									/*}}}*/
// Stats - Show some statistics						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Stats(ostream &out,pkgDepCache &Dep)
{
   unsigned long Upgrade = 0;
   unsigned long Downgrade = 0;
   unsigned long Install = 0;
   unsigned long ReInstall = 0;
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

   ioprintf(out,_("%lu upgraded, %lu newly installed, "),
	    Upgrade,Install);
   
   if (ReInstall != 0)
      ioprintf(out,_("%lu reinstalled, "),ReInstall);
   if (Downgrade != 0)
      ioprintf(out,_("%lu downgraded, "),Downgrade);

   ioprintf(out,_("%lu to remove and %lu not upgraded.\n"),
	    Dep.DelCount(),Dep.KeepCount());
   
   if (Dep.BadCount() != 0)
      ioprintf(out,_("%lu not fully installed or removed.\n"),
	       Dep.BadCount());
}
									/*}}}*/
// YnPrompt - Yes No Prompt.						/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool YnPrompt(bool Default)
{
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
      c2out << " " << _("[Y/n]") << " " << std::flush;
   else
      // TRANSLATOR: Yes/No question help-text: defaulting to N[o]
      //             e.g. "Should this file be removed? [y/N] "
      //             The user has to answer with an input matching the
      //             YESEXPR/NOEXPR defined in your l10n.
      c2out << " " << _("[y/N]") << " " << std::flush;

   if (language != NULL)
   {
      setenv("LANGUAGE", language, 0);
      free(language);
   }

   if (_config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      // TRANSLATOR: "Yes" answer printed for a yes/no question if --assume-yes is set
      c1out << _("Y") << std::endl;
      return true;
   }
   else if (_config->FindB("APT::Get::Assume-No",false) == true)
   {
      // TRANSLATOR: "No" answer printed for a yes/no question if --assume-no is set
      c1out << _("N") << std::endl;
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
									/*}}}*/
// AnalPrompt - Annoying Yes No Prompt.					/*{{{*/
// ---------------------------------------------------------------------
/* Returns true on a Yes.*/
bool AnalPrompt(const char *Text)
{
   char Buf[1024];
   std::cin.getline(Buf,sizeof(Buf));
   if (strcmp(Buf,Text) == 0)
      return true;
   return false;
}
									/*}}}*/
