// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Macros Header - Various useful macro definitions

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Brian C. White.
   
   ##################################################################### */
									/*}}}*/
// Private header
#ifndef MACROS_H
#define MACROS_H

// MIN_VAL(SINT16) will return -0x8000 and MAX_VAL(SINT16) = 0x7FFF
#define	MIN_VAL(t)	(((t)(-1) > 0) ? (t)( 0) : (t)(((1L<<(sizeof(t)*8-1))  )))
#define	MAX_VAL(t)	(((t)(-1) > 0) ? (t)(-1) : (t)(((1L<<(sizeof(t)*8-1))-1)))

// Min/Max functions
#if !defined(MIN)
#if defined(__HIGHC__)
#define MIN(x,y) _min(x,y)
#define MAX(x,y) _max(x,y)
#endif

// GNU C++ has a min/max operator <coolio>
#if defined(__GNUG__)
#define MIN(A,B) ((A) <? (B))
#define MAX(A,B) ((A) >? (B))
#endif

/* Templates tend to mess up existing code that uses min/max because of the
   strict matching requirements */
#if !defined(MIN)
#define MIN(A,B) ((A) < (B)?(A):(B))
#define MAX(A,B) ((A) > (B)?(A):(B))
#endif
#endif

/* Bound functions, bound will return the value b within the limits a-c
   bounv will change b so that it is within the limits of a-c. */
#define _bound(a,b,c) MIN(c,MAX(b,a))
#define _boundv(a,b,c) b = _bound(a,b,c)
#define ABS(a) (((a) < (0)) ?-(a) : (a))

/* Useful count macro, use on an array of things and it will return the
   number of items in the array */
#define _count(a) (sizeof(a)/sizeof(a[0]))

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

/* likely() and unlikely() can be used to mark boolean expressions
   as (not) likely true which will help the compiler to optimise */
#if APT_GCC_VERSION >= 0x0300
	#define likely(x)	__builtin_expect (!!(x), 1)
	#define unlikely(x)	__builtin_expect (!!(x), 0)
#else
	#define likely(x)	(x)
	#define unlikely(x)	(x)
#endif

#if APT_GCC_VERSION >= 0x0300
	#define APT_DEPRECATED	__attribute__ ((deprecated))
	#define APT_DEPRECATED_MSG(X)	__attribute__ ((deprecated(X)))
	// __attribute__((const)) is too dangerous for us, we end up using it wrongly
	#define APT_CONST	__attribute__((pure))
	#define APT_PURE	__attribute__((pure))
	#define APT_NORETURN	__attribute__((noreturn))
	#define APT_PRINTF(n)	__attribute__((format(printf, n, n + 1)))
	#define APT_WEAK        __attribute__((weak));
#else
	#define APT_DEPRECATED
	#define APT_DEPRECATED_MSG
	#define APT_CONST
	#define APT_PURE
	#define APT_NORETURN
	#define APT_PRINTF(n)
	#define APT_WEAK
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
#define APT_PKG_MAJOR 5
#define APT_PKG_MINOR 90
#define APT_PKG_RELEASE 0
#define APT_PKG_ABI ((APT_PKG_MAJOR * 100) + APT_PKG_MINOR)


#if APT_PKG_ABI > 590
#define APT_PKG_590(msg)  __attribute__((error(msg)))
#else
#define APT_PKG_590(msg)
#endif

#endif
