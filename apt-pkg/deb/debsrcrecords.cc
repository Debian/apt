// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.cc,v 1.1 1999/04/04 01:17:29 jgg Exp $
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/debsrcrecords.h"
#endif 

#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
									/*}}}*/

// SrcRecordParser::Binaries - Return the binaries field		/*{{{*/
// ---------------------------------------------------------------------
/* This member parses the binaries field into a pair of class arrays and
   returns a list of strings representing all of the components of the
   binaries field. The returned array need not be freed and will be
   reused by the next Binaries function call. */
const char **debSrcRecordParser::Binaries()
{
   string Bins = Sect.FindS("Binary");
   char *Buf = Buffer;
   unsigned int Bin = 0;
   if (Bins.empty() == true)
      return 0;
   
   // Strip any leading spaces
   string::const_iterator Start = Bins.begin();
   for (; Start != Bins.end() && isspace(*Start) != 0; Start++);

   string::const_iterator Pos = Start;
   while (Pos != Bins.end())
   {
      // Skip to the next ','
      for (; Pos != Bins.end() && *Pos != ','; Pos++);
      
      // Back remove spaces
      string::const_iterator End = Pos;
      for (; End > Start && (End[-1] == ',' || isspace(End[-1]) != 0); End--);
      
      // Stash the string
      memcpy(Buf,Start,End-Start);
      StaticBinList[Bin] = Buf;
      Bin++;
      Buf += End-Start;
      *Buf++ = 0;
      
      // Advance pos
      for (; Pos != Bins.end() && (*Pos == ',' || isspace(*Pos) != 0); Pos++);
      Start = Pos;
   }
   
   StaticBinList[Bin] = 0;
   return StaticBinList;
}
									/*}}}*/
