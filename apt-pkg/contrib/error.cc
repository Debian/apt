// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: error.cc,v 1.1 1998/07/02 02:58:13 jgg Exp $
/* ######################################################################
   
   Global Erorr Class - Global error mechanism

   We use a simple STL vector to store each error record. A PendingFlag
   is kept which indicates when the vector contains a Sever error.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <pkglib/error.h>
   									/*}}}*/

GlobalError *_error = new GlobalError;

// GlobalError::GlobalError - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
GlobalError::GlobalError() : PendingFlag(false)
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
   vsprintf(S,Description,args);
   sprintf(S + strlen(S)," - %s (%i %s)",Function,errno,strerror(errno));

   // Put it on the list
   Item Itm;
   Itm.Text = S;
   Itm.Error = true;
   List.push_back(Itm);
   
   PendingFlag = true;

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
   vsprintf(S,Description,args);

   // Put it on the list
   Item Itm;
   Itm.Text = S;
   Itm.Error = true;
   List.push_back(Itm);
   
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
   vsprintf(S,Description,args);

   // Put it on the list
   Item Itm;
   Itm.Text = S;
   Itm.Error = false;
   List.push_back(Itm);
   
   return false;
}
									/*}}}*/
// GlobalError::PopMessage - Pulls a single message out			/*{{{*/
// ---------------------------------------------------------------------
/* This should be used in a loop checking empty() each cycle. It returns
   true if the message is an error. */
bool GlobalError::PopMessage(string &Text)
{
   bool Ret = List.front().Error;
   Text = List.front().Text;
   List.erase(List.begin());

   // This really should check the list to see if only warnings are left..
   if (empty())
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
