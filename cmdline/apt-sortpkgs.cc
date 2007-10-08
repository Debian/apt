// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-sortpkgs.cc,v 1.5 2003/01/11 07:18:44 jgg Exp $
/* ######################################################################
   
   APT Sort Packages - Program to sort Package and Source files

   This program is quite simple, it just sorts the package files by
   package and sorts the fields inside by the internal APT sort order.
   Input is taken from a named file and sent to stdout.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/tagfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>

#include <config.h>
#include <apti18n.h>
    
#include <vector>
#include <algorithm>

#include <locale.h>
#include <unistd.h>
									/*}}}*/

using namespace std;

struct PkgName
{
   string Name;
   string Ver;
   string Arch;
   unsigned long Offset;
   unsigned long Length;
   
   inline int Compare3(const PkgName &x) const
   {
      int A = stringcasecmp(Name,x.Name);
      if (A == 0)
      {
	 A = stringcasecmp(Ver,x.Ver);
	 if (A == 0)
	    A = stringcasecmp(Arch,x.Arch);
      }
      return A;
   }
   
   bool operator <(const PkgName &x) const {return Compare3(x) < 0;};
   bool operator >(const PkgName &x) const {return Compare3(x) > 0;};
   bool operator ==(const PkgName &x) const {return Compare3(x) == 0;};
};

// DoIt - Sort a single file						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoIt(string InFile)
{
   FileFd Fd(InFile,FileFd::ReadOnly);
   pkgTagFile Tags(&Fd);
   if (_error->PendingError() == true)
      return false;
   
   // Parse.
   vector<PkgName> List;
   pkgTagSection Section;
   unsigned long Largest = 0;
   unsigned long Offset = Tags.Offset();
   bool Source = _config->FindB("APT::SortPkgs::Source",false);
   while (Tags.Step(Section) == true)
   {
      PkgName Tmp;
      
      /* Fetch the name, auto-detecting if this is a source file or a 
         package file */
      Tmp.Name = Section.FindS("Package");
      Tmp.Ver = Section.FindS("Version");
      Tmp.Arch = Section.FindS("Architecture");
      
      if (Tmp.Name.empty() == true)
	 return _error->Error(_("Unknown package record!"));
      
      Tmp.Offset = Offset;
      Tmp.Length = Section.size();
      if (Largest < Tmp.Length)
	 Largest = Tmp.Length;
      
      List.push_back(Tmp);
      
      Offset = Tags.Offset();
   }
   if (_error->PendingError() == true)
      return false;
   
   // Sort it
   sort(List.begin(),List.end());

   const char **Order = TFRewritePackageOrder;
   if (Source == true)
      Order = TFRewriteSourceOrder;
   
   // Emit
   unsigned char *Buffer = new unsigned char[Largest+1];
   for (vector<PkgName>::iterator I = List.begin(); I != List.end(); I++)
   {
      // Read in the Record.
      if (Fd.Seek(I->Offset) == false || Fd.Read(Buffer,I->Length) == false)
      {
	 delete [] Buffer;
	 return false;
      }
      
      Buffer[I->Length] = '\n';      
      if (Section.Scan((char *)Buffer,I->Length+1) == false)
      {
	 delete [] Buffer;
	 return _error->Error("Internal error, failed to scan buffer");
      }

      // Sort the section
      if (TFRewrite(stdout,Section,Order,0) == false)
      {
	 delete [] Buffer;
	 return _error->Error("Internal error, failed to sort fields");
      }
      
      fputc('\n',stdout);      
   }
   
   delete [] Buffer;
   return true;
}
									/*}}}*/
// ShowHelp - Show the help text					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return 0;
   
   cout <<
    _("Usage: apt-sortpkgs [options] file1 [file2 ...]\n"
      "\n"
      "apt-sortpkgs is a simple tool to sort package files. The -s option is used\n"
      "to indicate what kind of file it is.\n"
      "\n"
      "Options:\n"
      "  -h   This help text\n"
      "  -s   Use source file sorting\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n");

   return 0;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'s',"source","APT::SortPkgs::Source",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Match the operation
   for (unsigned int I = 0; I != CmdL.FileSize(); I++)
      if (DoIt(CmdL.FileList[I]) == false)
	 break;
   
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;   
}
