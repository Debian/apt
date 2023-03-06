// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+
// Description								/*{{{*/
/* ######################################################################
   
   Macros Header - Various useful macro definitions

   This file had this historic note, but now includes further changes
   under the GPL-2.0+:

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Brian C. White.
   
   ##################################################################### */
									/*}}}*/
// Private header
#ifndef MACROS_H
#define MACROS_H

/* Useful count macro, use on an array of things and it will return the
   number of items in the array */
#define APT_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

// Flag Macros
#define	FLAG(f)			(1L << (f))
#define	SETFLAG(v,f)	((v) |= FLAG(f))
#define CLRFLAG(v,f)	((v) &=~FLAG(f))
#define	CHKFLAG(v,f)	((v) &  FLAG(f) ? true : false)

#ifdef __GNUC__
#define APT_GCC_VERSION (__GNUC__ << 8 | __GNUC_MINOR__)
#else
#define APT_GCC_VERSION 0
#endif

#ifdef APT_COMPILING_APT
/* likely() and unlikely() can be used to mark boolean expressions
   as (not) likely true which will help the compiler to optimise */
#if APT_GCC_VERSION >= 0x0300
	#define likely(x)	__builtin_expect (!!(x), 1)
	#define unlikely(x)	__builtin_expect (!!(x), 0)
#else
	#define likely(x)	(x)
	#define unlikely(x)	(x)
#endif
#endif

#if APT_GCC_VERSION >= 0x0300
	#define APT_DEPRECATED	__attribute__ ((deprecated))
	#define APT_DEPRECATED_MSG(X)	__attribute__ ((deprecated(X)))
	// __attribute__((const)) is too dangerous for us, we end up using it wrongly
	#define APT_PURE	__attribute__((pure))
	#define APT_NORETURN	__attribute__((noreturn))
	#define APT_PRINTF(n)	__attribute__((format(printf, n, n + 1)))
	#define APT_WEAK        __attribute__((weak));
	#define APT_UNUSED      __attribute__((unused))
#else
	#define APT_DEPRECATED
	#define APT_DEPRECATED_MSG
	#define APT_PURE
	#define APT_NORETURN
	#define APT_PRINTF(n)
	#define APT_WEAK
	#define APT_UNUSED
#endif

#if APT_GCC_VERSION > 0x0302
	#define APT_NONNULL(...)	__attribute__((nonnull(__VA_ARGS__)))
	#define APT_MUSTCHECK		__attribute__((warn_unused_result))
#else
	#define APT_NONNULL(...)
	#define APT_MUSTCHECK
#endif

#if APT_GCC_VERSION >= 0x0400
	#define APT_SENTINEL	__attribute__((sentinel))
	#define APT_PUBLIC __attribute__ ((visibility ("default")))
	#define APT_HIDDEN __attribute__ ((visibility ("hidden")))
#else
	#define APT_SENTINEL
	#define APT_PUBLIC
	#define APT_HIDDEN
#endif

// cold functions are unlikely() to be called
#if APT_GCC_VERSION >= 0x0403
	#define APT_COLD	__attribute__ ((__cold__))
	#define APT_HOT		__attribute__ ((__hot__))
#else
	#define APT_COLD
	#define APT_HOT
#endif


#if __GNUC__ >= 4
	#define APT_IGNORE_DEPRECATED_PUSH \
		_Pragma("GCC diagnostic push") \
		_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
	#define APT_IGNORE_DEPRECATED_POP \
		_Pragma("GCC diagnostic pop")
	/* gcc has various problems with this shortcut, so prefer the long form */
	#define APT_IGNORE_DEPRECATED(XXX) \
		APT_IGNORE_DEPRECATED_PUSH \
		XXX \
		APT_IGNORE_DEPRECATED_POP
#else
	#define APT_IGNORE_DEPRECATED_PUSH
	#define APT_IGNORE_DEPRECATED_POP
	#define APT_IGNORE_DEPRECATED(XXX) XXX
#endif

#if __cplusplus >= 201103L
	#define APT_OVERRIDE override
#else
	#define APT_OVERRIDE /* no c++11 standard */
#endif

// These lines are extracted by the makefiles and the buildsystem
// Increasing MAJOR or MINOR results in the need of recompiling all
// reverse-dependencies of libapt-pkg against the new SONAME.
// Non-ABI-Breaks should only increase RELEASE number.
// See also buildlib/libversion.mak
#define APT_PKG_MAJOR 6
#define APT_PKG_MINOR 0
#define APT_PKG_RELEASE 0
#define APT_PKG_ABI ((APT_PKG_MAJOR * 100) + APT_PKG_MINOR)

/* Should be a multiple of the common page size (4096) */
static constexpr unsigned long long APT_BUFFER_SIZE = 64 * 1024;

#endif
