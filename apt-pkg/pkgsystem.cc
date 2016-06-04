// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgsystem.cc,v 1.3 2004/02/27 00:43:16 mdz Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/debsystem.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/macros.h>

#include <map>
#include <cassert>
#include <cstring>
									/*}}}*/

pkgSystem *_system = 0;
static pkgSystem *SysList[10];
pkgSystem **pkgSystem::GlobalList = SysList;
unsigned long pkgSystem::GlobalListLen = 0;

class APT_HIDDEN pkgSystemPrivate					/*{{{*/
{
public:
   typedef decltype(pkgCache::Version::ID) idtype;
   std::map<idtype,idtype> idmap;
   pkgSystemPrivate() {}
};
									/*}}}*/
// System::pkgSystem - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Add it to the global list.. */
pkgSystem::pkgSystem(char const * const label, pkgVersioningSystem * const vs) :
   Label(label), VS(vs), d(new pkgSystemPrivate())
{
   assert(GlobalListLen < sizeof(SysList)/sizeof(*SysList));
   SysList[GlobalListLen] = this;
   ++GlobalListLen;
}
									/*}}}*/
// System::GetSystem - Get the named system				/*{{{*/
// ---------------------------------------------------------------------
/* */
APT_PURE pkgSystem *pkgSystem::GetSystem(const char *Label)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(SysList[I]->Label,Label) == 0)
	 return SysList[I];
   return 0;   
}
									/*}}}*/
bool pkgSystem::MultiArchSupported() const				/*{{{*/
{
   debSystem const * const deb = dynamic_cast<debSystem const *>(this);
   if (deb != NULL)
      return deb->SupportsMultiArch();
   return true;
}
									/*}}}*/
std::vector<std::string> pkgSystem::ArchitecturesSupported() const	/*{{{*/
{
   debSystem const * const deb = dynamic_cast<debSystem const *>(this);
   if (deb != NULL)
      return deb->SupportedArchitectures();
   return {};
}
									/*}}}*/
// pkgSystem::Set/GetVersionMapping - for internal/external communcation/*{{{*/
void pkgSystem::SetVersionMapping(map_id_t const in, map_id_t const out)
{
   if (in == out)
      return;
   d->idmap.emplace(in, out);
}
map_id_t pkgSystem::GetVersionMapping(map_id_t const in) const
{
   auto const o = d->idmap.find(in);
   return (o == d->idmap.end()) ? in : o->second;
}
									/*}}}*/

pkgSystem::~pkgSystem() {}
