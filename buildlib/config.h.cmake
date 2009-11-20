/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#cmakedefine WORDS_BIGENDIAN

/* The following 4 are only used by inttypes.h shim if the system lacks
   inttypes.h */
/* The number of bytes in a usigned char.  */
#cmakedefine SIZEOF_CHAR

/* The number of bytes in a unsigned int.  */
#cmakedefine SIZEOF_INT

/* The number of bytes in a unsigned long.  */
#cmakedefine SIZEOF_LONG

/* The number of bytes in a unsigned short.  */
#cmakedefine SIZEOF_SHORT

/* Define if we have the timegm() function */
#cmakedefine HAVE_TIMEGM

/* These two are used by the statvfs shim for glibc2.0 and bsd */
/* Define if we have sys/vfs.h */
#cmakedefine HAVE_VFS_H

/* Define if we have sys/mount.h */
#cmakedefine HAVE_MOUNT_H

/* Define if we have enabled pthread support */
#cmakedefine HAVE_PTHREAD

/* Define the arch name string */
#define COMMON_ARCH "${COMMON_ARCH}"

/* The version number string */
#define VERSION "${VERSION}"

/* The package name string */
#define PACKAGE "${PACKAGE}"
