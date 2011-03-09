// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdromutl.h,v 1.3 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################

   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CDROMUTL_H
#define PKGLIB_CDROMUTL_H

#include <string>

using std::string;

// mount cdrom, DeviceName (e.g. /dev/sr0) is optional
bool MountCdrom(string Path, string DeviceName="");
bool UnmountCdrom(string Path);
bool IdentCdrom(string CD,string &Res,unsigned int Version = 2);
bool IsMounted(string &Path);
string FindMountPointForDevice(const char *device);

#endif
