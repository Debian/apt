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
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>

#include <apti18n.h>
    
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <stdarg.h>
#include <iconv.h>

#include "config.h"

using namespace std;
									/*}}}*/

// UTF8ToCodeset - Convert some UTF-8 string for some codeset   	/*{{{*/
// ---------------------------------------------------------------------
/* This is handy to use before display some information for enduser  */
bool UTF8ToCodeset(const char *codeset, const string &orig, string *dest)
{
  iconv_t cd;
  const char *inbuf;
  char *inptr, *outbuf, *outptr;
  size_t insize, outsize;
  
  cd = iconv_open(codeset, "UTF-8");
  if (cd == (iconv_t)(-1)) {
     // Something went wrong
     if (errno == EINVAL)
	_error->Error("conversion from 'UTF-8' to '%s' not available",
               codeset);
     else
	perror("iconv_open");
     
     // Clean the destination string
     *dest = "";
     
     return false;
  }

  insize = outsize = orig.size();
  inbuf = orig.data();
  inptr = (char *)inbuf;
  outbuf = new char[insize+1];
  outptr = outbuf;

  iconv(cd, &inptr, &insize, &outptr, &outsize);
  *outptr = '\0';

  *dest = outbuf;
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

   char *End = String + strlen(String) - 1;
   for (;End != String - 1 && (*End == ' ' || *End == '\t' || *End == '\n' ||
			       *End == '\r'); End--);
   End++;
   *End = 0;
   return String;
};
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
	 for (C++; *C != 0 && *C != '"'; C++);
	 if (*C == 0)
	    return false;
      }
      if (*C == '[')
      {
	 for (C++; *C != 0 && *C != ']'; C++);
	 if (*C == 0)
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
      if (*Start == '%' && Start + 2 < C)
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
   string Res;
   for (string::const_iterator I = Str.begin(); I != Str.end(); I++)
   {
      if (strchr(Bad,*I) != 0 || isprint(*I) == 0 || 
	  *I <= 0x20 || *I >= 0x7F)
      {
	 char Buf[10];
	 sprintf(Buf,"%%%02x",(int)*I);
	 Res += Buf;
      }
      else
	 Res += *I;
   }
   return Res;
}
									/*}}}*/
// DeQuoteString - Convert a string from quoted from                    /*{{{*/
// ---------------------------------------------------------------------
/* This undoes QuoteString */
string DeQuoteString(const string &Str)
{
   string Res;
   for (string::const_iterator I = Str.begin(); I != Str.end(); I++)
   {
      if (*I == '%' && I + 2 < Str.end())
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
   char S[300];
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
         sprintf(S,"%.1f%c",ASize,Ext[I]);
	 break;
      }
      
      if (ASize < 10000)
      {
         sprintf(S,"%.0f%c",ASize,Ext[I]);
	 break;
      }
      ASize /= 1000.0;
      I++;
   }
   
   return S;
}
									/*}}}*/
// TimeToStr - Convert the time into a string				/*{{{*/
// ---------------------------------------------------------------------
/* Converts a number of seconds to a hms format */
string TimeToStr(unsigned long Sec)
{
   char S[300];
   
   while (1)
   {
      if (Sec > 60*60*24)
      {
	 sprintf(S,"%lid %lih%limin%lis",Sec/60/60/24,(Sec/60/60) % 24,(Sec/60) % 60,Sec % 60);
	 break;
      }
      
      if (Sec > 60*60)
      {
	 sprintf(S,"%lih%limin%lis",Sec/60/60,(Sec/60) % 60,Sec % 60);
	 break;
      }
      
      if (Sec > 60)
      {
	 sprintf(S,"%limin%lis",Sec/60,Sec % 60);
	 break;
      }
      
      sprintf(S,"%lis",Sec);
      break;
   }
   
   return S;
}
									/*}}}*/
// SubstVar - Substitute a string for another string			/*{{{*/
// ---------------------------------------------------------------------
/* This replaces all occurances of Subst with Contents in Str. */
string SubstVar(const string &Str,const string &Subst,const string &Contents)
{
   string::size_type Pos = 0;
   string::size_type OldPos = 0;
   string Temp;
   
   while (OldPos < Str.length() && 
	  (Pos = Str.find(Subst,OldPos)) != string::npos)
   {
      Temp += string(Str,OldPos,Pos) + Contents;
      OldPos = Pos + Subst.length();      
   }
   
   if (OldPos == 0)
      return Str;
   
   return Temp + string(Str,OldPos);
}

string SubstVar(string Str,const struct SubstVar *Vars)
{
   for (; Vars->Subst != 0; Vars++)
      Str = SubstVar(Str,Vars->Subst,*Vars->Contents);
   return Str;
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
      if (toupper(*A) != toupper(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (toupper(*A) < toupper(*B))
      return -1;
   return 1;
}
#if __GNUC__ >= 3
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
		  const char *B,const char *BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (toupper(*A) != toupper(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (toupper(*A) < toupper(*B))
      return -1;
   return 1;
}
int stringcasecmp(string::const_iterator A,string::const_iterator AEnd,
		  string::const_iterator B,string::const_iterator BEnd)
{
   for (; A != AEnd && B != BEnd; A++, B++)
      if (toupper(*A) != toupper(*B))
	 break;

   if (A == AEnd && B == BEnd)
      return 0;
   if (A == AEnd)
      return 1;
   if (B == BEnd)
      return -1;
   if (toupper(*A) < toupper(*B))
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
   for (string::const_iterator I = Message.begin(); I + Length < Message.end(); I++)
   {
      // Found the tag
      if (I[Length] == ':' && stringcasecmp(I,I+Length,Tag) == 0)
      {
	 // Find the end of line and strip the leading/trailing spaces
	 string::const_iterator J;
	 I += Length + 1;
	 for (; isspace(*I) != 0 && I < Message.end(); I++);
	 for (J = I; *J != '\n' && J < Message.end(); J++);
	 for (; J > I && isspace(J[-1]) != 0; J--);
	 
	 return string(I,J);
      }
      
      for (; *I != '\n' && I < Message.end(); I++);
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
   char *End;
   int Res = strtol(Text.c_str(),&End,0);   
   if (End != Text.c_str() && Res >= 0 && Res <= 1)
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
   year 2000 complient and timezone neutral */
string TimeRFC1123(time_t Date)
{
   struct tm Conv = *gmtime(&Date);
   char Buf[300];

   const char *Day[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
   const char *Month[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul",
                          "Aug","Sep","Oct","Nov","Dec"};

   sprintf(Buf,"%s, %02i %s %i %02i:%02i:%02i GMT",Day[Conv.tm_wday],
	   Conv.tm_mday,Month[Conv.tm_mon],Conv.tm_year+1900,Conv.tm_hour,
	   Conv.tm_min,Conv.tm_sec);
   return Buf;
}
									/*}}}*/
// ReadMessages - Read messages from the FD				/*{{{*/
// ---------------------------------------------------------------------
/* This pulls full messages from the input FD into the message buffer. 
   It assumes that messages will not pause during transit so no
   fancy buffering is used.

   In particular: this reads blocks from the input until it believes
   that it's run out of input text.  Each block is terminated by a
   double newline ('\n' followed by '\n').  As noted below, there is a
   bug in this code: it assumes that all the blocks have been read if
   it doesn't see additional text in the buffer after the last one is
   parsed, which will cause it to lose blocks if the last block
   coincides with the end of the buffer.
 */
bool ReadMessages(int Fd, vector<string> &List)
{
   char Buffer[64000];
   char *End = Buffer;
   // Represents any left-over from the previous iteration of the
   // parse loop.  (i.e., if a message is split across the end
   // of the buffer, it goes here)
   string PartialMessage;
   
   while (1)
   {
      int Res = read(Fd,End,sizeof(Buffer) - (End-Buffer));
      if (Res < 0 && errno == EINTR)
	 continue;
      
      // Process is dead, this is kind of bad..
      if (Res == 0)
	 return false;
      
      // No data
      if (Res < 0 && errno == EAGAIN)
	 return true;
      if (Res < 0)
	 return false;
			      
      End += Res;
      
      // Look for the end of the message
      for (char *I = Buffer; I + 1 < End; I++)
      {
	 if (I[0] != '\n' || I[1] != '\n')
	    continue;
	 
	 // Pull the message out
	 string Message(Buffer,I-Buffer);
	 PartialMessage += Message;

	 // Fix up the buffer
	 for (; I < End && *I == '\n'; I++);
	 End -= I-Buffer;	 
	 memmove(Buffer,I,End-Buffer);
	 I = Buffer;
	 
	 List.push_back(PartialMessage);
	 PartialMessage.clear();
      }
      if (End != Buffer)
	{
	  // If there's text left in the buffer, store it
	  // in PartialMessage and throw the rest of the buffer
	  // away.  This allows us to handle messages that
	  // are longer than the static buffer size.
	  PartialMessage += string(Buffer, End);
	  End = Buffer;
	}
      else
	{
	  // BUG ALERT: if a message block happens to end at a
	  // multiple of 64000 characters, this will cause it to
	  // terminate early, leading to a badly formed block and
	  // probably crashing the method.  However, this is the only
	  // way we have to find the end of the message block.  I have
	  // an idea of how to fix this, but it will require changes
	  // to the protocol (essentially to mark the beginning and
	  // end of the block).
	  //
	  //  -- dburrows 2008-04-02
	  return true;
	}

      if (WaitFd(Fd) == false)
	 return false;
   }   
}
									/*}}}*/
// MonthConv - Converts a month string into a number			/*{{{*/
// ---------------------------------------------------------------------
/* This was lifted from the boa webserver which lifted it from 'wn-v1.07'
   Made it a bit more robust with a few touppers though. */
static int MonthConv(char *Month)
{
   switch (toupper(*Month)) 
   {
      case 'A':
      return toupper(Month[1]) == 'P'?3:7;
      case 'D':
      return 11;
      case 'F':
      return 1;
      case 'J':
      if (toupper(Month[1]) == 'A')
	 return 0;
      return toupper(Month[2]) == 'N'?5:6;
      case 'M':
      return toupper(Month[2]) == 'R'?2:4;
      case 'N':
      return 10;
      case 'O':
      return 9;
      case 'S':
      return 8;

      // Pretend it is January..
      default:
      return 0;
   }   
}
									/*}}}*/
// timegm - Internal timegm function if gnu is not available		/*{{{*/
// ---------------------------------------------------------------------
/* Ripped this evil little function from wget - I prefer the use of 
   GNU timegm if possible as this technique will have interesting problems
   with leap seconds, timezones and other.
   
   Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone (mktime assumes the latter).
   
   Contributed by Roger Beeman <beeman@cisco.com>, with the help of
   Mark Baushke <mdb@cisco.com> and the rest of the Gurus at CISCO. */

/* Turned it into an autoconf check, because GNU is not the only thing which
   can provide timegm. -- 2002-09-22, Joel Baker */

#ifndef HAVE_TIMEGM // Now with autoconf!
static time_t timegm(struct tm *t)
{
   time_t tl, tb;
   
   tl = mktime (t);
   if (tl == -1)
      return -1;
   tb = mktime (gmtime (&tl));
   return (tl <= tb ? (tl + (tl - tb)) : (tl - (tb - tl)));
}
#endif
									/*}}}*/
// StrToTime - Converts a string into a time_t				/*{{{*/
// ---------------------------------------------------------------------
/* This handles all 3 populare time formats including RFC 1123, RFC 1036
   and the C library asctime format. It requires the GNU library function
   'timegm' to convert a struct tm in UTC to a time_t. For some bizzar
   reason the C library does not provide any such function :< This also
   handles the weird, but unambiguous FTP time format*/
bool StrToTime(const string &Val,time_t &Result)
{
   struct tm Tm;
   char Month[10];
   const char *I = Val.c_str();
   
   // Skip the day of the week
   for (;*I != 0  && *I != ' '; I++);
   
   // Handle RFC 1123 time
   Month[0] = 0;
   if (sscanf(I," %d %3s %d %d:%d:%d GMT",&Tm.tm_mday,Month,&Tm.tm_year,
	      &Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec) != 6)
   {
      // Handle RFC 1036 time
      if (sscanf(I," %d-%3s-%d %d:%d:%d GMT",&Tm.tm_mday,Month,
		 &Tm.tm_year,&Tm.tm_hour,&Tm.tm_min,&Tm.tm_sec) == 6)
	 Tm.tm_year += 1900;
      else
      {
	 // asctime format
	 if (sscanf(I," %3s %d %d:%d:%d %d",Month,&Tm.tm_mday,
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
   return 0;
}
									/*}}}*/
// Hex2Num - Convert a long hex number into a buffer			/*{{{*/
// ---------------------------------------------------------------------
/* The length of the buffer must be exactly 1/2 the length of the string. */
bool Hex2Num(const string &Str,unsigned char *Num,unsigned int Length)
{
   if (Str.length() != Length*2)
      return false;
   
   // Convert each digit. We store it in the same order as the string
   int J = 0;
   for (string::const_iterator I = Str.begin(); I != Str.end();J++, I += 2)
   {
      if (isxdigit(*I) == 0 || isxdigit(I[1]) == 0)
	 return false;
      
      Num[J] = HexDigit(I[0]) << 4;
      Num[J] += HexDigit(I[1]);
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
// RegexChoice - Simple regex list/list matcher				/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long RegexChoice(RxChoiceList *Rxs,const char **ListBegin,
		      const char **ListEnd)
{
   for (RxChoiceList *R = Rxs; R->Str != 0; R++)
      R->Hit = false;

   unsigned long Hits = 0;
   for (; ListBegin != ListEnd; ListBegin++)
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
// ioprintf - C format string outputter to C++ iostreams		/*{{{*/
// ---------------------------------------------------------------------
/* This is used to make the internationalization strings easier to translate
   and to allow reordering of parameters */
void ioprintf(ostream &out,const char *format,...) 
{
   va_list args;
   va_start(args,format);
   
   // sprintf the description
   char S[400];
   vsnprintf(S,sizeof(S),format,args);
   out << S;
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
   unsigned long Did;

   va_start(args,Format);

   if (End <= Buffer)
      return End;

   Did = vsnprintf(Buffer,End - Buffer,Format,args);
   if (Did < 0 || Buffer + Did > End)
      return End;
   return Buffer + Did;
}
									/*}}}*/

// CheckDomainList - See if Host is in a , seperate list		/*{{{*/
// ---------------------------------------------------------------------
/* The domain list is a comma seperate list of domains that are suffix
   matched against the argument */
bool CheckDomainList(const string &Host,const string &List)
{
   string::const_iterator Start = List.begin();
   for (string::const_iterator Cur = List.begin(); Cur <= List.end(); Cur++)
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

// URI::CopyFrom - Copy from an object					/*{{{*/
// ---------------------------------------------------------------------
/* This parses the URI into all of its components */
void URI::CopyFrom(const string &U)
{
   string::const_iterator I = U.begin();

   // Locate the first colon, this separates the scheme
   for (; I < U.end() && *I != ':' ; I++);
   string::const_iterator FirstColon = I;

   /* Determine if this is a host type URI with a leading double //
      and then search for the first single / */
   string::const_iterator SingleSlash = I;
   if (I + 3 < U.end() && I[1] == '/' && I[2] == '/')
      SingleSlash += 3;
   
   /* Find the / indicating the end of the hostname, ignoring /'s in the
      square brackets */
   bool InBracket = false;
   for (; SingleSlash < U.end() && (*SingleSlash != '/' || InBracket == true); SingleSlash++)
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
   for (; I < SingleSlash && *I != ':'; I++);
   string::const_iterator SecondColon = I;
   
   // Search for the @ after the colon
   for (; I < SingleSlash && *I != '@'; I++);
   string::const_iterator At = I;
   
   // Now write the host and user/pass
   if (At == SingleSlash)
   {
      if (FirstColon < SingleSlash)
	 Host.assign(FirstColon,SingleSlash);
   }
   else
   {
      Host.assign(At+1,SingleSlash);
      User.assign(FirstColon,SecondColon);
      if (SecondColon < At)
	 Password.assign(SecondColon+1,At);
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
   string Res;
   
   if (Access.empty() == false)
      Res = Access + ':';
   
   if (Host.empty() == false)
   {	 
      if (Access.empty() == false)
	 Res += "//";
          
      if (User.empty() == false)
      {
	 Res +=  User;
	 if (Password.empty() == false)
	    Res += ":" + Password;
	 Res += "@";
      }
      
      // Add RFC 2732 escaping characters
      if (Access.empty() == false &&
	  (Host.find('/') != string::npos || Host.find(':') != string::npos))
	 Res += '[' + Host + ']';
      else
	 Res += Host;
      
      if (Port != 0)
      {
	 char S[30];
	 sprintf(S,":%u",Port);
	 Res += S;
      }	 
   }
   
   if (Path.empty() == false)
   {
      if (Path[0] != '/')
	 Res += "/" + Path;
      else
	 Res += Path;
   }
   
   return Res;
}
									/*}}}*/
// URI::SiteOnly - Return the schema and site for the URI		/*{{{*/
// ---------------------------------------------------------------------
/* */
string URI::SiteOnly(const string &URI)
{
   ::URI U(URI);
   U.User.clear();
   U.Password.clear();
   U.Path.clear();
   U.Port = 0;
   return U;
}
									/*}}}*/
