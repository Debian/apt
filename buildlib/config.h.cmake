/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#cmakedefine WORDS_BIGENDIAN

/* Define if we have the timegm() function */
#cmakedefine HAVE_TIMEGM

/* Define if we have enabled pthread support */
#cmakedefine HAVE_PTHREAD

/* Define the arch name string */
#define COMMON_ARCH "${COMMON_ARCH}"

/* The version number string */
#define VERSION "${VERSION}"

/* The package name string */
#define PACKAGE "${PACKAGE}"
