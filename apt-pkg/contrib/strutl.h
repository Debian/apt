// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.h,v 1.6 1998/10/22 04:56:49 jgg Exp $
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
#include <vector>

char *_strstrip(char *String);
char *_strtabexpand(char *String,size_t Len);
bool ParseQuoteWord(const char *&String,string &Res);
bool ParseCWord(const char *String,string &Res);
string QuoteString(string Str,const char *Bad);
string SizeToStr(double Bytes);
string TimeToStr(unsigned long Sec);
string SubstVar(string Str,string Subst,string Contents);
string Base64Encode(string Str);
string URItoFileName(string URI);
string URIAccess(string URI);
string TimeRFC1123(time_t Date);
string LookupTag(string Message,const char *Tag,const char *Default = 0);
int StringToBool(string Text,int Default = -1);
bool ReadMessages(int Fd, vector<string> &List);

int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
inline int stringcmp(const char *A,const char *AEnd,const char *B) {return stringcmp(A,AEnd,B,B+strlen(B));};
int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
inline int stringcasecmp(const char *A,const char *AEnd,const char *B) {return stringcasecmp(A,AEnd,B,B+strlen(B));};

#endif
