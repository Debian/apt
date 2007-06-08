// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: policy.cc,v 1.10 2003/08/12 00:17:37 mdz Exp $
/* ######################################################################

   Package Version Policy implementation
   
   This is just a really simple wrapper around pkgVersionMatch with
   some added goodies to manage the list of things..
   
   Priority Table:
   
   1000 -> inf = Downgradeable priorities
   1000        = The 'no downgrade' pseduo-status file
   100 -> 1000 = Standard priorities
   990         = Config file override package files
   989         = Start for preference auto-priorities
   500         = Default package files
   100         = The status file
   0 -> 100    = NotAutomatic sources like experimental
   -inf -> 0   = Never selected   
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/policy.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sptr.h>
    
#include <apti18n.h>

#include <iostream>
#include <sstream>
									/*}}}*/

using namespace std;

// Policy::Init - Startup and bind to a cache				/*{{{*/
// ---------------------------------------------------------------------
/* Set the defaults for operation. The default mode with no loaded policy
   file matches the V0 policy engine. */
pkgPolicy::pkgPolicy(pkgCache *Owner) : Pins(0), PFPriority(0), Cache(Owner)
{
   PFPriority = new signed short[Owner->Head().PackageFileCount];
   Pins = new Pin[Owner->Head().PackageCount];

   for (unsigned long I = 0; I != Owner->Head().PackageCount; I++)
      Pins[I].Type = pkgVersionMatch::None;

   // The config file has a master override.
   string DefRel = _config->Find("APT::Default-Release");
   if (DefRel.empty() == false)
      CreatePin(pkgVersionMatch::Release,"",DefRel,990);
      
   InitDefaults();
}
									/*}}}*/
// Policy::InitDefaults - Compute the default selections		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgPolicy::InitDefaults()
{   
   // Initialize the priorities based on the status of the package file
   for (pkgCache::PkgFileIterator I = Cache->FileBegin(); I != Cache->FileEnd(); I++)
   {
      PFPriority[I->ID] = 500;
      if ((I->Flags & pkgCache::Flag::NotSource) == pkgCache::Flag::NotSource)
	 PFPriority[I->ID] = 100;
      else
	 if ((I->Flags & pkgCache::Flag::NotAutomatic) == pkgCache::Flag::NotAutomatic)
	    PFPriority[I->ID] = 1;
   }

   // Apply the defaults..
   SPtrArray<bool> Fixed = new bool[Cache->HeaderP->PackageFileCount];
   memset(Fixed,0,sizeof(*Fixed)*Cache->HeaderP->PackageFileCount);
   signed Cur = 989;
   StatusOverride = false;
   for (vector<Pin>::const_iterator I = Defaults.begin(); I != Defaults.end();
	I++, Cur--)
   {
      pkgVersionMatch Match(I->Data,I->Type);
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); F++)
      {
	 if (Match.FileMatch(F) == true && Fixed[F->ID] == false)
	 {
	    if (I->Priority != 0 && I->Priority > 0)
	       Cur = I->Priority;
	    
	    if (I->Priority < 0)
	       PFPriority[F->ID] =  I->Priority;
	    else
	       PFPriority[F->ID] = Cur;
	    
	    if (PFPriority[F->ID] > 1000)
	       StatusOverride = true;
	    
	    Fixed[F->ID] = true;
	 }      
      }      
   }

   if (_config->FindB("Debug::pkgPolicy",false) == true)
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); F++)
	 cout << "Prio of " << F.FileName() << ' ' << PFPriority[F->ID] << endl; 
   
   return true;   
}
									/*}}}*/
// Policy::GetCandidateVer - Get the candidate install version		/*{{{*/
// ---------------------------------------------------------------------
/* Evaluate the package pins and the default list to deteremine what the
   best package is. */
pkgCache::VerIterator pkgPolicy::GetCandidateVer(pkgCache::PkgIterator Pkg)
{
   // Look for a package pin and evaluate it.
   signed Max = GetPriority(Pkg);
   pkgCache::VerIterator Pref = GetMatch(Pkg);

   /* Falling through to the default version.. Setting Max to zero
      effectively excludes everything <= 0 which are the non-automatic
      priorities.. The status file is given a prio of 100 which will exclude
      not-automatic sources, except in a single shot not-installed mode.
      The second pseduo-status file is at prio 1000, above which will permit
      the user to force-downgrade things.
      
      The user pin is subject to the same priority rules as default 
      selections. Thus there are two ways to create a pin - a pin that
      tracks the default when the default is taken away, and a permanent
      pin that stays at that setting.
    */
   for (pkgCache::VerIterator Ver = Pkg.VersionList(); Ver.end() == false; Ver++)
   {
      for (pkgCache::VerFileIterator VF = Ver.FileList(); VF.end() == false; VF++)
      {
	 /* If this is the status file, and the current version is not the
	    version in the status file (ie it is not installed, or somesuch)
	    then it is not a candidate for installation, ever. This weeds
	    out bogus entries that may be due to config-file states, or
	    other. */
	 if ((VF.File()->Flags & pkgCache::Flag::NotSource) == pkgCache::Flag::NotSource &&
	     Pkg.CurrentVer() != Ver)
	    continue;
	 	 	 
	 signed Prio = PFPriority[VF.File()->ID];
	 if (Prio > Max)
	 {
	    Pref = Ver;
	    Max = Prio;
	 }	 
      }      
      
      if (Pkg.CurrentVer() == Ver && Max < 1000)
      {
	 /* Elevate our current selection (or the status file itself)
	    to the Pseudo-status priority. */
	 if (Pref.end() == true)
	    Pref = Ver;
	 Max = 1000;
	 
	 // Fast path optimize.
	 if (StatusOverride == false)
	    break;
      }            
   }
   return Pref;
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
   Pin *P = 0;
   
   if (Name.empty() == true)
      P = &*Defaults.insert(Defaults.end(),PkgPin());
   else
   {
      // Get a spot to put the pin
      pkgCache::PkgIterator Pkg = Cache->FindPkg(Name);
      if (Pkg.end() == true)
      {
	 // Check the unmatched table
	 for (vector<PkgPin>::iterator I = Unmatched.begin(); 
	      I != Unmatched.end() && P == 0; I++)
	    if (I->Pkg == Name)
	       P = &*I;
	 
	 if (P == 0)
	    P = &*Unmatched.insert(Unmatched.end(),PkgPin());      
      }
      else
      {
	 P = Pins + Pkg->ID;
      }      
   }
   
   // Set..
   P->Type = Type;
   P->Priority = Priority;
   P->Data = Data;
}
									/*}}}*/
// Policy::GetMatch - Get the matching version for a package pin	/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::VerIterator pkgPolicy::GetMatch(pkgCache::PkgIterator Pkg)
{
   const Pin &PPkg = Pins[Pkg->ID];
   if (PPkg.Type != pkgVersionMatch::None)
   {
      pkgVersionMatch Match(PPkg.Data,PPkg.Type);
      return Match.Find(Pkg);
   }
   return pkgCache::VerIterator(*Pkg.Cache());
}
									/*}}}*/
// Policy::GetPriority - Get the priority of the package pin		/*{{{*/
// ---------------------------------------------------------------------
/* */
signed short pkgPolicy::GetPriority(pkgCache::PkgIterator const &Pkg)
{
   if (Pins[Pkg->ID].Type != pkgVersionMatch::None)
   {
      // In this case 0 means default priority
      if (Pins[Pkg->ID].Priority == 0)
	 return 989;
      return Pins[Pkg->ID].Priority;
   }
   
   return 0;
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

   if (FileExists(File) == false)
      return true;
   
   FileFd Fd(File,FileFd::ReadOnly);
   pkgTagFile TF(&Fd);
   if (_error->PendingError() == true)
      return false;
   
   pkgTagSection Tags;
   while (TF.Step(Tags) == true)
   {
      string Name = Tags.FindS("Package");
      if (Name.empty() == true)
	 return _error->Error(_("Invalid record in the preferences file, no Package header"));
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

      short int priority = Tags.FindI("Pin-Priority", 0);
      if (priority == 0)
      {
         _error->Warning(_("No priority (or zero) specified for pin"));
         continue;
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
