/* Compatibility for systems with out Single Unix Spec statvfs */
#include <config.h>

#ifdef HAVE_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_MOUNT_H
#include <sys/param.h>
#include <sys/mount.h>
#endif

#define statvfs statfs
