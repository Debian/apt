// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.cc,v 1.2 1999/04/04 08:07:39 jgg Exp $
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
#include <apt-pkg/strutl.h>
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
// SrcRecordParser::Files - Return a list of files for this source	/*{{{*/
// ---------------------------------------------------------------------
/* This parses the list of files and returns it, each file is required to have
   a complete source package */
bool debSrcRecordParser::Files(vector<pkgSrcRecords::File> &List)
{
   List.erase(List.begin(),List.end());
   
   string Files = Sect.FindS("Files");
   if (Files.empty() == true)
      return false;

   // Stash the / terminated directory prefix
   string Base = Sect.FindS("Directory:");
   if (Base.empty() == false && Base[Base.length()-1] != '/')
      Base += '/';
       
   // Iterate over the entire list grabbing each triplet
   const char *C = Files.c_str();
   while (*C != 0)
   {   
      pkgSrcRecords::File F;
      string Size;
      
      // Parse each of the elements
      if (ParseQuoteWord(C,F.MD5Hash) == false ||
	  ParseQuoteWord(C,Size) == false ||
	  ParseQuoteWord(C,F.Path) == false)
	 return _error->Error("Error parsing file record");
      
      // Parse the size and append the directory
      F.Size = atoi(Size.c_str());
      F.Path = Base + F.Path;
      List.push_back(F);
   }
   
   return true;
}
									/*}}}*/
