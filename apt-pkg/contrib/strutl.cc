// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: strutl.cc,v 1.4 1998/09/22 05:30:28 jgg Exp $
/* ######################################################################

   String Util - Some usefull string functions.

   strstrip - Remove whitespace from the front and end of a line.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@gpu.srv.ualberta.ca>   
   
   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#include <strutl.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
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
   and striped out as well. This is for URI/URL parsing. */
bool ParseQuoteWord(const char *&String,string &Res)
{
   // Skip leading whitespace
   const char *C = String;
   for (;*C != 0 && *C == ' '; C++);
   if (*C == 0)
      return false;
   
   // Jump to the next word
   for (;*C != 0 && *C != ' '; C++)
   {
      if (*C == '"')
      {
	 for (C++;*C != 0 && *C != '"'; C++);
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
	 Tmp[3] = 0;
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
   for (;*C != 0 && *C == ' '; C++);
   String = C;
   return true;
}
									/*}}}*/
// ParseCWord - Parses a string like a C "" expression			/*{{{*/
// ---------------------------------------------------------------------
/* This expects a series of space seperated strings enclosed in ""'s. 
   It concatenates the ""'s into a single string. */
bool ParseCWord(const char *String,string &Res)
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
   return true;
}
									/*}}}*/
// QuoteString - Convert a string into quoted from			/*{{{*/
// ---------------------------------------------------------------------
/* */
string QuoteString(string Str,const char *Bad)
{
   string Res;
   for (string::iterator I = Str.begin(); I != Str.end(); I++)
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
// SizeToStr - Convert a long into a human readable size		/*{{{*/
// ---------------------------------------------------------------------
/* A max of 4 digits are shown before conversion to the next highest unit. The
   max length of the string will be 5 chars unless the size is > 10 
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
   char Ext[] = {'b','k','M','G','T','P','E','Z','Y'};
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
	 sprintf(S,"%lid %lih%lim%lis",Sec/60/60/24,(Sec/60/60) % 24,(Sec/60) % 60,Sec % 60);
	 break;
      }
      
      if (Sec > 60*60)
      {
	 sprintf(S,"%lih%lim%lis",Sec/60/60,(Sec/60) % 60,Sec % 60);
	 break;
      }
      
      if (Sec > 60)
      {
	 sprintf(S,"%lim%lis",Sec/60,Sec % 60);
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
string SubstVar(string Str,string Subst,string Contents)
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
									/*}}}*/
// URItoFileName - Convert the uri into a unique file name		/*{{{*/
// ---------------------------------------------------------------------
/* This converts a URI into a safe filename. It quotes all unsafe characters
   and converts / to _ and removes the scheme identifier. The resulting
   file name should be unique and never occur again for a different file */
string URItoFileName(string URI)
{
   string::const_iterator I = URI.begin() + URI.find(':') + 1;
   for (; I < URI.end() && *I == '/'; I++);

   // "\x00-\x20{}|\\\\^\\[\\]<>\"\x7F-\xFF";
   URI = QuoteString(string(I,URI.end() - I),"\\|{}[]<>\"^~_=!@#$%^&*");
   string::iterator J = URI.begin();
   for (; J != URI.end(); J++)
      if (*J == '/') 
	 *J = '_';
   return URI;
}
									/*}}}*/
// Base64Encode - Base64 Encoding routine for short strings		/*{{{*/
// ---------------------------------------------------------------------
/* This routine performs a base64 transformation on a string. It was ripped
   from wget and then patched and bug fixed.
 
   This spec can be found in rfc2045 */
string Base64Encode(string S)
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
// stringcmp - Arbitary string compare					/*{{{*/
// ---------------------------------------------------------------------
/* This safely compares two non-null terminated strings of arbitary 
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
									/*}}}*/
// stringcasecmp - Arbitary case insensitive string compare		/*{{{*/
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
									/*}}}*/
