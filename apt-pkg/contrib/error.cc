// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: error.cc,v 1.11 2002/03/26 07:38:58 jgg Exp $
/* ######################################################################
   
   Global Erorr Class - Global error mechanism

   We use a simple STL vector to store each error record. A PendingFlag
   is kept which indicates when the vector contains a Sever error.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/error.h>

#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include <string>
#include <cstring>

#include "config.h"
   									/*}}}*/

using namespace std;

// Global Error Object							/*{{{*/
/* If the implementation supports posix threads then the accessor function
   is compiled to be thread safe otherwise a non-safe version is used. A
   Per-Thread error object is maintained in much the same manner as libc
   manages errno */
#if defined(_POSIX_THREADS) && defined(HAVE_PTHREAD)
 #include <pthread.h>

 static pthread_key_t ErrorKey;
 static void ErrorDestroy(void *Obj) {delete (GlobalError *)Obj;};
 static void KeyAlloc() {pthread_key_create(&ErrorKey,ErrorDestroy);};

 GlobalError *_GetErrorObj()
 {
    static pthread_once_t Once = PTHREAD_ONCE_INIT;
    pthread_once(&Once,KeyAlloc);
    
    void *Res = pthread_getspecific(ErrorKey);
    if (Res == 0)
       pthread_setspecific(ErrorKey,Res = new GlobalError);
    return (GlobalError *)Res;
 }
#else
 GlobalError *_GetErrorObj()
 {
    static GlobalError *Obj = new GlobalError;
    return Obj;
 }
#endif
									/*}}}*/

// GlobalError::GlobalError - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
GlobalError::GlobalError() : List(0), PendingFlag(false)
{
}
									/*}}}*/
// GlobalError::Errno - Get part of the error string from errno		/*{{{*/
// ---------------------------------------------------------------------
/* Function indicates the stdlib function that failed and Description is
   a user string that leads the text. Form is:
     Description - Function (errno: strerror)
   Carefull of the buffer overrun, sprintf.
 */
bool GlobalError::Errno(const char *Function,const char *Description,...)
{
   va_list args;
   va_start(args,Description);

   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S),Description,args);
   snprintf(S + strlen(S),sizeof(S) - strlen(S),
	    " - %s (%i %s)",Function,errno,strerror(errno));

   // Put it on the list
   Item *Itm = new Item;
   Itm->Text = S;
   Itm->Error = true;
   Insert(Itm);
   
   PendingFlag = true;

   return false;   
}
									/*}}}*/
// GlobalError::WarningE - Get part of the warn string from errno	/*{{{*/
// ---------------------------------------------------------------------
/* Function indicates the stdlib function that failed and Description is
   a user string that leads the text. Form is:
     Description - Function (errno: strerror)
   Carefull of the buffer overrun, sprintf.
 */
bool GlobalError::WarningE(const char *Function,const char *Description,...)
{
   va_list args;
   va_start(args,Description);

   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S),Description,args);
   snprintf(S + strlen(S),sizeof(S) - strlen(S)," - %s (%i %s)",Function,errno,strerror(errno));

   // Put it on the list
   Item *Itm = new Item;
   Itm->Text = S;
   Itm->Error = false;
   Insert(Itm);
   
   return false;   
}
									/*}}}*/
// GlobalError::Error - Add an error to the list			/*{{{*/
// ---------------------------------------------------------------------
/* Just vsprintfs and pushes */
bool GlobalError::Error(const char *Description,...)
{
   va_list args;
   va_start(args,Description);

   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S),Description,args);

   // Put it on the list
   Item *Itm = new Item;
   Itm->Text = S;
   Itm->Error = true;
   Insert(Itm);
   
   PendingFlag = true;
   
   return false;
}
									/*}}}*/
// GlobalError::Warning - Add a warning to the list			/*{{{*/
// ---------------------------------------------------------------------
/* This doesn't set the pending error flag */
bool GlobalError::Warning(const char *Description,...)
{
   va_list args;
   va_start(args,Description);

   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S),Description,args);

   // Put it on the list
   Item *Itm = new Item;
   Itm->Text = S;
   Itm->Error = false;
   Insert(Itm);
   
   return false;
}
									/*}}}*/
// GlobalError::PopMessage - Pulls a single message out			/*{{{*/
// ---------------------------------------------------------------------
/* This should be used in a loop checking empty() each cycle. It returns
   true if the message is an error. */
bool GlobalError::PopMessage(string &Text)
{
   if (List == 0)
      return false;
      
   bool Ret = List->Error;
   Text = List->Text;
   Item *Old = List;
   List = List->Next;
   delete Old;
   
   // This really should check the list to see if only warnings are left..
   if (List == 0)
      PendingFlag = false;
   
   return Ret;
}
									/*}}}*/
// GlobalError::DumpErrors - Dump all of the errors/warns to cerr	/*{{{*/
// ---------------------------------------------------------------------
/* */
void GlobalError::DumpErrors()
{
   // Print any errors or warnings found
   string Err;
   while (empty() == false)
   {
      bool Type = PopMessage(Err);
      if (Type == true)
	 cerr << "E: " << Err << endl;
      else
	 cerr << "W: " << Err << endl;
   }
}
									/*}}}*/
// GlobalError::Discard - Discard									/*{{{*/
// ---------------------------------------------------------------------
/* */
void GlobalError::Discard()
{
   while (List != 0)
   {
      Item *Old = List;
      List = List->Next;
      delete Old;
   }
   
   PendingFlag = false;
};
									/*}}}*/
// GlobalError::Insert - Insert a new item at the end			/*{{{*/
// ---------------------------------------------------------------------
/* */
void GlobalError::Insert(Item *Itm)
{
   Item **End = &List;
   for (Item *I = List; I != 0; I = I->Next)
      End = &I->Next;
   Itm->Next = *End;
   *End = Itm;
}
									/*}}}*/
