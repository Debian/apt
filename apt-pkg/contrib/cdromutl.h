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

#ifndef APT_8_CLEANER_HEADERS
using std::string;
#endif

// mount cdrom, DeviceName (e.g. /dev/sr0) is optional
bool MountCdrom(std::string Path, std::string DeviceName="");
bool UnmountCdrom(std::string Path);
bool IdentCdrom(std::string CD,std::string &Res,unsigned int Version = 2);
bool IsMounted(std::string &Path);
std::string FindMountPointForDevice(const char *device);

#endif
