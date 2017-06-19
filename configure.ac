ad
dnl Process this file with autoconf to produce a configure script.
dnl The ONLY thing this is used for is to configure for different
dnl linux architectures and configurations, it is not used to make the
dnl code more portable

dnl You MUST have an environment that has all the POSIX functions and
dnl some of the more popular bsd/sysv ones (like select). You'll also
dnl need a C++ compiler that is semi-standard conformant, exceptions are
dnl not used but STL is.

dnl 'make -f Makefile startup' will generate the configure file from
dnl configure.ac correctly and can be run at any time

AC_PREREQ([2.68])
AC_INIT
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR(buildlib)
AC_CONFIG_HEADER(include/config.h:buildlib/config.h.in include/apti18n.h:buildlib/apti18n.h.in)

PACKAGE="apt"
PACKAGE_VERSION="1.2.24"
PACKAGE_MAIL="APT Development Team <deity@lists.debian.org>"
AC_DEFINE_UNQUOTED(PACKAGE,"$PACKAGE")
AC_DEFINE_UNQUOTED(PACKAGE_VERSION,"$PACKAGE_VERSION")
AC_DEFINE_UNQUOTED(PACKAGE_MAIL,"$PACKAGE_MAIL")
AC_SUBST(PACKAGE)
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(PACKAGE_MAIL)

dnl Check the archs, we want the target type.
AC_CANONICAL_TARGET

dnl Check our C compiler

AC_PROG_CC
AC_SEARCH_LIBS([strerror],[cposix])

dnl check for large file support and enable it if possible
dnl do this early as other stuff might depend on it
AC_SYS_LARGEFILE

dnl Check for other programs
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_RANLIB
AC_CHECK_TOOL(AR,ar,"ar")

dnl Checks for sockets
SAVE_LIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS(gethostbyname,nsl)
AC_SEARCH_LIBS(connect,socket)
SOCKETLIBS="$LIBS"
AC_SUBST(SOCKETLIBS)
LIBS="$SAVE_LIBS"

dnl Checks for pthread -- disabled due to glibc bugs jgg
dnl AC_CHECK_LIB(pthread, pthread_create,[AC_DEFINE(HAVE_PTHREAD) PTHREADLIB="-lpthread"])
AC_SUBST(PTHREADLIB)
dnl if test "$PTHREADLIB" != "-lpthread"; then
dnl   AC_MSG_ERROR(failed: I need posix threads, pthread)
dnl fi

dnl Check for BDB
saveLIBS="$LIBS"
LIBS="$LIBS -ldb"

AC_CHECK_HEADER(db.h,
     [AC_MSG_CHECKING(if we can link against BerkeleyDB)
      AC_LINK_IFELSE(
      [AC_LANG_PROGRAM(
       [#include <db.h>],
       [int r, s, t; db_version(&r, &s, &t);]
      )],
      [AC_DEFINE(HAVE_BDB)
       BDBLIB="-ldb"
       AC_MSG_RESULT(yes)],
      [BDBLIB=""
       AC_MSG_RESULT(no)]
    )]
)

LIBS="$saveLIBS"

AC_CHECK_LIB(curl, curl_easy_init,
     [AC_CHECK_HEADER(curl/curl.h,
        curl_ok=yes,
        curl_ok=no)],
     AC_MSG_ERROR([failed: I need CURL due https support]),
)

AC_LANG_PUSH([C++])
AC_CHECK_HEADER(gtest/gtest.h,,
     AC_MSG_WARN([failed: I need gtest (packaged as libgtest-dev) for unit testing]),
)
AC_LANG_POP([C++])


AC_SUBST(BDBLIB)

HAVE_ZLIB=no
AC_CHECK_LIB(z, gzopen,
	[AC_CHECK_HEADER(zlib.h, [HAVE_ZLIB=yes], AC_MSG_ERROR([failed: zlib.h not found]))],
	AC_MSG_ERROR([failed: Need libz]))
AC_SUBST(HAVE_ZLIB)
if test "x$HAVE_ZLIB" = "xyes"; then
	AC_DEFINE(HAVE_ZLIB)
fi

HAVE_LZ4=no
AC_CHECK_LIB(lz4, LZ4F_createCompressionContext,[AC_CHECK_HEADER(lz4frame.h, [HAVE_LZ4=yes], [])], [])
AC_SUBST(HAVE_LZ4)
if test "x$HAVE_LZ4" = "xyes"; then
	AC_DEFINE(HAVE_LZ4)
fi

HAVE_BZ2=no
AC_CHECK_LIB(bz2, BZ2_bzopen,[AC_CHECK_HEADER(bzlib.h, [HAVE_BZ2=yes], [])], [])
AC_SUBST(HAVE_BZ2)
if test "x$HAVE_BZ2" = "xyes"; then
	AC_DEFINE(HAVE_BZ2)
fi

HAVE_LZMA=no
AC_CHECK_LIB(lzma, lzma_easy_encoder,[AC_CHECK_HEADER(lzma.h, [HAVE_LZMA=yes], [])], [])
AC_SUBST(HAVE_LZMA)
if test "x$HAVE_LZMA" = "xyes"; then
	AC_DEFINE(HAVE_LZMA)
fi

dnl Converts the ARCH to be something singular for this general CPU family
dnl This is often the dpkg architecture string.
dnl First check against the full canonical canoncial-system-type in $target
dnl and if that fails, just look for the cpu
AC_MSG_CHECKING(debian architecture)
archset="`dpkg-architecture -qDEB_HOST_ARCH`"
if test "x$archset" = "x"; then
   AC_MSG_ERROR([failed: use --host= or output from dpkg-architecture])
fi
AC_MSG_RESULT($archset)
AC_DEFINE_UNQUOTED(COMMON_ARCH,"$archset")

dnl Single Unix Spec statvfs
AC_CHECK_FUNC(statvfs,[HAVE_STATVFS=yes])
AC_SUBST(HAVE_STATVFS)

dnl Arg, linux and bsd put their statfs function in different places
if test x"$HAVE_STATVFS" != x"yes"; then
   AC_EGREP_HEADER(statfs,sys/vfs.h,[AC_DEFINE(HAVE_VFS_H)],[
   AC_EGREP_HEADER(statfs,sys/mount.h,[AC_DEFINE(HAVE_MOUNT_H)],[AC_MSG_ERROR(failed: Need statvfs)])
   ])
fi

AC_CHECK_MEMBERS([struct statfs.f_type],,,
    [$ac_includes_default
     #include <sys/vfs.h>])

dnl We should use the real timegm function if we have it.
AC_CHECK_FUNC(timegm,AC_DEFINE(HAVE_TIMEGM))
AC_SUBST(HAVE_TIMEGM)

dnl Check the architecture
AC_C_BIGENDIAN

dnl HP-UX sux..
AC_MSG_CHECKING(for missing socklen_t)
AC_EGREP_HEADER(socklen_t, sys/socket.h,[AC_MSG_RESULT(no)],[
	AC_DEFINE(NEED_SOCKLEN_T_DEFINE)
	NEED_SOCKLEN_T_DEFINE=yes
        AC_MSG_RESULT(missing.)])
AC_SUBST(NEED_SOCKLEN_T_DEFINE)

dnl HP-UX needs -d_XOPEN_SOURCE_EXTENDED for h_errno
AC_MSG_CHECKING(for h_errno)
AC_EGREP_HEADER(h_errno, netdb.h, [AC_MSG_RESULT(normal)],
   [CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE_EXTENDED"
    AC_EGREP_HEADER(h_errno, netdb.h,
       [AC_MSG_RESULT(needs _XOPEN_SOURCE_EXTENDED)],
       [AC_MSG_ERROR("not found.")])
   ])


dnl check for setuid checking function
AC_CHECK_FUNCS(getresuid getresgid setresuid setresgid)
AC_SUBST(HAVE_GETRESUID)
AC_SUBST(HAVE_GETRESGID)
AC_SUBST(HAVE_SETRESUID)
AC_SUBST(HAVE_SETRESGID)

AC_CHECK_FUNC(ptsname_r)
AC_SUBST(HAVE_PTSNAME_R)

dnl Check for doxygen
AC_PATH_PROG(DOXYGEN, doxygen)

dnl Check for the XSLTProc tool needed to build man pages together with po4a
AC_PATH_PROG(XSLTPROC,xsltproc)
AC_PATH_PROG(W3M, w3m)

dnl Check for the po4a tool needed to build man pages
AC_PATH_PROG(PO4A,po4a)

dnl Check for graphviz
AC_CHECK_PROG([HAVE_DOT], [dot], [YES], [NO])
AC_PATH_PROG([DOT], [dot], [])
DOTDIR=$(dirname $DOT)
AC_SUBST(DOTDIR)

ah_NUM_PROCS
ah_GCC3DEP

AM_GNU_GETTEXT(external)
if test x"$USE_NLS" = "xyes"; then
   AC_DEFINE(USE_NLS)
fi
AC_SUBST(USE_NLS)
AC_PATH_PROG(BASH, bash)

AC_CONFIG_FILES([environment.mak:buildlib/environment.mak.in makefile:buildlib/makefile.in doc/Doxyfile:doc/Doxyfile.in])
AC_OUTPUT
