// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.h,v 1.2 1999/07/18 23:06:56 jgg Exp $
/* ######################################################################

   Connect - Replacement connect call
   
   ##################################################################### */
									/*}}}*/
#ifndef CONNECT_H
#define CONNECT_H

#include <string>
#include <apt-pkg/acquire-method.h>

bool Connect(string To,int Port,const char *Service,int DefPort,
	     int &Fd,unsigned long TimeOut,pkgAcqMethod *Owner);

#endif
