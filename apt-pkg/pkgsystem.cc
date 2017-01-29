// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debsystem.h>
#include <apt-pkg/error.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgsystem.h>

#include <cassert>
#include <cstring>
#include <map>
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
// pkgSystem::Set/GetVersionMapping - for internal/external communication/*{{{*/
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

bool pkgSystem::LockInner()				/*{{{*/
{
   debSystem * const deb = dynamic_cast<debSystem *>(this);
   if (deb != NULL)
      return deb->LockInner();
   return _error->Error("LockInner is not implemented");
}
									/*}}}*/
bool pkgSystem::UnLockInner(bool NoErrors)				/*{{{*/
{
   debSystem * const deb = dynamic_cast<debSystem *>(this);
   if (deb != NULL)
      return deb->UnLockInner(NoErrors);
   return _error->Error("UnLockInner is not implemented");
}
									/*}}}*/
bool pkgSystem::IsLocked() 						/*{{{*/
{
   debSystem * const deb = dynamic_cast<debSystem *>(this);
   if (deb != NULL)
      return deb->IsLocked();
   return true;
}
									/*}}}*/
pkgSystem::~pkgSystem() {}
