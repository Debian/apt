// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: system.h,v 1.3 1999/12/10 23:40:29 jgg Exp $
/* ######################################################################
   
   System Header - Usefull private definitions

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Brian C. White.
   
   ##################################################################### */
									/*}}}*/
// Private header
#ifndef SYSTEM_H
#define SYSTEM_H

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

#endif
