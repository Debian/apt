// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.h,v 1.22 2003/02/02 22:20:27 jgg Exp $
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



#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <time.h>

using std::string;
using std::vector;
using std::ostream;

#ifdef __GNUG__
// Methods have a hidden this parameter that is visible to this attribute
#define APT_FORMAT2 __attribute__ ((format (printf, 2, 3)))
#define APT_FORMAT3 __attribute__ ((format (printf, 3, 4)))
#else
#define APT_FORMAT2
#define APT_FORMAT3
#endif    

bool UTF8ToCodeset(const char *codeset, const string &orig, string *dest);
char *_strstrip(char *String);
char *_strtabexpand(char *String,size_t Len);
bool ParseQuoteWord(const char *&String,string &Res);
bool ParseCWord(const char *&String,string &Res);
string QuoteString(const string &Str,const char *Bad);
string DeQuoteString(const string &Str);
string SizeToStr(double Bytes);
string TimeToStr(unsigned long Sec);
string Base64Encode(const string &Str);
string URItoFileName(const string &URI);
string TimeRFC1123(time_t Date);
bool StrToTime(const string &Val,time_t &Result);
string LookupTag(const string &Message,const char *Tag,const char *Default = 0);
int StringToBool(const string &Text,int Default = -1);
bool ReadMessages(int Fd, vector<string> &List);
bool StrToNum(const char *Str,unsigned long &Res,unsigned Len,unsigned Base = 0);
bool Hex2Num(const string &Str,unsigned char *Num,unsigned int Length);
bool TokSplitString(char Tok,char *Input,char **List,
		    unsigned long ListMax);
void ioprintf(ostream &out,const char *format,...) APT_FORMAT2;
char *safe_snprintf(char *Buffer,char *End,const char *Format,...) APT_FORMAT3;
bool CheckDomainList(const string &Host, const string &List);

#define APT_MKSTRCMP(name,func) \
inline int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));}; \
inline int name(string A,const char *B) {return func(A.c_str(),A.c_str()+A.length(),B,B+strlen(B));}; \
inline int name(string A,string B) {return func(A.c_str(),A.c_str()+A.length(),B.c_str(),B.c_str()+B.length());}; \
inline int name(string A,const char *B,const char *BEnd) {return func(A.c_str(),A.c_str()+A.length(),B,BEnd);}; 

#define APT_MKSTRCMP2(name,func) \
inline int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));}; \
inline int name(string A,const char *B) {return func(A.begin(),A.end(),B,B+strlen(B));}; \
inline int name(string A,string B) {return func(A.begin(),A.end(),B.begin(),B.end());}; \
inline int name(string A,const char *B,const char *BEnd) {return func(A.begin(),A.end(),B,BEnd);}; 

int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);

/* We assume that GCC 3 indicates that libstdc++3 is in use too. In that
   case the definition of string::const_iterator is not the same as
   const char * and we need these extra functions */
#if __GNUC__ >= 3
int stringcmp(string::const_iterator A,string::const_iterator AEnd,
	      const char *B,const char *BEnd);
int stringcmp(string::const_iterator A,string::const_iterator AEnd,
	      string::const_iterator B,string::const_iterator BEnd);
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
		  const char *B,const char *BEnd);
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
                  string::const_iterator B,string::const_iterator BEnd);

inline int stringcmp(string::const_iterator A,string::const_iterator Aend,const char *B) {return stringcmp(A,Aend,B,B+strlen(B));};
inline int stringcasecmp(string::const_iterator A,string::const_iterator Aend,const char *B) {return stringcasecmp(A,Aend,B,B+strlen(B));};
#endif

APT_MKSTRCMP2(stringcmp,stringcmp);
APT_MKSTRCMP2(stringcasecmp,stringcasecmp);

inline const char *DeNull(const char *s) {return (s == 0?"(null)":s);};

class URI
{
   void CopyFrom(const string &From);
		 
   public:
   
   string Access;
   string User;
   string Password;
   string Host;
   string Path;
   unsigned int Port;
   
   operator string();
   inline void operator =(const string &From) {CopyFrom(From);};
   inline bool empty() {return Access.empty();};
   static string SiteOnly(const string &URI);
   
   URI(string Path) {CopyFrom(Path);};
   URI() : Port(0) {};
};

struct SubstVar
{
   const char *Subst;
   const string *Contents;
};
string SubstVar(string Str,const struct SubstVar *Vars);
string SubstVar(const string &Str,const string &Subst,const string &Contents);

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
