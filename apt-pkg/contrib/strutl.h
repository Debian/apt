// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.h,v 1.18 2001/05/27 05:19:30 jgg Exp $
/* ######################################################################

   String Util - These are some useful string functions
   
   _strstrip is a function to remove whitespace from the front and end
   of a string.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@gpu.srv.ualberta.ca>   
   
   ##################################################################### */
									/*}}}*/
#ifndef STRUTL_H
#define STRUTL_H

#ifdef __GNUG__
#pragma interface "apt-pkg/strutl.h"
#endif 

#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include <time.h>

using std::string;
using std::vector;
using std::ostream;

#ifdef __GNUG__
// Methods have a hidden this parameter that is visible to this attribute
#define APT_FORMAT2 __attribute__ ((format (printf, 2, 3)))
#else
#define APT_FORMAT2
#endif    
    
char *_strstrip(char *String);
char *_strtabexpand(char *String,size_t Len);
bool ParseQuoteWord(const char *&String,string &Res);
bool ParseCWord(const char *&String,string &Res);
string QuoteString(string Str,const char *Bad);
string DeQuoteString(string Str);
string SizeToStr(double Bytes);
string TimeToStr(unsigned long Sec);
string Base64Encode(string Str);
string URItoFileName(string URI);
string TimeRFC1123(time_t Date);
bool StrToTime(string Val,time_t &Result);
string LookupTag(string Message,const char *Tag,const char *Default = 0);
int StringToBool(string Text,int Default = -1);
bool ReadMessages(int Fd, vector<string> &List);
bool StrToNum(const char *Str,unsigned long &Res,unsigned Len,unsigned Base = 0);
bool Hex2Num(string Str,unsigned char *Num,unsigned int Length);
bool TokSplitString(char Tok,char *Input,char **List,
		    unsigned long ListMax);
void ioprintf(ostream &out,const char *format,...) APT_FORMAT2;
bool CheckDomainList(string Host,string List);

int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
inline int stringcmp(const char *A,const char *AEnd,const char *B) {return stringcmp(A,AEnd,B,B+strlen(B));};
inline int stringcmp(string A,const char *B) {return stringcmp(A.c_str(),A.c_str()+A.length(),B,B+strlen(B));};

int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
inline int stringcasecmp(const char *A,const char *AEnd,const char *B) {return stringcasecmp(A,AEnd,B,B+strlen(B));};
inline int stringcasecmp(string A,const char *B) {return stringcasecmp(A.c_str(),A.c_str()+A.length(),B,B+strlen(B));};
inline int stringcasecmp(string A,string B) {return stringcasecmp(A.c_str(),A.c_str()+A.length(),B.c_str(),B.c_str()+B.length());};
inline int stringcasecmp(string A,const char *B,const char *BEnd) {return stringcasecmp(A.c_str(),A.c_str()+A.length(),B,BEnd);};

inline const char *DeNull(const char *s) {return (s == 0?"(null)":s);};

class URI
{
   void CopyFrom(string From);
		 
   public:
   
   string Access;
   string User;
   string Password;
   string Host;
   string Path;
   unsigned int Port;
   
   operator string();
   inline void operator =(string From) {CopyFrom(From);};
   inline bool empty() {return Access.empty();};
   static string SiteOnly(string URI);
   
   URI(string Path) {CopyFrom(Path);};
   URI() : Port(0) {};
};

struct SubstVar
{
   const char *Subst;
   const string *Contents;
};
string SubstVar(string Str,const struct SubstVar *Vars);
string SubstVar(string Str,string Subst,string Contents);

struct RxChoiceList
{
   void *UserData;
   const char *Str;
   bool Hit;
};
unsigned long RegexChoice(RxChoiceList *Rxs,const char **ListBegin,
		      const char **ListEnd);

#undef APT_FORMAT2

#endif
