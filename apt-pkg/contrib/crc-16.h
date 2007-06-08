// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: crc-16.h,v 1.1 1999/05/23 22:55:54 jgg Exp $
/* ######################################################################

   CRC16 - Compute a 16bit crc very quickly
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_CRC16_H
#define APTPKG_CRC16_H

#define INIT_FCS  0xffff
unsigned short AddCRC16(unsigned short fcs, void const *buf,
			unsigned long len);

#endif
