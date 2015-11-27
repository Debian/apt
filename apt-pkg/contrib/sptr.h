// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sptr.h,v 1.3 2001/03/11 07:22:19 jgg Exp $
/* ######################################################################
   
   Trivial non-ref counted 'smart pointer'
   
   This is really only good to eliminate 
     { 
       delete Foo;
       return;
     }
   
   Blocks from functions.
   
   I think G++ has become good enough that doing this won't have much
   code size implications.
   
   ##################################################################### */
									/*}}}*/
#ifndef SMART_POINTER_H
#define SMART_POINTER_H
#include <apt-pkg/macros.h>

template <class T>
class APT_DEPRECATED_MSG("use std::unique_ptr instead") SPtr
{
   public:
   T *Ptr;
   
   inline T *operator ->() {return Ptr;};
   inline T &operator *() {return *Ptr;};
   inline operator T *() {return Ptr;};
   inline operator void *() {return Ptr;};
   inline T *UnGuard() {T *Tmp = Ptr; Ptr = 0; return Tmp;};
   inline void operator =(T *N) {Ptr = N;};      
   inline bool operator ==(T *lhs) const {return Ptr == lhs;};
   inline bool operator !=(T *lhs) const {return Ptr != lhs;};
   inline T*Get() {return Ptr;};
      
   inline SPtr(T *Ptr) : Ptr(Ptr) {};
   inline SPtr() : Ptr(0) {};
   inline ~SPtr() {delete Ptr;};
};

template <class T>
class APT_DEPRECATED_MSG("use std::unique_ptr instead") SPtrArray
{
   public:
   T *Ptr;
   
   //inline T &operator *() {return *Ptr;};
   inline operator T *() {return Ptr;};
   inline operator void *() {return Ptr;};
   inline T *UnGuard() {T *Tmp = Ptr; Ptr = 0; return Tmp;};
   //inline T &operator [](signed long I) {return Ptr[I];};
   inline void operator =(T *N) {Ptr = N;};
   inline bool operator ==(T *lhs) const {return Ptr == lhs;};
   inline bool operator !=(T *lhs) const {return Ptr != lhs;};
   inline T *Get() {return Ptr;};
   
   inline SPtrArray(T *Ptr) : Ptr(Ptr) {};
   inline SPtrArray() : Ptr(0) {};
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunsafe-loop-optimizations"
	// gcc warns about this, but we can do nothing here…
#endif
   inline ~SPtrArray() {delete [] Ptr;};
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
};

#endif
