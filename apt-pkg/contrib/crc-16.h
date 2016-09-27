// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: crc-16.h,v 1.1 1999/05/23 22:55:54 jgg Exp $
/* ######################################################################

   CRC16 - Compute a 16bit crc very quickly
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_CRC16_H
#define APTPKG_CRC16_H

#include <apt-pkg/macros.h>

#define INIT_FCS  0xffff
unsigned short AddCRC16Byte(unsigned short fcs, unsigned char byte) APT_CONST;
unsigned short AddCRC16(unsigned short fcs, void const *buf,
			unsigned long long len) APT_PURE;

#endif
