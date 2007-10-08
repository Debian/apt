// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-extracttemplates.cc,v 1.15 2003/07/26 00:00:11 mdz Exp $
/* ######################################################################
   
   APT Extract Templates - Program to extract debconf config and template
                           files

   This is a simple program to extract config and template information 
   from Debian packages. It can be used to speed up the preconfiguration
   process for debconf-enabled packages
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/init.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/version.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/extracttar.h>
#include <apt-pkg/arfile.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
	
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>

#include <locale.h>
#include <config.h>
#include <apti18n.h>
#include "apt-extracttemplates.h"
									/*}}}*/

using namespace std;

#define TMPDIR		"/tmp"

pkgCache *DebFile::Cache = 0;

// DebFile::DebFile - Construct the DebFile object			/*{{{*/
// ---------------------------------------------------------------------
/* */
DebFile::DebFile(const char *debfile)
	: File(debfile, FileFd::ReadOnly), Control(0), DepOp(0), 
          PreDepOp(0), Config(0), Template(0), Which(None)
{
}
									/*}}}*/
// DebFile::~DebFile - Destruct the DebFile object			/*{{{*/
// ---------------------------------------------------------------------
/* */
DebFile::~DebFile()
{
	delete [] Control;
	delete [] Config;
	delete [] Template;
}
									/*}}}*/
// DebFile::GetInstalledVer - Find out the installed version of a pkg	/*{{{*/
// ---------------------------------------------------------------------
/* */
string DebFile::GetInstalledVer(const string &package)
{
	pkgCache::PkgIterator Pkg = Cache->FindPkg(package);
	if (Pkg.end() == false) 
	{
		pkgCache::VerIterator V = Pkg.CurrentVer();
		if (V.end() == false)
		{
			return V.VerStr();
		}
	}

	return string();
}
									/*}}}*/
// DebFile::Go - Start extracting a debian package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DebFile::Go()
{
	ARArchive AR(File);
	if (_error->PendingError() == true)
		return false;
		
	const ARArchive::Member *Member = AR.FindMember("control.tar.gz");
	if (Member == 0)
		return _error->Error(_("%s not a valid DEB package."),File.Name().c_str());
	
	if (File.Seek(Member->Start) == false)
		return false;
	ExtractTar Tar(File, Member->Size,"gzip");
	return Tar.Go(*this);
}
									/*}}}*/
// DebFile::DoItem examine element in package and mark			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DebFile::DoItem(Item &I, int &Fd)
{
	if (strcmp(I.Name, "control") == 0)
	{
		delete [] Control;
		Control = new char[I.Size+1];
		Control[I.Size] = 0;
		Which = IsControl;
		ControlLen = I.Size;
		// make it call the Process method below. this is so evil
		Fd = -2;
	}
	else if (strcmp(I.Name, "config") == 0)
	{
		delete [] Config;
		Config = new char[I.Size+1];
		Config[I.Size] = 0;
		Which = IsConfig;
		Fd = -2; 
	} 
	else if (strcmp(I.Name, "templates") == 0)
	{
		delete [] Template;
		Template = new char[I.Size+1];
		Template[I.Size] = 0;
		Which = IsTemplate;
		Fd = -2;
	} 
	else 
	{
		// Ignore it
		Fd = -1;
	}
	return true;
}
									/*}}}*/
// DebFile::Process examine element in package and copy			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DebFile::Process(Item &I, const unsigned char *data, 
		unsigned long size, unsigned long pos)
{
	switch (Which)
	{
	case IsControl:
		memcpy(Control + pos, data, size);
		break;
	case IsConfig:
		memcpy(Config + pos, data, size);
		break;
	case IsTemplate:
		memcpy(Template + pos, data, size);
		break;
	default: /* throw it away */ ;
	}
	return true;
}
									/*}}}*/
// DebFile::ParseInfo - Parse control file for dependency info		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DebFile::ParseInfo()
{
	if (Control == NULL) return false;
	
	pkgTagSection Section;
	Section.Scan(Control, ControlLen);

	Package = Section.FindS("Package");
	Version = GetInstalledVer(Package);

	const char *Start, *Stop;
	if (Section.Find("Depends", Start, Stop) == true)
	{
		while (1)
		{
			string P, V;
			unsigned int Op;
			Start = debListParser::ParseDepends(Start, Stop, P, V, Op);
			if (Start == 0) return false;
			if (P == "debconf")
			{
				DepVer = V;
				DepOp = Op;
				break;
			}
			if (Start == Stop) break;
		}
	}
	
	if (Section.Find("Pre-Depends", Start, Stop) == true)
	{
		while (1)
		{
			string P, V;
			unsigned int Op;
			Start = debListParser::ParseDepends(Start, Stop, P, V, Op);
			if (Start == 0) return false;
			if (P == "debconf")
			{
				PreDepVer = V;
				PreDepOp = Op;
				break;
			}
			if (Start == Stop) break;
		}
	}
	
	return true;
}
									/*}}}*/
// ShowHelp - show a short help text					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp(void)
{
   	ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);

	if (_config->FindB("version") == true) 
		return 0;

	cout << 
		_("Usage: apt-extracttemplates file1 [file2 ...]\n"
		"\n"
		"apt-extracttemplates is a tool to extract config and template info\n"
		"from debian packages\n"
		"\n"
		"Options:\n"
	        "  -h   This help text\n"
		"  -t   Set the temp dir\n"
		"  -c=? Read this configuration file\n"
		"  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n");
	return 0;
}
									/*}}}*/
// WriteFile - write the contents of the passed string to a file	/*{{{*/
// ---------------------------------------------------------------------
/* */
string WriteFile(const char *package, const char *prefix, const char *data)
{
	char fn[512];
	static int i;
	const char *tempdir = NULL;

        tempdir = getenv("TMPDIR");
        if (tempdir == NULL)
             tempdir = TMPDIR;

	snprintf(fn, sizeof(fn), "%s/%s.%s.%u%d",
                 _config->Find("APT::ExtractTemplates::TempDir", tempdir).c_str(),
                 package, prefix, getpid(), i++);
	FileFd f;
	if (data == NULL)
		data = "";

	if (!f.Open(fn, FileFd::WriteTemp, 0600))
	{
		_error->Errno("ofstream::ofstream",_("Unable to write to %s"),fn);
		return string();
	}

	f.Write(data, strlen(data));
	f.Close();
	return fn;
}
									/*}}}*/
// WriteConfig - write out the config data from a debian package file	/*{{{*/
// ---------------------------------------------------------------------
/* */
void WriteConfig(const DebFile &file)
{
	string templatefile = WriteFile(file.Package.c_str(), "template", file.Template);
	string configscript = WriteFile(file.Package.c_str(), "config", file.Config);

	if (templatefile.empty() == true || configscript.empty() == true)
		return;
	cout << file.Package << " " << file.Version << " " 
	     << templatefile << " " << configscript << endl;
}
									/*}}}*/
// InitCache - initialize the package cache				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Go(CommandLine &CmdL)
{	
	// Initialize the apt cache
	MMap *Map = 0;
	pkgSourceList List;
	List.ReadMainList();
	OpProgress Prog;
	pkgMakeStatusCache(List,Prog,&Map,true);
	if (Map == 0)
	   return false;
	DebFile::Cache = new pkgCache(Map);
	if (_error->PendingError() == true)
		return false;

	// Find out what version of debconf is currently installed
	string debconfver = DebFile::GetInstalledVer("debconf");
	if (debconfver.empty() == true)
		return _error->Error( _("Cannot get debconf version. Is debconf installed?"));

	// Process each package passsed in
	for (unsigned int I = 0; I != CmdL.FileSize(); I++)
	{
		// Will pick up the errors later..
		DebFile file(CmdL.FileList[I]);
		if (file.Go() == false)
		{
		        _error->Error("Prior errors apply to %s",CmdL.FileList[I]);
			continue;
		}

		// Does the package have templates?
		if (file.Template != 0 && file.ParseInfo() == true)
		{
			// Check to make sure debconf dependencies are
			// satisfied
			// cout << "Check " << file.DepVer << ',' << debconfver << endl;
			if (file.DepVer != "" &&
			    DebFile::Cache->VS->CheckDep(debconfver.c_str(),
					file.DepOp,file.DepVer.c_str()
							 ) == false)
				continue;
			if (file.PreDepVer != "" &&
			    DebFile::Cache->VS->CheckDep(debconfver.c_str(),
			                file.PreDepOp,file.PreDepVer.c_str()
							 ) == false) 
				continue;

			WriteConfig(file);
		}
	}
	

	delete Map;
	delete DebFile::Cache;
	
	return !_error->PendingError();
}
									/*}}}*/

int main(int argc, const char **argv)
{
	CommandLine::Args Args[] = {
		{'h',"help","help",0},
		{'v',"version","version",0},
		{'t',"tempdir","APT::ExtractTemplates::TempDir",CommandLine::HasArg},
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
	
	Go(CmdL);

	// Print any errors or warnings found during operation
	if (_error->empty() == false)
	{
		// This goes to stderr..
		bool Errors = _error->PendingError();
		_error->DumpErrors();
		return Errors == true?100:0;
	}
	
	return 0;
}
