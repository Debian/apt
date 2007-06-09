// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: error.h,v 1.8 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   Global Erorr Class - Global error mechanism

   This class has a single global instance. When a function needs to 
   generate an error condition, such as a read error, it calls a member
   in this class to add the error to a stack of errors. 
   
   By using a stack the problem with a scheme like errno is removed and
   it allows a very detailed account of what went wrong to be transmitted
   to the UI for display. (Errno has problems because each function sets
   errno to 0 if it didn't have an error thus eraseing erno in the process
   of cleanup)
   
   Several predefined error generators are provided to handle common 
   things like errno. The general idea is that all methods return a bool.
   If the bool is true then things are OK, if it is false then things 
   should start being undone and the stack should unwind under program
   control.
   
   A Warning should not force the return of false. Things did not fail, but
   they might have had unexpected problems. Errors are stored in a FIFO
   so Pop will return the first item..
   
   I have some thoughts about extending this into a more general UI<-> 
   Engine interface, ie allowing the Engine to say 'The disk is full' in 
   a dialog that says 'Panic' and 'Retry'.. The error generator functions
   like errno, Warning and Error return false always so this is normal:
     if (open(..))
        return _error->Errno(..);
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ERROR_H
#define PKGLIB_ERROR_H



#ifdef __GNUG__
// Methods have a hidden this parameter that is visible to this attribute
#define APT_MFORMAT1 __attribute__ ((format (printf, 2, 3)))
#define APT_MFORMAT2 __attribute__ ((format (printf, 3, 4)))
#else
#define APT_MFORMAT1
#define APT_MFORMAT2    
#endif    
    
#include <string>

using std::string;

class GlobalError
{
   struct Item
   {
      string Text;
      bool Error;
      Item *Next;
   };
   
   Item *List;
   bool PendingFlag;
   void Insert(Item *I);
   
   public:

   // Call to generate an error from a library call.
   bool Errno(const char *Function,const char *Description,...) APT_MFORMAT2;
   bool WarningE(const char *Function,const char *Description,...) APT_MFORMAT2;

   /* A warning should be considered less severe than an error, and may be
      ignored by the client. */
   bool Error(const char *Description,...) APT_MFORMAT1;
   bool Warning(const char *Description,...) APT_MFORMAT1;

   // Simple accessors
   inline bool PendingError() {return PendingFlag;};
   inline bool empty() {return List == 0;};
   bool PopMessage(string &Text);
   void Discard();

   // Usefull routine to dump to cerr
   void DumpErrors();
   
   GlobalError();
};

// The 'extra-ansi' syntax is used to help with collisions. 
GlobalError *_GetErrorObj();
#define _error _GetErrorObj()

#undef APT_MFORMAT1
#undef APT_MFORMAT2

#endif
