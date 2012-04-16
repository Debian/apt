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

#include "macros.h"

#ifndef APT_8_CLEANER_HEADERS
using std::string;
using std::vector;
using std::ostream;
#endif

bool UTF8ToCodeset(const char *codeset, const std::string &orig, std::string *dest);
char *_strstrip(char *String);
char *_strtabexpand(char *String,size_t Len);
bool ParseQuoteWord(const char *&String,std::string &Res);
bool ParseCWord(const char *&String,std::string &Res);
std::string QuoteString(const std::string &Str,const char *Bad);
std::string DeQuoteString(const std::string &Str);
std::string DeQuoteString(std::string::const_iterator const &begin, std::string::const_iterator const &end);

// unescape (\0XX and \xXX) from a string
std::string DeEscapeString(const std::string &input);

std::string SizeToStr(double Bytes);
std::string TimeToStr(unsigned long Sec);
std::string Base64Encode(const std::string &Str);
std::string OutputInDepth(const unsigned long Depth, const char* Separator="  ");
std::string URItoFileName(const std::string &URI);
std::string TimeRFC1123(time_t Date);
bool RFC1123StrToTime(const char* const str,time_t &time) __must_check;
bool FTPMDTMStrToTime(const char* const str,time_t &time) __must_check;
__deprecated bool StrToTime(const std::string &Val,time_t &Result);
std::string LookupTag(const std::string &Message,const char *Tag,const char *Default = 0);
int StringToBool(const std::string &Text,int Default = -1);
bool ReadMessages(int Fd, std::vector<std::string> &List);
bool StrToNum(const char *Str,unsigned long &Res,unsigned Len,unsigned Base = 0);
bool StrToNum(const char *Str,unsigned long long &Res,unsigned Len,unsigned Base = 0);
bool Base256ToNum(const char *Str,unsigned long &Res,unsigned int Len);
bool Hex2Num(const std::string &Str,unsigned char *Num,unsigned int Length);
bool TokSplitString(char Tok,char *Input,char **List,
		    unsigned long ListMax);
std::vector<std::string> VectorizeString(std::string const &haystack, char const &split) __attrib_const;
void ioprintf(std::ostream &out,const char *format,...) __like_printf(2);
void strprintf(std::string &out,const char *format,...) __like_printf(2);
char *safe_snprintf(char *Buffer,char *End,const char *Format,...) __like_printf(3);
bool CheckDomainList(const std::string &Host, const std::string &List);
int tolower_ascii(int const c) __attrib_const __hot;
std::string StripEpoch(const std::string &VerStr);

#define APT_MKSTRCMP(name,func) \
inline int name(const char *A,const char *B) {return func(A,A+strlen(A),B,B+strlen(B));}; \
inline int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));}; \
inline int name(const std::string& A,const char *B) {return func(A.c_str(),A.c_str()+A.length(),B,B+strlen(B));}; \
inline int name(const std::string& A,const std::string& B) {return func(A.c_str(),A.c_str()+A.length(),B.c_str(),B.c_str()+B.length());}; \
inline int name(const std::string& A,const char *B,const char *BEnd) {return func(A.c_str(),A.c_str()+A.length(),B,BEnd);};

#define APT_MKSTRCMP2(name,func) \
inline int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));}; \
inline int name(const std::string& A,const char *B) {return func(A.begin(),A.end(),B,B+strlen(B));}; \
inline int name(const std::string& A,const std::string& B) {return func(A.begin(),A.end(),B.begin(),B.end());}; \
inline int name(const std::string& A,const char *B,const char *BEnd) {return func(A.begin(),A.end(),B,BEnd);};

int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);

/* We assume that GCC 3 indicates that libstdc++3 is in use too. In that
   case the definition of string::const_iterator is not the same as
   const char * and we need these extra functions */
#if __GNUC__ >= 3
int stringcmp(std::string::const_iterator A,std::string::const_iterator AEnd,
	      const char *B,const char *BEnd);
int stringcmp(std::string::const_iterator A,std::string::const_iterator AEnd,
	      std::string::const_iterator B,std::string::const_iterator BEnd);
int stringcasecmp(std::string::const_iterator A,std::string::const_iterator AEnd,
		  const char *B,const char *BEnd);
int stringcasecmp(std::string::const_iterator A,std::string::const_iterator AEnd,
                  std::string::const_iterator B,std::string::const_iterator BEnd);

inline int stringcmp(std::string::const_iterator A,std::string::const_iterator Aend,const char *B) {return stringcmp(A,Aend,B,B+strlen(B));};
inline int stringcasecmp(std::string::const_iterator A,std::string::const_iterator Aend,const char *B) {return stringcasecmp(A,Aend,B,B+strlen(B));};
#endif

APT_MKSTRCMP2(stringcmp,stringcmp);
APT_MKSTRCMP2(stringcasecmp,stringcasecmp);

inline const char *DeNull(const char *s) {return (s == 0?"(null)":s);};

class URI
{
   void CopyFrom(const std::string &From);
		 
   public:
   
   std::string Access;
   std::string User;
   std::string Password;
   std::string Host;
   std::string Path;
   unsigned int Port;
   
   operator std::string();
   inline void operator =(const std::string &From) {CopyFrom(From);};
   inline bool empty() {return Access.empty();};
   static std::string SiteOnly(const std::string &URI);
   static std::string NoUserPassword(const std::string &URI);
   
   URI(std::string Path) {CopyFrom(Path);};
   URI() : Port(0) {};
};

struct SubstVar
{
   const char *Subst;
   const std::string *Contents;
};
std::string SubstVar(std::string Str,const struct SubstVar *Vars);
std::string SubstVar(const std::string &Str,const std::string &Subst,const std::string &Contents);

struct RxChoiceList
{
   void *UserData;
   const char *Str;
   bool Hit;
};
unsigned long RegexChoice(RxChoiceList *Rxs,const char **ListBegin,
		      const char **ListEnd);

#endif
