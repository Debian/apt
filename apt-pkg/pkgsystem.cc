// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgsystem.cc,v 1.3 2004/02/27 00:43:16 mdz Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
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
pkgSystem::pkgSystem()
{
   assert(GlobalListLen < sizeof(SysList)/sizeof(*SysList));
   SysList[GlobalListLen] = this;
   GlobalListLen++;
}
									/*}}}*/
// System::GetSystem - Get the named system				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSystem *pkgSystem::GetSystem(const char *Label)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(SysList[I]->Label,Label) == 0)
	 return SysList[I];
   return 0;   
}
									/*}}}*/
