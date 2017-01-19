// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.cc,v 1.48 2003/07/18 14:15:11 mdz Exp $
/* ######################################################################

   String Util - Some useful string functions.

   These have been collected from here and there to do all sorts of useful
   things to strings. They are useful in file parsers, URI handlers and
   especially in APT methods.   
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@gpu.srv.ualberta.ca>
   
   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>

#include <array>
#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <sstream>
#include <vector>

#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <stdarg.h>
#include <iconv.h>

#include <apti18n.h>
									/*}}}*/
using namespace std;

// Strip - Remove white space from the front and back of a string       /*{{{*/
// ---------------------------------------------------------------------
namespace APT {
   namespace String {
std::string Strip(const std::string &str)
{
   // ensure we have at least one character
   if (str.empty() == true)
      return str;

   char const * const s = str.c_str();
   size_t start = 0;
   for (; isspace(s[start]) != 0; ++start)
      ; // find the first not-space

   // string contains only whitespaces
   if (s[start] == '\0')
      return "";

   size_t end = str.length() - 1;
   for (; isspace(s[end]) != 0; --end)
      ; // find the last not-space

   return str.substr(start, end - start + 1);
}

bool Endswith(const std::string &s, const std::string &end)
{
   if (end.size() > s.size())
      return false;
   return (s.compare(s.size() - end.size(), end.size(), end) == 0);
}

bool Startswith(const std::string &s, const std::string &start)
{
   if (start.size() > s.size())
      return false;
   return (s.compare(0, start.size(), start) == 0);
}

std::string Join(std::vector<std::string> list, const std::string &sep)
{
   std::ostringstream oss;
   for (auto it = list.begin(); it != list.end(); it++)
   {
      if (it != list.begin()) oss << sep;
      oss << *it;
   }
   return oss.str();
}

}
}
									/*}}}*/
// UTF8ToCodeset - Convert some UTF-8 string for some codeset   	/*{{{*/
// ---------------------------------------------------------------------
/* This is handy to use before display some information for enduser  */
bool UTF8ToCodeset(const char *codeset, const string &orig, string *dest)
{
  iconv_t cd;
  const char *inbuf;
  char *inptr, *outbuf;
  size_t insize, bufsize;
  dest->clear();

  cd = iconv_open(codeset, "UTF-8");
  if (cd == (iconv_t)(-1)) {
     // Something went wrong
     if (errno == EINVAL)
	_error->Error("conversion from 'UTF-8' to '%s' not available",
               codeset);
     else
	perror("iconv_open");
     
     return false;
  }

  insize = bufsize = orig.size();
  inbuf = orig.data();
  inptr = (char *)inbuf;
  outbuf = new char[bufsize];
  size_t lastError = -1;

  while (insize != 0)
  {
     char *outptr = outbuf;
     size_t outsize = bufsize;
     size_t const err = iconv(cd, &inptr, &insize, &outptr, &outsize);
     dest->append(outbuf, outptr - outbuf);
     if (err == (size_t)(-1))
     {
	switch (errno)
	{
	case EILSEQ:
	   insize--;
	   inptr++;
	   // replace a series of unknown multibytes with a single "?"
	   if (lastError != insize) {
	      lastError = insize - 1;
	      dest->append("?");
	   }
	   break;
	case EINVAL:
	   insize = 0;
	   break;
	case E2BIG:
	   if (outptr == outbuf)
	   {
	      bufsize *= 2;
	      delete[] outbuf;
	      outbuf = new char[bufsize];
	   }
	   break;
	}
     }
  }

  delete[] outbuf;
  
  iconv_close(cd);

  return true;
}
									/*}}}*/
// strstrip - Remove white space from the front and back of a string	/*{{{*/
// ---------------------------------------------------------------------
/* This is handy to use when parsing a file. It also removes \n's left 
   over from fgets and company */
char *_strstrip(char *String)
{
   for (;*String != 0 && (*String == ' ' || *String == '\t'); String++);

   if (*String == 0)
      return String;
   return _strrstrip(String);
}
									/*}}}*/
// strrstrip - Remove white space from the back of a string	/*{{{*/
// ---------------------------------------------------------------------
char *_strrstrip(char *String)
{
   char *End = String + strlen(String) - 1;
   for (;End != String - 1 && (*End == ' ' || *End == '\t' || *End == '\n' ||
			       *End == '\r'); End--);
   End++;
   *End = 0;
   return String;
}
									/*}}}*/
// strtabexpand - Converts tabs into 8 spaces				/*{{{*/
// ---------------------------------------------------------------------
/* */
char *_strtabexpand(char *String,size_t Len)
{
   for (char *I = String; I != I + Len && *I != 0; I++)
   {
      if (*I != '\t')
	 continue;
      if (I + 8 > String + Len)
      {
	 *I = 0;
	 return String;
      }

      /* Assume the start of the string is 0 and find the next 8 char
         division */
      int Len;
      if (String == I)
	 Len = 1;
      else
	 Len = 8 - ((String - I) % 8);
      Len -= 2;
      if (Len <= 0)
      {
	 *I = ' ';
	 continue;
      }
      
      memmove(I + Len,I + 1,strlen(I) + 1);
      for (char *J = I; J + Len != I; *I = ' ', I++);
   }
   return String;
}
									/*}}}*/
// ParseQuoteWord - Parse a single word out of a string			/*{{{*/
// ---------------------------------------------------------------------
/* This grabs a single word, converts any % escaped characters to their
   proper values and advances the pointer. Double quotes are understood
   and striped out as well. This is for URI/URL parsing. It also can 
   understand [] brackets.*/
bool ParseQuoteWord(const char *&String,string &Res)
{
   // Skip leading whitespace
   const char *C = String;
   for (;*C != 0 && *C == ' '; C++);
   if (*C == 0)
      return false;
   
   // Jump to the next word
   for (;*C != 0 && isspace(*C) == 0; C++)
   {
      if (*C == '"')
      {
	 C = strchr(C + 1, '"');
	 if (C == NULL)
	    return false;
      }
      if (*C == '[')
      {
	 C = strchr(C + 1, ']');
	 if (C == NULL)
	    return false;
      }
   }

   // Now de-quote characters
   char Buffer[1024];
   char Tmp[3];
   const char *Start = String;
   char *I;
   for (I = Buffer; I < Buffer + sizeof(Buffer) && Start != C; I++)
   {
      if (*Start == '%' && Start + 2 < C &&
	  isxdigit(Start[1]) && isxdigit(Start[2]))
      {
	 Tmp[0] = Start[1];
	 Tmp[1] = Start[2];
	 Tmp[2] = 0;
	 *I = (char)strtol(Tmp,0,16);
	 Start += 3;
	 continue;
      }
      if (*Start != '"')
	 *I = *Start;
      else
	 I--;
      Start++;
   }
   *I = 0;
   Res = Buffer;
   
   // Skip ending white space
   for (;*C != 0 && isspace(*C) != 0; C++);
   String = C;
   return true;
}
									/*}}}*/
// ParseCWord - Parses a string like a C "" expression			/*{{{*/
// ---------------------------------------------------------------------
/* This expects a series of space separated strings enclosed in ""'s. 
   It concatenates the ""'s into a single string. */
bool ParseCWord(const char *&String,string &Res)
{
   // Skip leading whitespace
   const char *C = String;
   for (;*C != 0 && *C == ' '; C++);
   if (*C == 0)
      return false;
   
   char Buffer[1024];
   char *Buf = Buffer;
   if (strlen(String) >= sizeof(Buffer))
       return false;
       
   for (; *C != 0; C++)
   {
      if (*C == '"')
      {
	 for (C++; *C != 0 && *C != '"'; C++)
	    *Buf++ = *C;
	 
	 if (*C == 0)
	    return false;
	 
	 continue;
      }      
      
      if (C != String && isspace(*C) != 0 && isspace(C[-1]) != 0)
	 continue;
      if (isspace(*C) == 0)
	 return false;
      *Buf++ = ' ';
   }
   *Buf = 0;
   Res = Buffer;
   String = C;
   return true;
}
									/*}}}*/
// QuoteString - Convert a string into quoted from			/*{{{*/
// ---------------------------------------------------------------------
/* */
string QuoteString(const string &Str, const char *Bad)
{
   std::stringstream Res;
   for (string::const_iterator I = Str.begin(); I != Str.end(); ++I)
   {
      if (strchr(Bad,*I) != 0 || isprint(*I) == 0 ||
	  *I == 0x25 || // percent '%' char
	  *I <= 0x20 || *I >= 0x7F) // control chars
      {
	 ioprintf(Res, "%%%02hhx", *I);
      }
      else
	 Res << *I;
   }
   return Res.str();
}
									/*}}}*/
// DeQuoteString - Convert a string from quoted from                    /*{{{*/
// ---------------------------------------------------------------------
/* This undoes QuoteString */
string DeQuoteString(const string &Str)
{
   return DeQuoteString(Str.begin(),Str.end());
}
string DeQuoteString(string::const_iterator const &begin,
			string::const_iterator const &end)
{
   string Res;
   for (string::const_iterator I = begin; I != end; ++I)
   {
      if (*I == '%' && I + 2 < end &&
	  isxdigit(I[1]) && isxdigit(I[2]))
      {
	 char Tmp[3];
	 Tmp[0] = I[1];
	 Tmp[1] = I[2];
	 Tmp[2] = 0;
	 Res += (char)strtol(Tmp,0,16);
	 I += 2;
	 continue;
      }
      else
	 Res += *I;
   }
   return Res;   
}

                                                                        /*}}}*/
// SizeToStr - Convert a long into a human readable size		/*{{{*/
// ---------------------------------------------------------------------
/* A max of 4 digits are shown before conversion to the next highest unit. 
   The max length of the string will be 5 chars unless the size is > 10
   YottaBytes (E24) */
string SizeToStr(double Size)
{
   double ASize;
   if (Size >= 0)
      ASize = Size;
   else
      ASize = -1*Size;

   /* bytes, KiloBytes, MegaBytes, GigaBytes, TeraBytes, PetaBytes, 
      ExaBytes, ZettaBytes, YottaBytes */
   char Ext[] = {'\0','k','M','G','T','P','E','Z','Y'};
   int I = 0;
   while (I <= 8)
   {
      if (ASize < 100 && I != 0)
      {
	 std::string S;
	 strprintf(S, "%'.1f %c", ASize, Ext[I]);
	 return S;
      }

      if (ASize < 10000)
      {
	 std::string S;
	 strprintf(S, "%'.0f %c", ASize, Ext[I]);
	 return S;
      }
      ASize /= 1000.0;
      I++;
   }
   return "";
}
									/*}}}*/
// TimeToStr - Convert the time into a string				/*{{{*/
// ---------------------------------------------------------------------
/* Converts a number of seconds to a hms format */
string TimeToStr(unsigned long Sec)
{
   std::string S;
   if (Sec > 60*60*24)
   {
      //TRANSLATOR: d means days, h means hours, min means minutes, s means seconds
      strprintf(S,_("%lid %lih %limin %lis"),Sec/60/60/24,(Sec/60/60) % 24,(Sec/60) % 60,Sec % 60);
   }
   else if (Sec > 60*60)
   {
      //TRANSLATOR: h means hours, min means minutes, s means seconds
      strprintf(S,_("%lih %limin %lis"),Sec/60/60,(Sec/60) % 60,Sec % 60);
   }
   else if (Sec > 60)
   {
      //TRANSLATOR: min means minutes, s means seconds
      strprintf(S,_("%limin %lis"),Sec/60,Sec % 60);
   }
   else
   {
      //TRANSLATOR: s means seconds
      strprintf(S,_("%lis"),Sec);
   }
   return S;
}
									/*}}}*/
// SubstVar - Substitute a string for another string			/*{{{*/
// ---------------------------------------------------------------------
/* This replaces all occurrences of Subst with Contents in Str. */
string SubstVar(const string &Str,const string &Subst,const string &Contents)
{
   if (Subst.empty() == true)
      return Str;

   string::size_type Pos = 0;
   string::size_type OldPos = 0;
   string Temp;

   while (OldPos < Str.length() &&
	  (Pos = Str.find(Subst,OldPos)) != string::npos)
   {
      if (OldPos != Pos)
	 Temp.append(Str, OldPos, Pos - OldPos);
      if (Contents.empty() == false)
	 Temp.append(Contents);
      OldPos = Pos + Subst.length();
   }

   if (OldPos == 0)
      return Str;

   if (OldPos >= Str.length())
      return Temp;

   Temp.append(Str, OldPos, string::npos);
   return Temp;
}
string SubstVar(string Str,const struct SubstVar *Vars)
{
   for (; Vars->Subst != 0; Vars++)
      Str = SubstVar(Str,Vars->Subst,*Vars->Contents);
   return Str;
}
									/*}}}*/
// OutputInDepth - return a string with separator multiplied with depth /*{{{*/
// ---------------------------------------------------------------------
/* Returns a string with the supplied separator depth + 1 times in it */
std::string OutputInDepth(const unsigned long Depth, const char* Separator)
{
   std::string output = "";
   for(unsigned long d=Depth+1; d > 0; d--)
      output.append(Separator);
   return output;
}
									/*}}}*/
// URItoFileName - Convert the uri into a unique file name		/*{{{*/
// ---------------------------------------------------------------------
/* This converts a URI into a safe filename. It quotes all unsafe characters
   and converts / to _ and removes the scheme identifier. The resulting
   file name should be unique and never occur again for a different file */
string URItoFileName(const string &URI)
{
   // Nuke 'sensitive' items
   ::URI U(URI);
   U.User.clear();
   U.Password.clear();
   U.Access.clear();
   
   // "\x00-\x20{}|\\\\^\\[\\]<>\"\x7F-\xFF";
   string NewURI = QuoteString(U,"\\|{}[]<>\"^~_=!@#$%^&*");
   replace(NewURI.begin(),NewURI.end(),'/','_');
   return NewURI;
}
									/*}}}*/
// Base64Encode - Base64 Encoding routine for short strings		/*{{{*/
// ---------------------------------------------------------------------
/* This routine performs a base64 transformation on a string. It was ripped
   from wget and then patched and bug fixed.
 
   This spec can be found in rfc2045 */
string Base64Encode(const string &S)
{
   // Conversion table.
   static char tbl[64] = {'A','B','C','D','E','F','G','H',
   			  'I','J','K','L','M','N','O','P',
                          'Q','R','S','T','U','V','W','X',
                          'Y','Z','a','b','c','d','e','f',
                          'g','h','i','j','k','l','m','n',
                          'o','p','q','r','s','t','u','v',
                          'w','x','y','z','0','1','2','3',
                          '4','5','6','7','8','9','+','/'};
   
   // Pre-allocate some space
   string Final;
   Final.reserve((4*S.length() + 2)/3 + 2);

   /* Transform the 3x8 bits to 4x6 bits, as required by
      base64.  */
   for (string::const_iterator I = S.begin(); I < S.end(); I += 3)
   {
      char Bits[3] = {0,0,0};
      Bits[0] = I[0];
      if (I + 1 < S.end())
	 Bits[1] = I[1];
      if (I + 2 < S.end())
	 Bits[2] = I[2];

      Final += tbl[Bits[0] >> 2];
      Final += tbl[((Bits[0] & 3) << 4) + (Bits[1] >> 4)];
      
      if (I + 1 >= S.end())
	 break;
      
      Final += tbl[((Bits[1] & 0xf) << 2) + (Bits[2] >> 6)];
      
      if (I + 2 >= S.end())
	 break;
      
      Final += tbl[Bits[2] & 0x3f];
   }

   /* Apply the padding elements, this tells how many bytes the remote
      end should discard */
   if (S.length() % 3 == 2)
      Final += '=';
   if (S.length() % 3 == 1)
      Final += "==";
   
   return Final;
}
									/*}}}*/
// stringcmp - Arbitrary string compare					/*{{{*/
// ---------------------------------------------------------------------
/* This safely compares two non-null terminated strings of arbitrary 
   length */
int stringcmp(const char *A,const char *AEnd,const char *B,const char *BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (*A != *B)
	 break;
   
   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (*A < *B)
      return -1;
   return 1;
}

#if __GNUC__ >= 3
int stringcmp(string::const_iterator A,string::const_iterator AEnd,
	      const char *B,const char *BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (*A != *B)
	 break;
   
   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (*A < *B)
      return -1;
   return 1;
}
int stringcmp(string::const_iterator A,string::const_iterator AEnd,
	      string::const_iterator B,string::const_iterator BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (*A != *B)
	 break;
   
   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (*A < *B)
      return -1;
   return 1;
}
#endif
									/*}}}*/
// stringcasecmp - Arbitrary case insensitive string compare		/*{{{*/
// ---------------------------------------------------------------------
/* */
int stringcasecmp(const char *A,const char *AEnd,const char *B,const char *BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (tolower_ascii(*A) != tolower_ascii(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (tolower_ascii(*A) < tolower_ascii(*B))
      return -1;
   return 1;
}
#if __GNUC__ >= 3
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
		  const char *B,const char *BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (tolower_ascii(*A) != tolower_ascii(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (tolower_ascii(*A) < tolower_ascii(*B))
      return -1;
   return 1;
}
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
		  string::const_iterator B,string::const_iterator BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (tolower_ascii(*A) != tolower_ascii(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (tolower_ascii(*A) < tolower_ascii(*B))
      return -1;
   return 1;
}
#endif
									/*}}}*/
// LookupTag - Lookup the value of a tag in a taged string		/*{{{*/
// ---------------------------------------------------------------------
/* The format is like those used in package files and the method 
   communication system */
string LookupTag(const string &Message,const char *Tag,const char *Default)
{
   // Look for a matching tag.
   int Length = strlen(Tag);
   for (string::const_iterator I = Message.begin(); I + Length < Message.end(); ++I)
   {
      // Found the tag
      if (I[Length] == ':' && stringcasecmp(I,I+Length,Tag) == 0)
      {
	 // Find the end of line and strip the leading/trailing spaces
	 string::const_iterator J;
	 I += Length + 1;
	 for (; isspace_ascii(*I) != 0 && I < Message.end(); ++I);
	 for (J = I; *J != '\n' && J < Message.end(); ++J);
	 for (; J > I && isspace_ascii(J[-1]) != 0; --J);
	 
	 return string(I,J);
      }
      
      for (; *I != '\n' && I < Message.end(); ++I);
   }   
   
   // Failed to find a match
   if (Default == 0)
      return string();
   return Default;
}
									/*}}}*/
// StringToBool - Converts a string into a boolean			/*{{{*/
// ---------------------------------------------------------------------
/* This inspects the string to see if it is true or if it is false and
   then returns the result. Several varients on true/false are checked. */
int StringToBool(const string &Text,int Default)
{
   char *ParseEnd;
   int Res = strtol(Text.c_str(),&ParseEnd,0);
   // ensure that the entire string was converted by strtol to avoid
   // failures on "apt-cache show -a 0ad" where the "0" is converted
   const char *TextEnd = Text.c_str()+Text.size();
   if (ParseEnd == TextEnd && Res >= 0 && Res <= 1)
      return Res;
   
   // Check for positives
   if (strcasecmp(Text.c_str(),"no") == 0 ||
       strcasecmp(Text.c_str(),"false") == 0 ||
       strcasecmp(Text.c_str(),"without") == 0 ||
       strcasecmp(Text.c_str(),"off") == 0 ||
       strcasecmp(Text.c_str(),"disable") == 0)
      return 0;
   
   // Check for negatives
   if (strcasecmp(Text.c_str(),"yes") == 0 ||
       strcasecmp(Text.c_str(),"true") == 0 ||
       strcasecmp(Text.c_str(),"with") == 0 ||
       strcasecmp(Text.c_str(),"on") == 0 ||
       strcasecmp(Text.c_str(),"enable") == 0)
      return 1;
   
   return Default;
}
									/*}}}*/
// TimeRFC1123 - Convert a time_t into RFC1123 format			/*{{{*/
// ---------------------------------------------------------------------
/* This converts a time_t into a string time representation that is
   year 2000 compliant and timezone neutral */
string TimeRFC1123(time_t Date)
{
   return TimeRFC1123(Date, false);
}
string TimeRFC1123(time_t Date, bool const NumericTimezone)
{
   struct tm Conv;
   if (gmtime_r(&Date, &Conv) == NULL)
      return "";

   auto const posix = std::locale::classic();
   std::ostringstream datestr;
   datestr.imbue(posix);
   APT::StringView const fmt("%a, %d %b %Y %H:%M:%S");
   std::use_facet<std::time_put<char>>(posix).put(
                    std::ostreambuf_iterator<char>(datestr),
                    datestr, ' ', &Conv, fmt.data(), fmt.data() + fmt.size());
   if (NumericTimezone)
      datestr << " +0000";
   else
      datestr << " GMT";
   return datestr.str();
}
									/*}}}*/
// ReadMessages - Read messages from the FD				/*{{{*/
// ---------------------------------------------------------------------
/* This pulls full messages from the input FD into the message buffer. 
   It assumes that messages will not pause during transit so no
   fancy buffering is used.

   In particular: this reads blocks from the input until it believes
   that it's run out of input text.  Each block is terminated by a
   double newline ('\n' followed by '\n').
 */
bool ReadMessages(int Fd, vector<string> &List)
{
   char Buffer[64000];
   // Represents any left-over from the previous iteration of the
   // parse loop.  (i.e., if a message is split across the end
   // of the buffer, it goes here)
   string PartialMessage;

   do {
      int const Res = read(Fd, Buffer, sizeof(Buffer));
      if (Res < 0 && errno == EINTR)
	 continue;

      // process we read from has died
      if (Res == 0)
	 return false;

      // No data
#if EAGAIN != EWOULDBLOCK
      if (Res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
#else
      if (Res < 0 && errno == EAGAIN)
#endif
	 return true;
      if (Res < 0)
	 return false;

      // extract the message(s) from the buffer
      char const *Start = Buffer;
      char const * const End = Buffer + Res;

      char const * NL = (char const *) memchr(Start, '\n', End - Start);
      if (NL == NULL)
      {
	 // end of buffer: store what we have so far and read new data in
	 PartialMessage.append(Start, End - Start);
	 Start = End;
      }
      else
	 ++NL;

      if (PartialMessage.empty() == false && Start < End)
      {
	 // if we start with a new line, see if the partial message we have ended with one
	 // so that we properly detect records ending between two read() runs
	 // cases are: \n|\n  ,  \r\n|\r\n  and  \r\n\r|\n
	 // the case \r|\n\r\n is handled by the usual double-newline handling
	 if ((NL - Start) == 1 || ((NL - Start) == 2 && *Start == '\r'))
	 {
	    if (APT::String::Endswith(PartialMessage, "\n") || APT::String::Endswith(PartialMessage, "\r\n\r"))
	    {
	       PartialMessage.erase(PartialMessage.find_last_not_of("\r\n") + 1);
	       List.push_back(PartialMessage);
	       PartialMessage.clear();
	       while (NL < End && (*NL == '\n' || *NL == '\r')) ++NL;
	       Start = NL;
	    }
	 }
      }

      while (Start < End) {
	 char const * NL2 = (char const *) memchr(NL, '\n', End - NL);
	 if (NL2 == NULL)
	 {
	    // end of buffer: store what we have so far and read new data in
	    PartialMessage.append(Start, End - Start);
	    break;
	 }
	 ++NL2;

	 // did we find a double newline?
	 if ((NL2 - NL) == 1 || ((NL2 - NL) == 2 && *NL == '\r'))
	 {
	    PartialMessage.append(Start, NL2 - Start);
	    PartialMessage.erase(PartialMessage.find_last_not_of("\r\n") + 1);
	    List.push_back(PartialMessage);
	    PartialMessage.clear();
	    while (NL2 < End && (*NL2 == '\n' || *NL2 == '\r')) ++NL2;
	    Start = NL2;
	 }
	 NL = NL2;
      }

      // we have read at least one complete message and nothing left
      if (PartialMessage.empty() == true)
	 return true;

      if (WaitFd(Fd) == false)
	 return false;
   } while (true);
}
									/*}}}*/
// MonthConv - Converts a month string into a number			/*{{{*/
// ---------------------------------------------------------------------
/* This was lifted from the boa webserver which lifted it from 'wn-v1.07'
   Made it a bit more robust with a few tolower_ascii though. */
static int MonthConv(char const * const Month)
{
   switch (tolower_ascii(*Month)) 
   {
      case 'a':
      return tolower_ascii(Month[1]) == 'p'?3:7;
      case 'd':
      return 11;
      case 'f':
      return 1;
      case 'j':
      if (tolower_ascii(Month[1]) == 'a')
	 return 0;
      return tolower_ascii(Month[2]) == 'n'?5:6;
      case 'm':
      return tolower_ascii(Month[2]) == 'r'?2:4;
      case 'n':
      return 10;
      case 'o':
      return 9;
      case 's':
      return 8;

      // Pretend it is January..
      default:
      return 0;
   }   
}
									/*}}}*/
// timegm - Internal timegm if the gnu version is not available		/*{{{*/
// ---------------------------------------------------------------------
/* Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone (mktime assumes the latter).

   This function is a nonstandard GNU extension that is also present on
   the BSDs and maybe other systems. For others we follow the advice of
   the manpage of timegm and use his portable replacement. */
#ifndef HAVE_TIMEGM
static time_t timegm(struct tm *t)
{
   char *tz = getenv("TZ");
   setenv("TZ", "", 1);
   tzset();
   time_t ret = mktime(t);
   if (tz)
      setenv("TZ", tz, 1);
   else
      unsetenv("TZ");
   tzset();
   return ret;
}
#endif
									/*}}}*/
// RFC1123StrToTime - Converts a HTTP1.1 full date strings into a time_t	/*{{{*/
// ---------------------------------------------------------------------
/* tries to parses a full date as specified in RFC7231 §7.1.1.1
   with one exception: HTTP/1.1 valid dates need to have GMT as timezone.
   As we encounter dates from UTC or with a numeric timezone in other places,
   we allow them here to to be able to reuse the method. Either way, a date
   must be in UTC or parsing will fail. Previous implementations of this
   method used to ignore the timezone and assume always UTC. */
bool RFC1123StrToTime(const char* const str,time_t &time)
{
   unsigned short day = 0;
   signed int year = 0; // yes, Y23K problem – we gonna worry then…
   std::string weekday, month, datespec, timespec, zone;
   std::istringstream ss(str);
   auto const &posix = std::locale::classic();
   ss.imbue(posix);
   ss >> weekday;
   // we only superficially check weekday, mostly to avoid accepting localized
   // weekdays here and take only its length to decide which datetime format we
   // encounter here. The date isn't stored.
   std::transform(weekday.begin(), weekday.end(), weekday.begin(), ::tolower);
   std::array<char const * const, 7> c_weekdays = {{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" }};
   if (std::find(c_weekdays.begin(), c_weekdays.end(), weekday.substr(0,3)) == c_weekdays.end())
      return false;

   switch (weekday.length())
   {
   case 4:
      // Sun, 06 Nov 1994 08:49:37 GMT ; RFC 822, updated by RFC 1123
      if (weekday[3] != ',')
	 return false;
      ss >> day >> month >> year >> timespec >> zone;
      break;
   case 3:
      // Sun Nov  6 08:49:37 1994 ; ANSI C's asctime() format
      ss >> month >> day >> timespec >> year;
      zone = "UTC";
      break;
   case 0:
   case 1:
   case 2:
      return false;
   default:
      // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
      if (weekday[weekday.length() - 1] != ',')
	 return false;
      ss >> datespec >> timespec >> zone;
      auto const expldate = VectorizeString(datespec, '-');
      if (expldate.size() != 3)
	 return false;
      try {
	 size_t pos;
	 day = std::stoi(expldate[0], &pos);
	 if (pos != expldate[0].length())
	    return false;
	 year = 1900 + std::stoi(expldate[2], &pos);
	 if (pos != expldate[2].length())
	    return false;
	 strprintf(datespec, "%.4d-%.2d-%.2d", year, MonthConv(expldate[1].c_str()) + 1, day);
      } catch (...) {
         return false;
      }
      break;
   }

   if (ss.fail() || ss.bad() || !ss.eof())
      return false;

   if (zone != "GMT" && zone != "UTC" && zone != "Z") // RFC 822
   {
      // numeric timezones as a should of RFC 1123 and generally preferred
      try {
	 size_t pos;
	 auto const z = std::stoi(zone, &pos);
	 if (z != 0 || pos != zone.length())
	    return false;
      } catch (...) {
	 return false;
      }
   }

   if (datespec.empty())
   {
      if (month.empty())
	 return false;
      strprintf(datespec, "%.4d-%.2d-%.2d", year, MonthConv(month.c_str()) + 1, day);
   }

   std::string const datetime = datespec + ' ' + timespec;
   struct tm Tm;
   if (strptime(datetime.c_str(), "%Y-%m-%d %H:%M:%S", &Tm) == nullptr)
      return false;
   time = timegm(&Tm);
   return true;
}
									/*}}}*/
// FTPMDTMStrToTime - Converts a ftp modification date into a time_t	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FTPMDTMStrToTime(const char* const str,time_t &time)
{
   struct tm Tm;
   // MDTM includes no whitespaces but recommend and ignored by strptime
   if (strptime(str, "%Y %m %d %H %M %S", &Tm) == NULL)
      return false;

   time = timegm(&Tm);
   return true;
}
									/*}}}*/
// StrToTime - Converts a string into a time_t				/*{{{*/
// ---------------------------------------------------------------------
/* This handles all 3 popular time formats including RFC 1123, RFC 1036
   and the C library asctime format. It requires the GNU library function
   'timegm' to convert a struct tm in UTC to a time_t. For some bizzar
   reason the C library does not provide any such function :< This also
   handles the weird, but unambiguous FTP time format*/
bool StrToTime(const string &Val,time_t &Result)
{
   struct tm Tm;
   char Month[10];

   // Skip the day of the week
   const char *I = strchr(Val.c_str(), ' ');

   // Handle RFC 1123 time
   Month[0] = 0;
   if (sscanf(I," %2d %3s %4d %2d:%2d:%2d GMT",&Tm.tm_mday,Month,&Tm.tm_year,
	      &Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec) != 6)
   {
      // Handle RFC 1036 time
      if (sscanf(I," %2d-%3s-%3d %2d:%2d:%2d GMT",&Tm.tm_mday,Month,
		 &Tm.tm_year,&Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec) == 6)
	 Tm.tm_year += 1900;
      else
      {
	 // asctime format
	 if (sscanf(I," %3s %2d %2d:%2d:%2d %4d",Month,&Tm.tm_mday,
		    &Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec,&Tm.tm_year) != 6)
	 {
	    // 'ftp' time
	    if (sscanf(Val.c_str(),"%4d%2d%2d%2d%2d%2d",&Tm.tm_year,&Tm.tm_mon,
		       &Tm.tm_mday,&Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec) != 6)
	       return false;
	    Tm.tm_mon--;
	 }	 
      }
   }
   
   Tm.tm_isdst = 0;
   if (Month[0] != 0)
      Tm.tm_mon = MonthConv(Month);
   else
      Tm.tm_mon = 0; // we don't have a month, so pick something
   Tm.tm_year -= 1900;
   
   // Convert to local time and then to GMT
   Result = timegm(&Tm);
   return true;
}
									/*}}}*/
// StrToNum - Convert a fixed length string to a number			/*{{{*/
// ---------------------------------------------------------------------
/* This is used in decoding the crazy fixed length string headers in 
   tar and ar files. */
bool StrToNum(const char *Str,unsigned long &Res,unsigned Len,unsigned Base)
{
   char S[30];
   if (Len >= sizeof(S))
      return false;
   memcpy(S,Str,Len);
   S[Len] = 0;
   
   // All spaces is a zero
   Res = 0;
   unsigned I;
   for (I = 0; S[I] == ' '; I++);
   if (S[I] == 0)
      return true;
   
   char *End;
   Res = strtoul(S,&End,Base);
   if (End == S)
      return false;
   
   return true;
}
									/*}}}*/
// StrToNum - Convert a fixed length string to a number			/*{{{*/
// ---------------------------------------------------------------------
/* This is used in decoding the crazy fixed length string headers in 
   tar and ar files. */
bool StrToNum(const char *Str,unsigned long long &Res,unsigned Len,unsigned Base)
{
   char S[30];
   if (Len >= sizeof(S))
      return false;
   memcpy(S,Str,Len);
   S[Len] = 0;
   
   // All spaces is a zero
   Res = 0;
   unsigned I;
   for (I = 0; S[I] == ' '; I++);
   if (S[I] == 0)
      return true;
   
   char *End;
   Res = strtoull(S,&End,Base);
   if (End == S)
      return false;
   
   return true;
}
									/*}}}*/

// Base256ToNum - Convert a fixed length binary to a number             /*{{{*/
// ---------------------------------------------------------------------
/* This is used in decoding the 256bit encoded fixed length fields in
   tar files */
bool Base256ToNum(const char *Str,unsigned long long &Res,unsigned int Len)
{
   if ((Str[0] & 0x80) == 0)
      return false;
   else
   {
      Res = Str[0] & 0x7F;
      for(unsigned int i = 1; i < Len; ++i)
         Res = (Res<<8) + Str[i];
      return true;
   }
}
									/*}}}*/
// Base256ToNum - Convert a fixed length binary to a number             /*{{{*/
// ---------------------------------------------------------------------
/* This is used in decoding the 256bit encoded fixed length fields in
   tar files */
bool Base256ToNum(const char *Str,unsigned long &Res,unsigned int Len)
{
   unsigned long long Num = 0;
   bool rc;

   rc = Base256ToNum(Str, Num, Len);
   // rudimentary check for overflow (Res = ulong, Num = ulonglong)
   Res = Num;
   if (Res != Num)
      return false;

   return rc;
}
									/*}}}*/
// HexDigit - Convert a hex character into an integer			/*{{{*/
// ---------------------------------------------------------------------
/* Helper for Hex2Num */
static int HexDigit(int c)
{   
   if (c >= '0' && c <= '9')
      return c - '0';
   if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
   if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
   return -1;
}
									/*}}}*/
// Hex2Num - Convert a long hex number into a buffer			/*{{{*/
// ---------------------------------------------------------------------
/* The length of the buffer must be exactly 1/2 the length of the string. */
bool Hex2Num(const string &Str,unsigned char *Num,unsigned int Length)
{
   return Hex2Num(APT::StringView(Str), Num, Length);
}

bool Hex2Num(const APT::StringView Str,unsigned char *Num,unsigned int Length)
{
   if (Str.length() != Length*2)
      return false;
   
   // Convert each digit. We store it in the same order as the string
   int J = 0;
   for (auto I = Str.begin(); I != Str.end();J++, I += 2)
   {
      int first_half = HexDigit(I[0]);
      int second_half;
      if (first_half < 0)
	 return false;
      
      second_half = HexDigit(I[1]);
      if (second_half < 0)
	 return false;
      Num[J] = first_half << 4;
      Num[J] += second_half;
   }
   
   return true;
}
									/*}}}*/
// TokSplitString - Split a string up by a given token			/*{{{*/
// ---------------------------------------------------------------------
/* This is intended to be a faster splitter, it does not use dynamic
   memories. Input is changed to insert nulls at each token location. */
bool TokSplitString(char Tok,char *Input,char **List,
		    unsigned long ListMax)
{
   // Strip any leading spaces
   char *Start = Input;
   char *Stop = Start + strlen(Start);
   for (; *Start != 0 && isspace(*Start) != 0; Start++);

   unsigned long Count = 0;
   char *Pos = Start;
   while (Pos != Stop)
   {
      // Skip to the next Token
      for (; Pos != Stop && *Pos != Tok; Pos++);
      
      // Back remove spaces
      char *End = Pos;
      for (; End > Start && (End[-1] == Tok || isspace(End[-1]) != 0); End--);
      *End = 0;
      
      List[Count++] = Start;
      if (Count >= ListMax)
      {
	 List[Count-1] = 0;
	 return false;
      }
      
      // Advance pos
      for (; Pos != Stop && (*Pos == Tok || isspace(*Pos) != 0 || *Pos == 0); Pos++);
      Start = Pos;
   }
   
   List[Count] = 0;
   return true;
}
									/*}}}*/
// VectorizeString - Split a string up into a vector of strings		/*{{{*/
// ---------------------------------------------------------------------
/* This can be used to split a given string up into a vector, so the
   propose is the same as in the method above and this one is a bit slower
   also, but the advantage is that we have an iteratable vector */
vector<string> VectorizeString(string const &haystack, char const &split)
{
   vector<string> exploded;
   if (haystack.empty() == true)
      return exploded;
   string::const_iterator start = haystack.begin();
   string::const_iterator end = start;
   do {
      for (; end != haystack.end() && *end != split; ++end);
      exploded.push_back(string(start, end));
      start = end + 1;
   } while (end != haystack.end() && (++end) != haystack.end());
   return exploded;
}
									/*}}}*/
// StringSplit - split a string into a string vector by token		/*{{{*/
// ---------------------------------------------------------------------
/* See header for details.
 */
vector<string> StringSplit(std::string const &s, std::string const &sep,
                           unsigned int maxsplit)
{
   vector<string> split;
   size_t start, pos;

   // no separator given, this is bogus
   if(sep.size() == 0)
      return split;

   start = pos = 0;
   while (pos != string::npos)
   {
      pos = s.find(sep, start);
      split.push_back(s.substr(start, pos-start));
      
      // if maxsplit is reached, the remaining string is the last item
      if(split.size() >= maxsplit)
      {
         split[split.size()-1] = s.substr(start);
         break;
      }
      start = pos+sep.size();
   }
   return split;
}
									/*}}}*/
// RegexChoice - Simple regex list/list matcher				/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long RegexChoice(RxChoiceList *Rxs,const char **ListBegin,
		      const char **ListEnd)
{
   for (RxChoiceList *R = Rxs; R->Str != 0; R++)
      R->Hit = false;

   unsigned long Hits = 0;
   for (; ListBegin < ListEnd; ++ListBegin)
   {
      // Check if the name is a regex
      const char *I;
      bool Regex = true;
      for (I = *ListBegin; *I != 0; I++)
	 if (*I == '.' || *I == '?' || *I == '*' || *I == '|')
	    break;
      if (*I == 0)
	 Regex = false;
	 
      // Compile the regex pattern
      regex_t Pattern;
      if (Regex == true)
	 if (regcomp(&Pattern,*ListBegin,REG_EXTENDED | REG_ICASE |
		     REG_NOSUB) != 0)
	    Regex = false;
	 
      // Search the list
      bool Done = false;
      for (RxChoiceList *R = Rxs; R->Str != 0; R++)
      {
	 if (R->Str[0] == 0)
	    continue;
	 
	 if (strcasecmp(R->Str,*ListBegin) != 0)
	 {
	    if (Regex == false)
	       continue;
	    if (regexec(&Pattern,R->Str,0,0,0) != 0)
	       continue;
	 }
	 Done = true;
	 
	 if (R->Hit == false)
	    Hits++;
	 
	 R->Hit = true;
      }
      
      if (Regex == true)
	 regfree(&Pattern);
      
      if (Done == false)
	 _error->Warning(_("Selection %s not found"),*ListBegin);
   }
   
   return Hits;
}
									/*}}}*/
// {str,io}printf - C format string outputter to C++ strings/iostreams	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to make the internationalization strings easier to translate
   and to allow reordering of parameters */
static bool iovprintf(ostream &out, const char *format,
		      va_list &args, ssize_t &size) {
   char *S = (char*)malloc(size);
   ssize_t const n = vsnprintf(S, size, format, args);
   if (n > -1 && n < size) {
      out << S;
      free(S);
      return true;
   } else {
      if (n > -1)
	 size = n + 1;
      else
	 size *= 2;
   }
   free(S);
   return false;
}
void ioprintf(ostream &out,const char *format,...)
{
   va_list args;
   ssize_t size = 400;
   while (true) {
      bool ret;
      va_start(args,format);
      ret = iovprintf(out, format, args, size);
      va_end(args);
      if (ret == true)
	 return;
   }
}
void strprintf(string &out,const char *format,...)
{
   va_list args;
   ssize_t size = 400;
   std::ostringstream outstr;
   while (true) {
      bool ret;
      va_start(args,format);
      ret = iovprintf(outstr, format, args, size);
      va_end(args);
      if (ret == true)
	 break;
   }
   out = outstr.str();
}
									/*}}}*/
// safe_snprintf - Safer snprintf					/*{{{*/
// ---------------------------------------------------------------------
/* This is a snprintf that will never (ever) go past 'End' and returns a
   pointer to the end of the new string. The returned string is always null
   terminated unless Buffer == end. This is a better alterantive to using
   consecutive snprintfs. */
char *safe_snprintf(char *Buffer,char *End,const char *Format,...)
{
   va_list args;
   int Did;

   if (End <= Buffer)
      return End;
   va_start(args,Format);
   Did = vsnprintf(Buffer,End - Buffer,Format,args);
   va_end(args);

   if (Did < 0 || Buffer + Did > End)
      return End;
   return Buffer + Did;
}
									/*}}}*/
// StripEpoch - Remove the version "epoch" from a version string	/*{{{*/
// ---------------------------------------------------------------------
string StripEpoch(const string &VerStr)
{
   size_t i = VerStr.find(":");
   if (i == string::npos)
      return VerStr;
   return VerStr.substr(i+1);
}
									/*}}}*/

// tolower_ascii - tolower() function that ignores the locale		/*{{{*/
// ---------------------------------------------------------------------
/* This little function is the most called method we have and tries
   therefore to do the absolute minimum - and is notable faster than
   standard tolower/toupper and as a bonus avoids problems with different
   locales - we only operate on ascii chars anyway. */
#undef tolower_ascii
int tolower_ascii(int const c) APT_CONST APT_COLD;
int tolower_ascii(int const c)
{
   return tolower_ascii_inline(c);
}
									/*}}}*/

// isspace_ascii - isspace() function that ignores the locale		/*{{{*/
// ---------------------------------------------------------------------
/* This little function is one of the most called methods we have and tries
   therefore to do the absolute minimum - and is notable faster than
   standard isspace() and as a bonus avoids problems with different
   locales - we only operate on ascii chars anyway. */
#undef isspace_ascii
int isspace_ascii(int const c) APT_CONST APT_COLD;
int isspace_ascii(int const c)
{
   return isspace_ascii_inline(c);
}
									/*}}}*/

// CheckDomainList - See if Host is in a , separate list		/*{{{*/
// ---------------------------------------------------------------------
/* The domain list is a comma separate list of domains that are suffix
   matched against the argument */
bool CheckDomainList(const string &Host,const string &List)
{
   string::const_iterator Start = List.begin();
   for (string::const_iterator Cur = List.begin(); Cur <= List.end(); ++Cur)
   {
      if (Cur < List.end() && *Cur != ',')
	 continue;
      
      // Match the end of the string..
      if ((Host.size() >= (unsigned)(Cur - Start)) &&
	  Cur - Start != 0 &&
	  stringcasecmp(Host.end() - (Cur - Start),Host.end(),Start,Cur) == 0)
	 return true;
      
      Start = Cur + 1;
   }
   return false;
}
									/*}}}*/
// strv_length - Return the length of a NULL-terminated string array	/*{{{*/
// ---------------------------------------------------------------------
/* */
size_t strv_length(const char **str_array)
{
   size_t i;
   for (i=0; str_array[i] != NULL; i++)
      /* nothing */
      ;
   return i;
}
									/*}}}*/
// DeEscapeString - unescape (\0XX and \xXX) from a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
string DeEscapeString(const string &input)
{
   char tmp[3];
   string::const_iterator it;
   string output;
   for (it = input.begin(); it != input.end(); ++it)
   {
      // just copy non-escape chars
      if (*it != '\\')
      {
         output += *it;
         continue;
      }

      // deal with double escape
      if (*it == '\\' && 
          (it + 1 < input.end()) &&  it[1] == '\\')
      {
         // copy
         output += *it;
         // advance iterator one step further
         ++it;
         continue;
      }
        
      // ensure we have a char to read
      if (it + 1 == input.end())
         continue;

      // read it
      ++it;
      switch (*it)
      {
         case '0':
            if (it + 2 <= input.end()) {
               tmp[0] = it[1];
               tmp[1] = it[2];
               tmp[2] = 0;
               output += (char)strtol(tmp, 0, 8);
               it += 2;
            }
            break;
         case 'x':
            if (it + 2 <= input.end()) {
               tmp[0] = it[1];
               tmp[1] = it[2];
               tmp[2] = 0;
               output += (char)strtol(tmp, 0, 16);
               it += 2;
            }
            break;
         default:
            // FIXME: raise exception here?
            break;
      }
   }
   return output;
}
									/*}}}*/
// URI::CopyFrom - Copy from an object					/*{{{*/
// ---------------------------------------------------------------------
/* This parses the URI into all of its components */
void URI::CopyFrom(const string &U)
{
   string::const_iterator I = U.begin();

   // Locate the first colon, this separates the scheme
   for (; I < U.end() && *I != ':' ; ++I);
   string::const_iterator FirstColon = I;

   /* Determine if this is a host type URI with a leading double //
      and then search for the first single / */
   string::const_iterator SingleSlash = I;
   if (I + 3 < U.end() && I[1] == '/' && I[2] == '/')
      SingleSlash += 3;
   
   /* Find the / indicating the end of the hostname, ignoring /'s in the
      square brackets */
   bool InBracket = false;
   for (; SingleSlash < U.end() && (*SingleSlash != '/' || InBracket == true); ++SingleSlash)
   {
      if (*SingleSlash == '[')
	 InBracket = true;
      if (InBracket == true && *SingleSlash == ']')
	 InBracket = false;
   }
   
   if (SingleSlash > U.end())
      SingleSlash = U.end();

   // We can now write the access and path specifiers
   Access.assign(U.begin(),FirstColon);
   if (SingleSlash != U.end())
      Path.assign(SingleSlash,U.end());
   if (Path.empty() == true)
      Path = "/";

   // Now we attempt to locate a user:pass@host fragment
   if (FirstColon + 2 <= U.end() && FirstColon[1] == '/' && FirstColon[2] == '/')
      FirstColon += 3;
   else
      FirstColon += 1;
   if (FirstColon >= U.end())
      return;
   
   if (FirstColon > SingleSlash)
      FirstColon = SingleSlash;
   
   // Find the colon...
   I = FirstColon + 1;
   if (I > SingleSlash)
      I = SingleSlash;

   // Search for the @ separating user:pass from host
   auto const RevAt = std::find(
	 std::string::const_reverse_iterator(SingleSlash),
	 std::string::const_reverse_iterator(I), '@');
   string::const_iterator const At = RevAt.base() == I ? SingleSlash : std::prev(RevAt.base());
   // and then look for the colon between user and pass
   string::const_iterator const SecondColon = std::find(I, At, ':');

   // Now write the host and user/pass
   if (At == SingleSlash)
   {
      if (FirstColon < SingleSlash)
	 Host.assign(FirstColon,SingleSlash);
   }
   else
   {
      Host.assign(At+1,SingleSlash);
      // username and password must be encoded (RFC 3986)
      User.assign(DeQuoteString(FirstColon,SecondColon));
      if (SecondColon < At)
	 Password.assign(DeQuoteString(SecondColon+1,At));
   }   
   
   // Now we parse the RFC 2732 [] hostnames.
   unsigned long PortEnd = 0;
   InBracket = false;
   for (unsigned I = 0; I != Host.length();)
   {
      if (Host[I] == '[')
      {
	 InBracket = true;
	 Host.erase(I,1);
	 continue;
      }
      
      if (InBracket == true && Host[I] == ']')
      {
	 InBracket = false;
	 Host.erase(I,1);
	 PortEnd = I;
	 continue;
      }
      I++;
   }
   
   // Tsk, weird.
   if (InBracket == true)
   {
      Host.clear();
      return;
   }
   
   // Now we parse off a port number from the hostname
   Port = 0;
   string::size_type Pos = Host.rfind(':');
   if (Pos == string::npos || Pos < PortEnd)
      return;
   
   Port = atoi(string(Host,Pos+1).c_str());
   Host.assign(Host,0,Pos);
}
									/*}}}*/
// URI::operator string - Convert the URI to a string			/*{{{*/
// ---------------------------------------------------------------------
/* */
URI::operator string()
{
   std::stringstream Res;

   if (Access.empty() == false)
      Res << Access << ':';

   if (Host.empty() == false)
   {
      if (Access.empty() == false)
	 Res << "//";

      if (User.empty() == false)
      {
	 // FIXME: Technically userinfo is permitted even less
	 // characters than these, but this is not conveniently
	 // expressed with a blacklist.
	 Res << QuoteString(User, ":/?#[]@");
	 if (Password.empty() == false)
	    Res << ":" << QuoteString(Password, ":/?#[]@");
	 Res << "@";
      }

      // Add RFC 2732 escaping characters
      if (Access.empty() == false && Host.find_first_of("/:") != string::npos)
	 Res << '[' << Host << ']';
      else
	 Res << Host;

      if (Port != 0)
	 Res << ':' << std::to_string(Port);
   }

   if (Path.empty() == false)
   {
      if (Path[0] != '/')
	 Res << "/" << Path;
      else
	 Res << Path;
   }

   return Res.str();
}
									/*}}}*/
// URI::SiteOnly - Return the schema and site for the URI		/*{{{*/
string URI::SiteOnly(const string &URI)
{
   ::URI U(URI);
   U.User.clear();
   U.Password.clear();
   U.Path.clear();
   return U;
}
									/*}}}*/
// URI::ArchiveOnly - Return the schema, site and cleaned path for the URI /*{{{*/
string URI::ArchiveOnly(const string &URI)
{
   ::URI U(URI);
   U.User.clear();
   U.Password.clear();
   if (U.Path.empty() == false && U.Path[U.Path.length() - 1] == '/')
      U.Path.erase(U.Path.length() - 1);
   return U;
}
									/*}}}*/
// URI::NoUserPassword - Return the schema, site and path for the URI	/*{{{*/
string URI::NoUserPassword(const string &URI)
{
   ::URI U(URI);
   U.User.clear();
   U.Password.clear();
   return U;
}
									/*}}}*/
