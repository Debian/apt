// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.h,v 1.1 1999/05/29 03:25:03 jgg Exp $
/* ######################################################################

   Connect - Replacement connect call
   
   ##################################################################### */
									/*}}}*/
#ifndef CONNECT_H
#define CONNECT_H

#include <string>
#include <apt-pkg/acquire-method.h>

bool Connect(string To,int Port,const char *Service,int &Fd,
	     unsigned long TimeOut,pkgAcqMethod *Owner);

#endif
