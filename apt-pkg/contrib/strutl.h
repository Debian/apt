// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+
// Description								/*{{{*/
/* ######################################################################

   String Util - These are some useful string functions
   
   _strstrip is a function to remove whitespace from the front and end
   of a string.
   
   This file had this historic note, but now includes further changes
   under the GPL-2.0+:

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@gpu.srv.ualberta.ca>   
   
   ##################################################################### */
									/*}}}*/
#ifndef STRUTL_H
#define STRUTL_H

#include <apt-pkg/string_view.h>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "macros.h"


namespace APT {
   namespace String {
      APT_PUBLIC std::string Strip(const std::string &s);
      APT_PUBLIC bool Endswith(const std::string &s, const std::string &ending);
      APT_PUBLIC bool Startswith(const std::string &s, const std::string &starting);
      APT_PUBLIC std::string Join(std::vector<std::string> list, const std::string &sep);
      // Returns string display length honoring multi-byte characters
      APT_PUBLIC size_t DisplayLength(StringView str);
   }
}


APT_PUBLIC bool UTF8ToCodeset(const char *codeset, const std::string &orig, std::string *dest);
APT_PUBLIC char *_strstrip(char *String);
APT_PUBLIC char *_strrstrip(char *String); // right strip only
APT_DEPRECATED_MSG("Use SubstVar to avoid memory headaches") APT_PUBLIC char *_strtabexpand(char *String,size_t Len);
APT_PUBLIC bool ParseQuoteWord(const char *&String,std::string &Res);
APT_PUBLIC bool ParseCWord(const char *&String,std::string &Res);
APT_PUBLIC std::string QuoteString(const std::string &Str,const char *Bad);
APT_PUBLIC std::string DeQuoteString(const std::string &Str);
APT_PUBLIC std::string DeQuoteString(std::string::const_iterator const &begin, std::string::const_iterator const &end);

// unescape (\0XX and \xXX) from a string
APT_PUBLIC std::string DeEscapeString(const std::string &input);

APT_PUBLIC std::string SizeToStr(double Bytes);
APT_PUBLIC std::string TimeToStr(unsigned long Sec);
APT_PUBLIC std::string Base64Encode(const std::string &Str);
APT_PUBLIC std::string OutputInDepth(const unsigned long Depth, const char* Separator="  ");
APT_PUBLIC std::string URItoFileName(const std::string &URI);
/** returns a datetime string as needed by HTTP/1.1 and Debian files.
 *
 * Note: The date will always be represented in a UTC timezone
 *
 * @param Date to be represented as a string
 * @param NumericTimezone is preferred in general, but HTTP/1.1 requires the use
 *    of GMT as timezone instead. \b true means that the timezone should be denoted
 *    as "+0000" while \b false uses "GMT".
 */
APT_PUBLIC std::string TimeRFC1123(time_t Date, bool const NumericTimezone);
/** parses time as needed by HTTP/1.1 and Debian files.
 *
 * HTTP/1.1 prefers dates in RFC1123 format (but the other two obsolete date formats
 * are supported to) and e.g. Release files use the same format in Date & Valid-Until
 * fields.
 *
 * Note: datetime strings need to be in UTC timezones (GMT, UTC, Z, +/-0000) to be
 * parsed. Other timezones will be rejected as invalid. Previous implementations
 * accepted other timezones, but treated them as UTC.
 *
 * @param str is the datetime string to parse
 * @param[out] time will be the seconds since epoch of the given datetime if
 *    parsing is successful, undefined otherwise.
 * @return \b true if parsing was successful, otherwise \b false.
 */
APT_PUBLIC bool RFC1123StrToTime(const std::string &str,time_t &time) APT_MUSTCHECK;
APT_PUBLIC bool FTPMDTMStrToTime(const char* const str,time_t &time) APT_MUSTCHECK;
APT_PUBLIC std::string LookupTag(const std::string &Message,const char *Tag,const char *Default = 0);
APT_PUBLIC int StringToBool(const std::string &Text,int Default = -1);
APT_PUBLIC bool ReadMessages(int Fd, std::vector<std::string> &List);
APT_PUBLIC bool StrToNum(const char *Str,unsigned long &Res,unsigned Len,unsigned Base = 0);
APT_PUBLIC bool StrToNum(const char *Str,unsigned long long &Res,unsigned Len,unsigned Base = 0);
APT_PUBLIC bool Base256ToNum(const char *Str,unsigned long &Res,unsigned int Len);
APT_PUBLIC bool Base256ToNum(const char *Str,unsigned long long &Res,unsigned int Len);
APT_PUBLIC bool Hex2Num(const APT::StringView Str,unsigned char *Num,unsigned int Length);
// input changing string split
APT_PUBLIC bool TokSplitString(char Tok,char *Input,char **List,
		    unsigned long ListMax);

// split a given string by a char
APT_PUBLIC std::vector<std::string> VectorizeString(std::string const &haystack, char const &split) APT_PURE;

/* \brief Return a vector of strings from string "input" where "sep"
 * is used as the delimiter string.
 *
 * \param input The input string.
 *
 * \param sep The separator to use.
 *
 * \param maxsplit (optional) The maximum amount of splitting that
 * should be done .
 * 
 * The optional "maxsplit" argument can be used to limit the splitting,
 * if used the string is only split on maxsplit places and the last
 * item in the vector contains the remainder string.
 */
APT_PUBLIC std::vector<std::string> StringSplit(std::string const &input,
                                     std::string const &sep, 
                                     unsigned int maxsplit=std::numeric_limits<unsigned int>::max()) APT_PURE;


APT_HIDDEN bool iovprintf(std::ostream &out, const char *format, va_list &args, ssize_t &size);
APT_PUBLIC void ioprintf(std::ostream &out,const char *format,...) APT_PRINTF(2);
APT_PUBLIC void strprintf(std::string &out,const char *format,...) APT_PRINTF(2);
APT_PUBLIC char *safe_snprintf(char *Buffer,char *End,const char *Format,...) APT_PRINTF(3);
APT_PUBLIC bool CheckDomainList(const std::string &Host, const std::string &List);

/* Do some compat mumbo jumbo */
#define tolower_ascii  tolower_ascii_inline
#define isspace_ascii  isspace_ascii_inline

APT_PURE APT_HOT
static inline int tolower_ascii_unsafe(int const c)
{
   return c | 0x20;
}
APT_PURE APT_HOT
static inline int tolower_ascii_inline(int const c)
{
   return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}
APT_PURE APT_HOT
static inline int isspace_ascii_inline(int const c)
{
   // 9='\t',10='\n',11='\v',12='\f',13='\r',32=' '
   return (c >= 9 && c <= 13) || c == ' ';
}
APT_PURE APT_HOT
static inline int islower_ascii(int const c)
{
   return c >= 'a' && c <= 'z';
}
APT_PURE APT_HOT
static inline int isupper_ascii(int const c)
{
   return c >= 'A' && c <= 'Z';
}
APT_PURE APT_HOT
static inline int isalpha_ascii(int const c)
{
   return isupper_ascii(c) || islower_ascii(c);
}

APT_PUBLIC std::string StripEpoch(const std::string &VerStr);

#define APT_MKSTRCMP(name,func) \
inline APT_PURE int name(const char *A,const char *B) {return func(A,A+strlen(A),B,B+strlen(B));} \
inline APT_PURE int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));} \
inline APT_PURE int name(const std::string& A,const char *B) {return func(A.c_str(),A.c_str()+A.length(),B,B+strlen(B));} \
inline APT_PURE int name(const std::string& A,const std::string& B) {return func(A.c_str(),A.c_str()+A.length(),B.c_str(),B.c_str()+B.length());} \
inline APT_PURE int name(const std::string& A,const char *B,const char *BEnd) {return func(A.c_str(),A.c_str()+A.length(),B,BEnd);}

#define APT_MKSTRCMP2(name,func) \
inline APT_PURE int name(const char *A,const char *AEnd,const char *B) {return func(A,AEnd,B,B+strlen(B));} \
inline APT_PURE int name(const std::string& A,const char *B) {return func(A.begin(),A.end(),B,B+strlen(B));} \
inline APT_PURE int name(const std::string& A,const std::string& B) {return func(A.begin(),A.end(),B.begin(),B.end());} \
inline APT_PURE int name(const std::string& A,const char *B,const char *BEnd) {return func(A.begin(),A.end(),B,BEnd);}

APT_PUBLIC int APT_PURE stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd);
APT_PUBLIC int APT_PURE stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd);

/* We assume that GCC 3 indicates that libstdc++3 is in use too. In that
   case the definition of string::const_iterator is not the same as
   const char * and we need these extra functions */
#if __GNUC__ >= 3
APT_PUBLIC int APT_PURE stringcmp(std::string::const_iterator A,std::string::const_iterator AEnd,
	      const char *B,const char *BEnd);
APT_PUBLIC int APT_PURE stringcmp(std::string::const_iterator A,std::string::const_iterator AEnd,
	      std::string::const_iterator B,std::string::const_iterator BEnd);
APT_PUBLIC int APT_PURE stringcasecmp(std::string::const_iterator A,std::string::const_iterator AEnd,
		  const char *B,const char *BEnd);
APT_PUBLIC int APT_PURE stringcasecmp(std::string::const_iterator A,std::string::const_iterator AEnd,
                  std::string::const_iterator B,std::string::const_iterator BEnd);

inline APT_PURE int stringcmp(std::string::const_iterator A,std::string::const_iterator Aend,const char *B) {return stringcmp(A,Aend,B,B+strlen(B));}
inline APT_PURE int stringcasecmp(std::string::const_iterator A,std::string::const_iterator Aend,const char *B) {return stringcasecmp(A,Aend,B,B+strlen(B));}
#endif

APT_MKSTRCMP2(stringcmp,stringcmp)
APT_MKSTRCMP2(stringcasecmp,stringcasecmp)

// Return the length of a NULL-terminated string array
APT_PUBLIC size_t APT_PURE strv_length(const char **str_array);


inline const char *DeNull(const char *s) {return (s == 0?"(null)":s);}

class APT_PUBLIC URI
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
   inline void operator =(const std::string &From) {CopyFrom(From);}
   inline bool empty() {return Access.empty();};
   static std::string SiteOnly(const std::string &URI);
   static std::string ArchiveOnly(const std::string &URI);
   static std::string NoUserPassword(const std::string &URI);

   explicit URI(std::string Path) { CopyFrom(Path); }
   URI() : Port(0) {}
};

struct SubstVar
{
   const char *Subst;
   const std::string *Contents;
};
APT_PUBLIC std::string SubstVar(std::string Str,const struct SubstVar *Vars);
APT_PUBLIC std::string SubstVar(const std::string &Str,const std::string &Subst,const std::string &Contents);

struct RxChoiceList
{
   void *UserData;
   const char *Str;
   bool Hit;
};
APT_PUBLIC unsigned long RegexChoice(RxChoiceList *Rxs,const char **ListBegin,
		      const char **ListEnd);

#endif
