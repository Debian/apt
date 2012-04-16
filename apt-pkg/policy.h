// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: policy.h,v 1.4 2001/05/07 04:24:08 jgg Exp $
/* ######################################################################

   Package Version Policy implementation

   This implements the more advanced 'Version 4' APT policy engine. The
   standard 'Version 0' engine is included inside the DepCache which is
   it's historical location.
   
   The V4 engine allows the user to completly control all aspects of
   version selection. There are three primary means to choose a version
    * Selection by version match
    * Selection by Release file match
    * Selection by origin server
   
   Each package may be 'pinned' with a single criteria, which will ultimately
   result in the selection of a single version, or no version, for each
   package.
   
   Furthermore, the default selection can be influenced by specifying
   the ordering of package files. The order is derived by reading the
   package file preferences and assigning a priority to each package 
   file.
   
   A special flag may be set to indicate if no version should be returned
   if no matching versions are found, otherwise the default matching
   rules are used to locate a hit.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_POLICY_H
#define PKGLIB_POLICY_H


#include <apt-pkg/depcache.h>
#include <apt-pkg/versionmatch.h>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
using std::vector;
#endif

class pkgPolicy : public pkgDepCache::Policy
{
   protected:

   struct Pin
   {
      pkgVersionMatch::MatchType Type;
      std::string Data;
      signed short Priority;
      Pin() : Type(pkgVersionMatch::None), Priority(0) {};
   };

   struct PkgPin : Pin
   {
      std::string Pkg;
      PkgPin(std::string const &Pkg) : Pin(), Pkg(Pkg) {};
   };
   
   Pin *Pins;
   signed short *PFPriority;
   std::vector<Pin> Defaults;
   std::vector<PkgPin> Unmatched;
   pkgCache *Cache;
   bool StatusOverride;
   
   public:

   // Things for manipulating pins
   void CreatePin(pkgVersionMatch::MatchType Type,std::string Pkg,
		  std::string Data,signed short Priority);
   pkgCache::VerIterator GetMatch(pkgCache::PkgIterator const &Pkg);

   // Things for the cache interface.
   virtual pkgCache::VerIterator GetCandidateVer(pkgCache::PkgIterator const &Pkg);
   virtual signed short GetPriority(pkgCache::PkgIterator const &Pkg);
   virtual signed short GetPriority(pkgCache::PkgFileIterator const &File);

   bool InitDefaults();
   
   pkgPolicy(pkgCache *Owner);
   virtual ~pkgPolicy() {delete [] PFPriority; delete [] Pins;};
};

bool ReadPinFile(pkgPolicy &Plcy, std::string File = "");
bool ReadPinDir(pkgPolicy &Plcy, std::string Dir = "");

#endif
