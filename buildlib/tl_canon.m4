# Our own versions of the other canonicalizing stuff

dnl replace AC_CANONICAL_xxx

undefine([AC_CANONICAL_HOST])
define([AC_CANONICAL_HOST], [tl_CANONICAL_HOST])
undefine([AC_CANONICAL_BUILD])
define([AC_CANONICAL_BUILD], [tl_CANONICAL_BUILD])
undefine([AC_CANONICAL_TARGET])
define([AC_CANONICAL_TARGET], [tl_CANONICAL_TARGET])
undefine([AC_CANONICAL_SYSTEM])
define([AC_CANONICAL_SYSTEM], [tl_CANONICAL_SYSTEM])

dnl Canonicalize the host, target, and build system types.
AC_DEFUN(tl_CANONICAL_SYSTEM,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_PROVIDE([AC_CANONICAL_SYSTEM])dnl
AC_BEFORE([$0], [AC_ARG_PROGRAM])
# Do some error checking and defaulting for the host and target type.
# The inputs are:
#    configure --host=HOST --target=TARGET --build=BUILD NONOPT
#
# The rules are:
# 1. You are not allowed to specify --host, --target, and nonopt at the
#    same time.
# 2. Host defaults to nonopt.
# 3. If nonopt is not specified, then host defaults to the current host,
#    as determined by config.guess.
# 4. Target and build default to nonopt.
# 5. If nonopt is not specified, then target and build default to host.

# The aliases save the names the user supplied, while $host etc.
# will get canonicalized.
case $host---$target---$nonopt in
NONE---*---* | *---NONE---* | *---*---NONE) ;;
*) AC_MSG_ERROR(can only configure for one host and one target at a time) ;;
esac

tl_CANONICAL_HOST
tl_CANONICAL_TARGET
tl_CANONICAL_BUILD
test "$host_alias" != "$target_alias" &&
  test "$program_prefix$program_suffix$program_transform_name" = \
    NONENONEs,x,x, &&
  program_prefix=${target_alias}-
AC_CHECK_TOOL_PREFIX
])

dnl Subroutines of tl_CANONICAL_SYSTEM.

AC_DEFUN(tl_CANONICAL_HOST,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_PROVIDE([AC_CANONICAL_HOST])dnl

# Make sure we can run config.sub.
if $ac_config_sub sun4 >/dev/null 2>&1; then :
else AC_MSG_ERROR(can not run $ac_config_sub)
fi

AC_MSG_CHECKING(host system type)

dnl Set host_alias.

if test "${GCC-no}" = "yes" -a "$nonopt" = "NONE"; then
changequote(, )dnl
  libgcc="`${CC} --print-libgcc-file-name`"
  host_alias="`expr ${libgcc} : '.*/gcc-lib/\([^/]*\)/.*'`"
  case ${host_alias} in
   *-linux{,elf,aout})
    host_alias="`echo ${host_alias} | sed 's/\([^-]*\)-linux.*/\1/'`"
changequote([, ])dnl
    if ar p "${libgcc}" __main.o 2>/dev/null | file - 2>/dev/null | grep ELF >/dev/null; then
      host_alias="${host_alias}-linux"
    else
      host_alias="${host_alias}-linuxaout"
    fi ;;
  esac
  host_guessed=y
else
  host_alias=$host
  case "$host_alias" in
  NONE)
    case "$nonopt" in
    NONE)
      if host_alias=`$ac_config_guess`; then host_guessed=y
      else AC_MSG_ERROR(can not guess host type; you must specify one)
      fi ;;
    *) host_alias=$nonopt ;;
    esac ;;
  esac
fi

dnl Set the other host vars.
changequote(<<, >>)dnl
host=`$ac_config_sub $host_alias`
host_cpu=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
host_vendor=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
host_os=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
AC_MSG_RESULT($host)
AC_SUBST(host)dnl
AC_SUBST(host_alias)dnl
AC_SUBST(host_cpu)dnl
AC_SUBST(host_vendor)dnl
AC_SUBST(host_os)dnl
])

dnl Internal use only.
AC_DEFUN(tl_CANONICAL_TARGET,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_PROVIDE([AC_CANONICAL_TARGET])dnl
AC_MSG_CHECKING(target system type)

dnl Set target_alias.
target_alias=$target
case "$target_alias" in
NONE)
  case $nonopt in
  NONE)
  target_cpu="`dpkg --print-architecture`"
  if test "$target_cpu" = ""; then
   target_alias=$host_alias
  else
   target_alias="`echo ${host_alias} | sed 's/[^-]*-/${target_cpu}-/'`"
  fi
  ;;
  *) target_alias=$nonopt ;;
  esac ;;
esac

dnl Set the other target vars.
if test $target_alias = $host_alias; then
  target=$host
  target_cpu=$host_cpu
  target_vendor=$host_vendor
  target_os=$host_os
elif test $target_alias = "$build_alias"; then
  target=$build
  target_cpu=$build_cpu
  target_vendor=$build_vendor
  target_os=$build_os
else
changequote(<<, >>)dnl
  target=`$ac_config_sub $target_alias`
  target_cpu=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
  target_vendor=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
  target_os=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
fi
AC_MSG_RESULT($target)
AC_SUBST(target)dnl
AC_SUBST(target_alias)dnl
AC_SUBST(target_cpu)dnl
AC_SUBST(target_vendor)dnl
AC_SUBST(target_os)dnl
])

AC_DEFUN(tl_CANONICAL_BUILD,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_PROVIDE([AC_CANONICAL_BUILD])dnl

# Make sure we can run config.sub.
#if $ac_config_sub sun4 >/dev/null 2>&1; then :
#else AC_MSG_ERROR(can not run $ac_config_sub)
#fi

AC_MSG_CHECKING(build system type)

dnl Set build_alias.
build_alias=$build
case "$build_alias" in
NONE)
  case $nonopt in
  NONE)
    if test "$host_guessed" = "y"; then
      build_alias=$host_alias
    else
      if build_alias=`$ac_config_guess`; then :
      else build_alias=$host_alias
      fi
    fi ;;
  *) build_alias=$nonopt ;;
  esac ;;
esac

dnl Set the other build vars.
if test $build_alias = $host_alias; then
  build=$host
  build_cpu=$host_cpu
  build_vendor=$host_vendor
  build_os=$host_os
else
changequote(<<, >>)dnl
  build=`$ac_config_sub $build_alias`
  build_cpu=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
  build_vendor=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
  build_os=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
fi
AC_MSG_RESULT($build)
AC_SUBST(build)dnl
AC_SUBST(build_alias)dnl
AC_SUBST(build_cpu)dnl
AC_SUBST(build_vendor)dnl
AC_SUBST(build_os)dnl
])
