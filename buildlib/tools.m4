# tl_CHECK_TOOL_PREFIX will work _BEFORE_ AC_CANONICAL_HOST, etc., has been
# called. It should be called again after these have been called.
#
# Basically we want to check if the host alias specified by the user is
# different from the build alias. The rules work like this:-
#
# If host is not specified, it defaults to NONOPT
# If build is not specified, it defaults to NONOPT
# If nonopt is not specified, we guess all other values

dnl Replace AC_CHECK_TOOL_PREFIX
undefine([AC_CHECK_TOOL_PREFIX])
define([AC_CHECK_TOOL_PREFIX], [tl_CHECK_TOOL_PREFIX])

AC_DEFUN(tl_CHECK_TOOL_PREFIX,
[AC_PROVIDE([AC_CHECK_TOOL_PREFIX])
AC_BEFORE([AC_CANONICAL_HOST])
AC_BEFORE([AC_CANONICAL_BUILD])
dnl Quick check
if test "$host_alias" = ""; then
  if test $host = NONE; then
    thost=$nonopt
  else
    thost=$host
  fi
  if test $thost != $build -a $thost != NONE; then
    ac_tool_prefix=${thost}-
    ac_tool_dir=${thost}
  else
    ac_tool_prefix=
    ac_tool_dir=
  fi
else
  if test $host != $build; then
    ac_tool_prefix=${host_alias}-
    ac_tool_dir=${host_alias}
  else
    ac_tool_prefix=
    ac_tool_dir=
  fi
fi
])

dnl replacement for AC_CHECK_TOOL
undefine([AC_CHECK_TOOL])
define([AC_CHECK_TOOL], [tl_CHECK_TOOL($1, $2, $3, $4)])

dnl tl_CHECK_TOOL - AC_CHECK_TOOL, with a couple of extra checks
dnl tl_CHECK_TOOL(VARIABLE, PROG-TO-CHECK-FOR[, VALUE-IF-NOT-FOUND [, PATH
dnl [, REJECT]])
AC_DEFUN(tl_CHECK_TOOL,
[AC_REQUIRE([AC_CHECK_TOOL_PREFIX])
AC_CHECK_PROG($1, ${ac_tool_prefix}$2, ${ac_tool_prefix}$2,
	      ifelse([$3], , [$2], ), $4, $5)
if test -z "$ac_cv_prog_$1_dir";then ac_cv_prog_$1_dir=""; fi
if test "$ac_tool_dir" != ""; then
  if test -z "$ac_cv_prog_$1" -a "$5" != "/usr/${ac_tool_dir}/bin/$2" -a \
	"$5" != "/usr/local/${ac_tool_dir}/bin/$2"; then
    if test -f /usr/${ac_tool_dir}/bin/$2; then $1="/usr/${ac_tool_dir}/bin/$2"; ac_cv_prog_$1_dir=/usr/${ac_tool_dir}
    elif test -f /usr/local/${ac_tool_dir}/bin/$2; then $1="/usr/local/${ac_tool_dir}/bin/$2"; ac_cv_prog_$1_dir=/usr/local/${ac_tool_dir}
    fi
  fi
fi
ifelse([$3], , , [
if test -z "$ac_cv_prog_$1"; then
if test -n "$ac_tool_prefix"; then
  AC_CHECK_PROG($1, $2, $2, $3, $4, $5)
else
  $1="$3"
fi
fi])
])

dnl tl_CHECK_TOOLS -
dnl  do a tl_CHECK_TOOL for multiple tools (like AC_CHECK_PROGS)
dnl tl_CHECK_TOOLS(VARIABLE, PROGS-TO-CHECK-FOR [, VALUE-IF-NOT-FOUND
dnl		   [, PATH]])
AC_DEFUN(tl_CHECK_TOOLS,
[for ac_tool in $2
do
tl_CHECK_TOOL($1, [$]ac_tool, [$]ac_tool, , $4)
test -n "[$]$1" && break
done
ifelse([$3], , , [test -n "[$]$1" || $1="$3"
])])

dnl replace AC_PROG_CC and AC_PROG_CXX
undefine([AC_PROG_CC])
define([AC_PROG_CC], [tl_PROG_CC])
undefine([AC_PROG_CXX])
define([AC_PROG_CXX], [tl_PROG_CXX])

dnl tl_PROG_CC, tl_PROG_CXX - same as old AC_PROG_CC and AC_PROG_CXX, but
dnl use AC_CHECK_TOOL/tl_CHECK_TOOLS instead of AC_CHECK_PROG, etc.
AC_DEFUN(tl_PROG_CC,
[AC_BEFORE([$0], [AC_PROG_CPP])dnl
AC_PROVIDE([AC_PROG_CC])dnl
tl_CHECK_TOOL(CC, gcc, gcc)
if test -z "$CC"; then
  AC_CHECK_TOOL(CC, cc, cc, , , /usr/ucb/cc)
  test -z "$CC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])
fi
if test -n "$ac_tool_prefix" -a "`echo $CC | grep '$ac_tool_prefix'`" = "" \
	-a "`echo $CC | grep -- '-b'`" = ""; then
  if test -z "$ac_cv_prog_CC_dir" && $CC -v 2>&1 | grep gcc >/dev/null 2>&1 ; then
    AC_CACHE_CHECK([if $CC -b${ac_tool_dir} works], tl_cv_prog_cc_bhost,[
    old_cc="${CC}"
    CC="${CC} -b${ac_tool_dir}"
    AC_LANG_SAVE
    AC_LANG_C
    AC_TRY_COMPILER([main(){return(0);}], tl_cv_prog_cc_bhost, ac_cv_prog_cc_cross)
    AC_LANG_RESTORE])
    if test $tl_cv_prog_cc_bhost = "yes"; then
      ac_cv_prog_cc_works=yes
      cctest=yes
    else
      CC="${old_cc}"
    fi
  fi
fi

if test "$cctest" != "yes"; then
  tl_PROG_CC_WORKS
fi
AC_PROG_CC_GNU

if test $ac_cv_prog_gcc = yes; then
  GCC=yes
dnl Check whether -g works, even if CFLAGS is set, in case the package
dnl plays around with CFLAGS (such as to build both debugging and
dnl normal versions of a library), tasteless as that idea is.
  ac_test_CFLAGS="${CFLAGS+set}"
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS=
  AC_PROG_CC_G
  if test "$ac_test_CFLAGS" = set; then
    CFLAGS="$ac_save_CFLAGS"
  elif test $ac_cv_prog_cc_g = yes; then
    CFLAGS="-g -O2"
  else
    CFLAGS="-O2"
  fi
else
  GCC=
  test "${CFLAGS+set}" = set || CFLAGS="-g"
fi
])

AC_DEFUN(tl_PROG_CXX,
[AC_BEFORE([$0], [AC_PROG_CXXCPP])dnl
AC_PROVIDE([AC_PROG_CXX])dnl
tl_CHECK_TOOLS(CXX, $CCC c++ g++ gcc CC cxx cc++, gcc)
if test -n "$CXX"; then
  if test -n "$ac_tool_prefix" -a "`echo $CXX | grep '$ac_tool_prefix'`" = "" \
	-a "`echo $CXX | grep -- '-b'`" = ""; then
    if test -z "$ac_cv_prog_CXX_dir" && $CXX -v 2>&1 | grep gcc >/dev/null 2>&1; then
      AC_CACHE_CHECK([if $CXX -b${ac_tool_dir} works], tl_cv_prog_cxx_bhost,[
      old_cxx="${CXX}"
      CXX="${CXX} -b${ac_tool_dir}"
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_COMPILER([main(){return(0);}], tl_cv_prog_cxx_bhost, ac_cv_prog_cxx_cross)
      AC_LANG_RESTORE])
      if test $tl_cv_prog_cxx_bhost = "yes"; then
	ac_cv_prog_cxx_works=yes
	cxxtest=yes
      else
	CXX="${old_cxx}"
      fi
    fi
  fi
  
  if test "$cxxtest" != "yes"; then
    tl_PROG_CXX_WORKS
  fi
  AC_PROG_CXX_GNU
  
  if test $ac_cv_prog_gxx = yes; then
    GXX=yes
dnl Check whether -g works, even if CXXFLAGS is set, in case the package
dnl plays around with CXXFLAGS (such as to build both debugging and
dnl normal versions of a library), tasteless as that idea is.
    ac_test_CXXFLAGS="${CXXFLAGS+set}"
    ac_save_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS=
    AC_PROG_CXX_G
    if test "$ac_test_CXXFLAGS" = set; then
      CXXFLAGS="$ac_save_CXXFLAGS"
    elif test $ac_cv_prog_cxx_g = yes; then
      CXXFLAGS="-g -O2"
    else
      CXXFLAGS="-O2"
    fi
  else
    GXX=
    test "${CXXFLAGS+set}" = set || CXXFLAGS="-g"
  fi
fi
])

AC_DEFUN(tl_PROG_CC_WORKS,
[AC_PROVIDE(AC_PROG_CC_WORKS)
AC_CACHE_CHECK([whether the C compiler ($CC $CFLAGS $LDFLAGS) works],
	ac_cv_prog_cc_works, [
AC_LANG_SAVE
AC_LANG_C
AC_TRY_COMPILER([main(){return(0);}], ac_cv_prog_cc_works, ac_cv_prog_cc_cross)
AC_LANG_RESTORE
if test $ac_cv_prog_cc_works = no; then
  AC_MSG_ERROR([installation or configuration problem: C compiler cannot create executables.])
fi])
AC_MSG_CHECKING([whether the C compiler ($CC $CFLAGS $LDFLAGS) is a cross-compiler])
AC_MSG_RESULT($ac_cv_prog_cc_cross)
cross_compiling=$ac_cv_prog_cc_cross
])

AC_DEFUN(tl_PROG_CXX_WORKS,
[AC_PROVIDE(AC_PROG_CXX_WORKS)
AC_CACHE_CHECK([whether the C++ compiler ($CXX $CXXFLAGS $LDFLAGS) works],
	ac_cv_prog_cxx_works, [
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_TRY_COMPILER([main(){return(0);}], ac_cv_prog_cxx_works, ac_cv_prog_cxx_cross)
AC_LANG_RESTORE
if test $ac_cv_prog_cxx_works = no; then
  AC_MSG_ERROR([installation or configuration problem: C++ compiler cannot create executables.])
fi])
AC_MSG_CHECKING([whether the C++ compiler ($CXX $CXXFLAGS $LDFLAGS) is a cross-compiler])
AC_MSG_RESULT($ac_cv_prog_cxx_cross)
cross_compiling=$ac_cv_prog_cxx_cross
])
