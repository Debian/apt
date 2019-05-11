// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CDROMUTL_H
#define PKGLIB_CDROMUTL_H

#include <string>


// mount cdrom, DeviceName (e.g. /dev/sr0) is optional
bool MountCdrom(std::string Path, std::string DeviceName="");
bool UnmountCdrom(std::string Path);
bool IdentCdrom(std::string CD,std::string &Res,unsigned int Version = 2);
bool IsMounted(std::string &Path);
std::string FindMountPointForDevice(const char *device);

#endif
