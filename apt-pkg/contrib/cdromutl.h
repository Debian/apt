// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdromutl.h,v 1.1 1998/11/29 01:19:27 jgg Exp $
/* ######################################################################

   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CDROMUTL_H
#define PKGLIB_ACQUIRE_METHOD_H

#include <string>

#ifdef __GNUG__
#pragma interface "apt-pkg/cdromutl.h"
#endif 

bool MountCdrom(string Path);
bool UnmountCdrom(string Path);
bool IdentCdrom(string CD,string &Res);

#endif
