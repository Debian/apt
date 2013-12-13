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
#include<config.h>

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
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgsystem.h>

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

#include <apt-private/private-cmndline.h>

#include <apti18n.h>
									/*}}}*/
static const char *W_NO_CDROM_FOUND = \
   N_("No CD-ROM could be auto-detected or found using "
      "the default mount point.\n"
      "You may try the --cdrom option to set the CD-ROM mount point. "
      "See 'man apt-cdrom' for more "
      "information about the CD-ROM auto-detection and mount point.");

using namespace std;

class pkgCdromTextStatus : public pkgCdromStatus			/*{{{*/
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
   if (read(STDIN_FILENO,&C,1) < 0)
      _error->Errno("pkgCdromTextStatus::Prompt", 
                    "Failed to read from standard input (not a terminal?)");
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
   cout << _("Please provide a name for this Disc, such as 'Debian 5.0.3 Disk 1'") << flush;
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
// SetupAutoDetect       						/*{{{*/
bool AutoDetectCdrom(pkgUdevCdromDevices &UdevCdroms, unsigned int &i, bool &automounted)
{
   bool Debug =  _config->FindB("Debug::Acquire::cdrom", false);

   automounted = false;

   vector<struct CdromDevice> v = UdevCdroms.Scan();
   if (i >= v.size())
      return false;

   if (Debug)
      clog << "Looking at devce " << i
	   << " DeviveName: " << v[i].DeviceName 
	   << " IsMounted: '" << v[i].Mounted << "'"
	   << " MountPoint: '" << v[i].MountPath << "'"
	   << endl;

   if (v[i].Mounted)
   {
      // set the right options
      _config->Set("Acquire::cdrom::mount", v[i].MountPath);
      _config->Set("APT::CDROM::NoMount", true);
   } else {
      string AptMountPoint = _config->FindDir("Dir::Media::MountPath");
      if (!FileExists(AptMountPoint))
	 mkdir(AptMountPoint.c_str(), 0750);
      if(MountCdrom(AptMountPoint, v[i].DeviceName) == false)
	 _error->Warning(_("Failed to mount '%s' to '%s'"), v[i].DeviceName.c_str(), AptMountPoint.c_str());
      else
	 automounted = true;
      _config->Set("Acquire::cdrom::mount", AptMountPoint);
      _config->Set("APT::CDROM::NoMount", true);
   }
   i++;

   return true;
}
									/*}}}*/
// DoAdd - Add a new CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* This does the main add bit.. We show some status and things. The
   sequence is to mount/umount the CD, Ident it then scan it for package 
   files and reduce that list. Then we copy over the package files and
   verify them. Then rewrite the database files */
bool DoAdd(CommandLine &)
{
   pkgUdevCdromDevices UdevCdroms;
   pkgCdromTextStatus log;
   pkgCdrom cdrom;
   bool res = true;

   bool AutoDetect = _config->FindB("Acquire::cdrom::AutoDetect", true);
   unsigned int count = 0;
   string AptMountPoint = _config->FindDir("Dir::Media::MountPath");
   bool automounted = false;
   if (AutoDetect && UdevCdroms.Dlopen())
      while (AutoDetectCdrom(UdevCdroms, count, automounted)) {
	 if (count == 1) {
	    // begin loop with res false to detect any success using OR
	    res = false;
	 }

	 // dump any warnings/errors from autodetect
	 if (_error->empty() == false)
	    _error->DumpErrors();

	 res |= cdrom.Add(&log);

	 if (automounted)
	    UnmountCdrom(AptMountPoint);

	 // dump any warnings/errors from add/unmount
	 if (_error->empty() == false)
	    _error->DumpErrors();
      }

   if (count == 0)
      res = cdrom.Add(&log);

   if (res == false)
      _error->Error("%s", _(W_NO_CDROM_FOUND));
   else
      cout << _("Repeat this process for the rest of the CDs in your set.") << endl;

   return res;
}
									/*}}}*/
// DoIdent - Ident a CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoIdent(CommandLine &)
{
   pkgUdevCdromDevices UdevCdroms;
   string ident;
   pkgCdromTextStatus log;
   pkgCdrom cdrom;
   bool res = true;

   bool AutoDetect = _config->FindB("Acquire::cdrom::AutoDetect", true);

   unsigned int count = 0;
   string AptMountPoint = _config->FindDir("Dir::Media::MountPath");
   bool automounted = false;
   if (AutoDetect && UdevCdroms.Dlopen())
      while (AutoDetectCdrom(UdevCdroms, count, automounted)) {
	 if (count == 1) {
	    // begin loop with res false to detect any success using OR
	    res = false;
	 }

	 // dump any warnings/errors from autodetect
	 if (_error->empty() == false)
	    _error->DumpErrors();

	 res |= cdrom.Ident(ident, &log);

	 if (automounted)
	    UnmountCdrom(AptMountPoint);

	 // dump any warnings/errors from add/unmount
	 if (_error->empty() == false)
	    _error->DumpErrors();
      }

   if (count == 0)
      res = cdrom.Ident(ident, &log);

   if (res == false)
      _error->Error("%s", _(W_NO_CDROM_FOUND));

   return res;
}
									/*}}}*/
// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp(CommandLine &)
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return true;
   
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
      "  --no-auto-detect Do not try to auto detect drive and mount point\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See fstab(5)\n";
   return true;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {
      {"add",&DoAdd},
      {"ident",&DoIdent},
      {"help",&ShowHelp},
      {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs("apt-cdrom", CommandLine::GetCommand(Cmds, argc, argv));

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(),_config);
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
      return ShowHelp(CmdL);

   // Deal with stdout not being a tty
   if (isatty(STDOUT_FILENO) && _config->FindI("quiet", -1) == -1)
      _config->Set("quiet","1");
   
   // Match the operation
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   return Errors == true ? 100 : 0;
}
									/*}}}*/
