// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CDROMUTL_H
#define PKGLIB_CDROMUTL_H

#include <apt-pkg/macros.h>

#include <string>


// mount cdrom, DeviceName (e.g. /dev/sr0) is optional
APT_PUBLIC bool MountCdrom(std::string Path, std::string DeviceName="");
APT_PUBLIC bool UnmountCdrom(std::string Path);
APT_PUBLIC bool IdentCdrom(std::string CD,std::string &Res,unsigned int Version = 2);
APT_PUBLIC bool IsMounted(std::string &Path);
APT_PUBLIC std::string FindMountPointForDevice(const char *device);

#endif
