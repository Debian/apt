// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.cc,v 1.6 2004/03/17 05:58:54 mdz Exp $
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
      
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>

using std::max;
									/*}}}*/

// SrcRecordParser::Binaries - Return the binaries field		/*{{{*/
// ---------------------------------------------------------------------
/* This member parses the binaries field into a pair of class arrays and
   returns a list of strings representing all of the components of the
   binaries field. The returned array need not be freed and will be
   reused by the next Binaries function call. This function is commonly
   used during scanning to find the right package */
const char **debSrcRecordParser::Binaries()
{
   // This should use Start/Stop too, it is supposed to be efficient after all.
   string Bins = Sect.FindS("Binary");
   if (Bins.empty() == true || Bins.length() >= 102400)
      return 0;
   
   if (Bins.length() >= BufSize)
   {
      delete [] Buffer;
      // allocate new size based on buffer (but never smaller than 4000)
      BufSize = max((unsigned int)4000, max((unsigned int)Bins.length()+1,2*BufSize));
      Buffer = new char[BufSize];
   }

   strcpy(Buffer,Bins.c_str());
   if (TokSplitString(',',Buffer,StaticBinList,
		      sizeof(StaticBinList)/sizeof(StaticBinList[0])) == false)
      return 0;

   return (const char **)StaticBinList;
}
									/*}}}*/
// SrcRecordParser::BuildDepends - Return the Build-Depends information	/*{{{*/
// ---------------------------------------------------------------------
/* This member parses the build-depends information and returns a list of 
   package/version records representing the build dependency. The returned 
   array need not be freed and will be reused by the next call to this 
   function */
bool debSrcRecordParser::BuildDepends(vector<pkgSrcRecords::Parser::BuildDepRec> &BuildDeps, bool ArchOnly)
{
   unsigned int I;
   const char *Start, *Stop;
   BuildDepRec rec;
   const char *fields[] = {"Build-Depends", 
                           "Build-Depends-Indep",
			   "Build-Conflicts",
			   "Build-Conflicts-Indep"};

   BuildDeps.clear();

   for (I = 0; I < 4; I++) 
   {
      if (ArchOnly && (I == 1 || I == 3))
         continue;

      if (Sect.Find(fields[I], Start, Stop) == false)
         continue;
      
      while (1)
      {
         Start = debListParser::ParseDepends(Start, Stop, 
		     rec.Package,rec.Version,rec.Op,true);
	 
         if (Start == 0) 
            return _error->Error("Problem parsing dependency: %s", fields[I]);
	 rec.Type = I;

	 if (rec.Package != "")
   	    BuildDeps.push_back(rec);
	 
   	 if (Start == Stop) 
	    break;
      }	 
   }
   
   return true;
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
   string Base = Sect.FindS("Directory");
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
      
      // Try to guess what sort of file it is we are getting.
      string::size_type Pos = F.Path.length()-1;
      while (1)
      {
	 string::size_type Tmp = F.Path.rfind('.',Pos);
	 if (Tmp == string::npos)
	    break;
	 F.Type = string(F.Path,Tmp+1,Pos-Tmp);
	 
	 if (F.Type == "gz" || F.Type == "bz2" || F.Type == "lzma")
	 {
	    Pos = Tmp-1;
	    continue;
	 }
	 
	 break;
      }
      
      List.push_back(F);
   }
   
   return true;
}
									/*}}}*/
