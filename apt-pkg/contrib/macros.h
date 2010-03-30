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

/* Usefull count macro, use on an array of things and it will return the
   number of items in the array */
#define _count(a) (sizeof(a)/sizeof(a[0]))

// Flag Macros
#define	FLAG(f)			(1L << (f))
#define	SETFLAG(v,f)	((v) |= FLAG(f))
#define CLRFLAG(v,f)	((v) &=~FLAG(f))
#define	CHKFLAG(v,f)	((v) &  FLAG(f) ? true : false)

// some nice optional GNUC features
#if __GNUC__ >= 3
	#define __must_check	__attribute__ ((warn_unused_result))
	#define __deprecated	__attribute__ ((deprecated))
	#define __attrib_const	__attribute__ ((__const__))
	/* likely() and unlikely() can be used to mark boolean expressions
	   as (not) likely true which will help the compiler to optimise */
	#define likely(x)	__builtin_expect (!!(x), 1)
	#define unlikely(x)	__builtin_expect (!!(x), 0)
#else
	#define __must_check	/* no warn_unused_result */
	#define __deprecated	/* no deprecated */
	#define __attrib_const	/* no const attribute */
	#define likely(x)	(x)
	#define unlikely(x)	(x)
#endif

// cold functions are unlikely() to be called
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4
	#define __cold	__attribute__ ((__cold__))
	#define __hot	__attribute__ ((__hot__))
#else
	#define __cold	/* no cold marker */
	#define __hot	/* no hot marker */
#endif

#ifdef __GNUG__
// Methods have a hidden this parameter that is visible to this attribute
	#define __like_printf(n)	__attribute__((format(printf, n, n + 1)))
#else
	#define __like_printf(n)	/* no like-printf */
#endif

#endif
