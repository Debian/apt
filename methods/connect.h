// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: connect.h,v 1.3 2001/02/20 07:03:18 jgg Exp $
/* ######################################################################

   Connect - Replacement connect call
   
   ##################################################################### */
									/*}}}*/
#ifndef CONNECT_H
#define CONNECT_H

#include <string>

class pkgAcqMethod;

bool Connect(std::string To,int Port,const char *Service,int DefPort,
	     int &Fd,unsigned long TimeOut,pkgAcqMethod *Owner);
void RotateDNS();

#endif
