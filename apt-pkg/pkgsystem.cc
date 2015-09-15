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

#include <cassert>
#include <cstring>
									/*}}}*/

pkgSystem *_system = 0;
static pkgSystem *SysList[10];
pkgSystem **pkgSystem::GlobalList = SysList;
unsigned long pkgSystem::GlobalListLen = 0;

// System::pkgSystem - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Add it to the global list.. */
pkgSystem::pkgSystem(char const * const label, pkgVersioningSystem * const vs) :
   Label(label), VS(vs), d(NULL)
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

pkgSystem::~pkgSystem() {}
