// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cdrom.cc,v 1.45 2003/11/19 23:50:51 mdz Exp $
/* ######################################################################
   
   APT CDROM - Tool for handling APT's CDROM database.
   
   Currently the only option is 'add' which will take the current CD
   in the drive and add it into the database.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/cdrom.h>
#include <config.h>
#include <apti18n.h>
    
//#include "indexcopy.h"

#include <locale.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
									/*}}}*/

using namespace std;

                                                                        /*{{{*/
class pkgCdromTextStatus : public pkgCdromStatus
{
protected:
   OpTextProgress Progress;
   void Prompt(const char *Text); 
   string PromptLine(const char *Text);
   bool AskCdromName(string &name);

public:
   virtual void Update(string text, int current);
   virtual bool ChangeCdrom();
   virtual OpProgress* GetOpProgress();
};

void pkgCdromTextStatus::Prompt(const char *Text) 
{
   char C;
   cout << Text << ' ' << flush;
   read(STDIN_FILENO,&C,1);
   if (C != '\n')
      cout << endl;
}

string pkgCdromTextStatus::PromptLine(const char *Text)
{
   cout << Text << ':' << endl;
   
   string Res;
   getline(cin,Res);
   return Res;
}

bool pkgCdromTextStatus::AskCdromName(string &name) 
{
   cout << _("Please provide a name for this Disc, such as 'Debian 2.1r1 Disk 1'") << flush;
   name = PromptLine("");
	 
   return true;
}
   

void pkgCdromTextStatus::Update(string text, int current) 
{
   if(text.size() > 0)
      cout << text << flush;
}

bool pkgCdromTextStatus::ChangeCdrom() 
{
   Prompt(_("Please insert a Disc in the drive and press enter"));
   return true;
}

OpProgress* pkgCdromTextStatus::GetOpProgress() 
{ 
   return &Progress; 
};

									/*}}}*/

// DoAdd - Add a new CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* This does the main add bit.. We show some status and things. The
   sequence is to mount/umount the CD, Ident it then scan it for package 
   files and reduce that list. Then we copy over the package files and
   verify them. Then rewrite the database files */
bool DoAdd(CommandLine &)
{
   bool res = false;
   pkgCdromTextStatus log;
   pkgCdrom cdrom;
   res = cdrom.Add(&log);
   if(res)
      cout << _("Repeat this process for the rest of the CDs in your set.") << endl;
   return res;
}
									/*}}}*/
// DoIdent - Ident a CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoIdent(CommandLine &)
{
   string ident;
   pkgCdromTextStatus log;
   pkgCdrom cdrom;
   return cdrom.Ident(ident, &log);
}
									/*}}}*/

// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return 0;
   
   cout << 
      "Usage: apt-cdrom [options] command\n"
      "\n"
      "apt-cdrom is a tool to add CDROM's to APT's source list. The\n"
      "CDROM mount point and device information is taken from apt.conf\n"
      "and /etc/fstab.\n"
      "\n"
      "Commands:\n"
      "   add - Add a CDROM\n"
      "   ident - Report the identity of a CDROM\n"
      "\n"
      "Options:\n"
      "  -h   This help text\n"
      "  -d   CD-ROM mount point\n"
      "  -r   Rename a recognized CD-ROM\n"
      "  -m   No mounting\n"
      "  -f   Fast mode, don't check package files\n"
      "  -a   Thorough scan mode\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See fstab(5)\n";
   return 0;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'d',"cdrom","Acquire::cdrom::mount",CommandLine::HasArg},
      {'r',"rename","APT::CDROM::Rename",0},
      {'m',"no-mount","APT::CDROM::NoMount",0},
      {'f',"fast","APT::CDROM::Fast",0},
      {'n',"just-print","APT::CDROM::NoAct",0},
      {'n',"recon","APT::CDROM::NoAct",0},      
      {'n',"no-act","APT::CDROM::NoAct",0},
      {'a',"thorough","APT::CDROM::Thorough",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {
      {"add",&DoAdd},
      {"ident",&DoIdent},
      {0,0}};

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
   if (_config->FindB("help") == true || _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Deal with stdout not being a tty
   if (isatty(STDOUT_FILENO) && _config->FindI("quiet",0) < 1)
      _config->Set("quiet","1");
   
   // Match the operation
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;
}
