// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.h,v 1.1 1998/07/07 04:17:16 jgg Exp $
/* ######################################################################

   String Util - These are some usefull string functions
   
   _strstrip is a function to remove whitespace from the front and end
   of a string.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@gpu.srv.ualberta.ca>   
   
   ##################################################################### */
									/*}}}*/
// This is a private header
// Header section: /
#ifndef STRUTL_H
#define STRUTL_H

#include <stdlib.h>
#include <string>

char *_strstrip(char *String);
char *_strtabexpand(char *String,size_t Len);
bool ParseQuoteWord(const char *&String,string &Res);
string QuoteString(string Str,const char *Bad);
string SizeToStr(double Bytes);
string TimeToStr(unsigned long Sec);
string SubstVar(string Str,string Subst,string Contents);
string Base64Encode(string Str);

int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);

#endif
