// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgsystem.cc,v 1.2 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/pkgsystem.h"
#endif

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
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
