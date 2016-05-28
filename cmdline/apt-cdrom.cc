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
#include <apt-pkg/cdrom.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgsystem.h>

#include <iostream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-output.h>
#include <apt-private/private-main.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

class pkgCdromTextStatus : public pkgCdromStatus			/*{{{*/
{
protected:
   OpTextProgress Progress;
   void Prompt(const char *Text);
   string PromptLine(const char *Text);
   bool AskCdromName(string &name) APT_OVERRIDE;

public:
   virtual void Update(string text, int current) APT_OVERRIDE;
   virtual bool ChangeCdrom() APT_OVERRIDE;
   virtual OpProgress* GetOpProgress() APT_OVERRIDE;
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


void pkgCdromTextStatus::Update(string text, int /*current*/)
{
   if(text.size() > 0)
      cout << text << flush;
}

bool pkgCdromTextStatus::ChangeCdrom()
{
   Prompt(_("Please insert a Disc in the drive and press [Enter]"));
   return true;
}

APT_CONST OpProgress* pkgCdromTextStatus::GetOpProgress()
{
   return &Progress;
}
									/*}}}*/
// AddOrIdent - Add or Ident a CDROM					/*{{{*/
static bool AddOrIdent(bool const Add)
{
   pkgUdevCdromDevices UdevCdroms;
   pkgCdromTextStatus log;
   pkgCdrom cdrom;

   bool oneSuccessful = false;
   bool AutoDetect = _config->FindB("Acquire::cdrom::AutoDetect", true);
   if (AutoDetect == true && UdevCdroms.Dlopen() == true)
   {
      bool const Debug = _config->FindB("Debug::Acquire::cdrom", false);
      std::string const CDMount = _config->Find("Acquire::cdrom::mount");
      bool const NoMount = _config->FindB("APT::CDROM::NoMount", false);
      if (NoMount == false)
	 _config->Set("APT::CDROM::NoMount", true);

      vector<struct CdromDevice> const v = UdevCdroms.Scan();
      for (std::vector<struct CdromDevice>::const_iterator cd = v.begin(); cd != v.end(); ++cd)
      {
	 if (Debug)
	    clog << "Looking at device:"
	       << "\tDeviveName: '" << cd->DeviceName << "'"
	       << "\tIsMounted: '" << cd->Mounted << "'"
	       << "\tMountPoint: '" << cd->MountPath << "'"
	       << endl;

	 std::string AptMountPoint;
	 if (cd->Mounted)
	    _config->Set("Acquire::cdrom::mount", cd->MountPath);
	 else if (NoMount == true)
	    continue;
	 else
	 {
	    AptMountPoint = _config->FindDir("Dir::Media::MountPath");
	    if (FileExists(AptMountPoint) == false)
	       mkdir(AptMountPoint.c_str(), 0750);
	    if(MountCdrom(AptMountPoint, cd->DeviceName) == false)
	    {
	       _error->Warning(_("Failed to mount '%s' to '%s'"), cd->DeviceName.c_str(), AptMountPoint.c_str());
	       continue;
	    }
	    _config->Set("Acquire::cdrom::mount", AptMountPoint);
	 }

	 _error->PushToStack();
	 if (Add == true)
	    oneSuccessful = cdrom.Add(&log);
	 else
	 {
	    std::string id;
	    oneSuccessful = cdrom.Ident(id, &log);
	 }
	 _error->MergeWithStack();

	 if (AptMountPoint.empty() == false)
	    UnmountCdrom(AptMountPoint);
      }
      if (NoMount == false)
	 _config->Set("APT::CDROM::NoMount", NoMount);
      _config->Set("Acquire::cdrom::mount", CDMount);
   }

   // fallback if auto-detect didn't work
   if (oneSuccessful == false)
   {
      _error->PushToStack();
      if (Add == true)
	 oneSuccessful = cdrom.Add(&log);
      else
      {
	 std::string id;
	 oneSuccessful = cdrom.Ident(id, &log);
      }
      _error->MergeWithStack();
   }

   if (oneSuccessful == false)
      _error->Error("%s", _("No CD-ROM could be auto-detected or found using the default mount point.\n"
      "You may try the --cdrom option to set the CD-ROM mount point.\n"
      "See 'man apt-cdrom' for more information about the CD-ROM auto-detection and mount point."));
   else if (Add == true)
      cout << _("Repeat this process for the rest of the CDs in your set.") << endl;

   return oneSuccessful;
}
									/*}}}*/
// DoAdd - Add a new CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* This does the main add bit.. We show some status and things. The
   sequence is to mount/umount the CD, Ident it then scan it for package
   files and reduce that list. Then we copy over the package files and
   verify them. Then rewrite the database files */
static bool DoAdd(CommandLine &)
{
   return AddOrIdent(true);
}
									/*}}}*/
// DoIdent - Ident a CDROM						/*{{{*/
static bool DoIdent(CommandLine &)
{
   return AddOrIdent(false);
}
									/*}}}*/
static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
      _("Usage: apt-cdrom [options] command\n"
      "\n"
      "apt-cdrom is used to add CDROM's, USB flashdrives and other removable\n"
      "media types as package sources to APT. The mount point and device\n"
      "information is taken from apt.conf(5), udev(7) and fstab(5).\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {"add", &DoAdd, "Add a CDROM"},
      {"ident", &DoIdent, "Report the identity of a CDROM"},
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   // Parse the command line and initialize the package library
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_CDROM, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);

   InitOutput();

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/
