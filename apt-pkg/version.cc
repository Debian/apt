// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version - Versioning system..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/version.h>

#include <stdlib.h>
#include <string.h>
									/*}}}*/
    
static pkgVersioningSystem *VSList[10];
pkgVersioningSystem **pkgVersioningSystem::GlobalList = VSList;
unsigned long pkgVersioningSystem::GlobalListLen = 0;

// pkgVS::pkgVersioningSystem - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Link to the global list of versioning systems supported */
pkgVersioningSystem::pkgVersioningSystem() : Label(NULL)
{
   VSList[GlobalListLen] = this;
   ++GlobalListLen;
}
									/*}}}*/
// pkgVS::GetVS - Find a VS by name					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgVersioningSystem *pkgVersioningSystem::GetVS(const char *Label)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(VSList[I]->Label,Label) == 0)
	 return VSList[I];
   return 0;
}
									/*}}}*/


pkgVersioningSystem::~pkgVersioningSystem() {}
