// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-extracttemplates.cc,v 1.3 2001/02/25 05:25:29 tausq Exp $
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
#include <apt-pkg/debversion.h>
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
	
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <fstream.h>

#include <config.h>
#include <apti18n.h>
#include "apt-extracttemplates.h"

#define TMPDIR		"/var/lib/debconf/"

pkgCache *DebFile::Cache = 0;

// DebFile::DebFile - Construct the DebFile object			/*{{{*/
// ---------------------------------------------------------------------
/* */
DebFile::DebFile(const char *debfile)
	: File(debfile, FileFd::ReadOnly), Control(0), DepOp(0), PreDepOp(0), 
	  Config(0), Template(0), Which(None)
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
char *DebFile::GetInstalledVer(const string &package)
{
	char *ver = 0;

	pkgCache::PkgIterator Pkg = Cache->FindPkg(package);
	if (Pkg.end() == false) 
	{
		pkgCache::VerIterator V = Pkg.CurrentVer();
		if (V.end() == false) 
		{
			ver = strdup(V.VerStr());
		}
	}

	return ver;
}
									/*}}}*/
// DebFile::Go - Start extracting a debian package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DebFile::Go()
{
	ARArchive AR(File);
	const ARArchive::Member *Member = AR.FindMember("control.tar.gz");
	if (Member == 0)
	{
		fprintf(stderr, _("This is not a valid DEB package.\n"));
		return false;
	}
	
	if (File.Seek(Member->Start) == false)
	{
		return false;
	}

	ExtractTar Tar(File, Member->Size);
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
void ShowHelp(void)
{
   	ioprintf(cout,_("%s %s for %s %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_OS,COMMON_CPU,__DATE__,__TIME__);

	if (_config->FindB("version") == true) return;

	fprintf(stderr, 
		_("Usage: apt-extracttemplates file1 [file2 ...]\n"
		"\n"
		"apt-extracttemplates is a tool to extract config and template info\n"
		"from debian packages\n"));
	exit(0);
}
									/*}}}*/
// WriteFile - write the contents of the passed string to a file	/*{{{*/
// ---------------------------------------------------------------------
/* */
char *WriteFile(const char *prefix, const char *data)
{
	char fn[512];
	static int i;
	snprintf(fn, sizeof(fn), "%s%s.%u%d", _config->Find("APT::ExtractTemplates::TempDir", TMPDIR).c_str(), prefix, getpid(), i++);

	ofstream ofs(fn);
	if (!ofs) return NULL;
	ofs << (data ? data : "");
	ofs.close();
	return strdup(fn);
}
									/*}}}*/
// WriteConfig - write out the config data from a debian package file	/*{{{*/
// ---------------------------------------------------------------------
/* */
void WriteConfig(const DebFile &file)
{
	char *templatefile = WriteFile("template", file.Template);
	char *configscript = WriteFile("config", file.Config);

	if (templatefile == 0 || configscript == 0)
	{
		fprintf(stderr, _("Cannot write config script or templates\n"));
		return;
	}
	cout << file.Package << " " << file.Version << " " 
	     << templatefile << " " << configscript << endl;

	free(templatefile);
	free(configscript);
}
									/*}}}*/
// InitCache - initialize the package cache				/*{{{*/
// ---------------------------------------------------------------------
/* */
int InitCache(MMap *&Map, pkgCache *&Cache)
{
	// Initialize the apt cache
	if (pkgInitConfig(*_config) == false || pkgInitSystem(*_config, _system) == false)
	{
      		_error->DumpErrors();
		return -1;
	}
	pkgSourceList List;
	List.ReadMainList();
	OpProgress Prog;
	pkgMakeStatusCache(List,Prog,&Map,true);
	Cache = new pkgCache(Map);
	return 0;
}
									/*}}}*/

int main(int argc, const char **argv)
{
	MMap *Map = 0;
	const char *debconfver = NULL;

	CommandLine::Args Args[] = {
		{'h',"help","help",0},
		{'v',"version","version",0},
		{'t',"tempdir","APT::ExtractTemplates::TempDir",CommandLine::HasArg},
		{'c',"config-file",0,CommandLine::ConfigFile},
		{'o',"option",0,CommandLine::ArbItem},
		{0,0,0,0}};
	
	// Initialize the package cache
	if (InitCache(Map, DebFile::Cache) < 0 || Map == 0 || DebFile::Cache == 0)
	{
		fprintf(stderr, _("Cannot initialize APT cache\n"));
		return 100;
	}

	// Parse the command line
	CommandLine CmdL(Args,_config);
	if (CmdL.Parse(argc,argv) == false)
	{
		fprintf(stderr, _("Cannot parse commandline options\n"));
		return 100;
	}

	// See if the help should be shown
	if (_config->FindB("help") == true || CmdL.FileSize() == 0)
	{
		ShowHelp();
		return 0;
	}

	// Find out what version of debconf is currently installed
	if ((debconfver = DebFile::GetInstalledVer("debconf")) == NULL)
	{
		fprintf(stderr, _("Cannot get debconf version. Is debconf installed?\n"));
		return 1;
	}

	// Process each package passsed in
	for (unsigned int I = 0; I != CmdL.FileSize(); I++)
	{
		DebFile file(CmdL.FileList[I]);
		if (file.Go() == false) 
		{
			fprintf(stderr, _("Cannot read %s\n"), CmdL.FileList[I]);
			continue;
		}
		// Does the package have templates?
		if (file.Template != 0 && file.ParseInfo() == true)
		{
			// Check to make sure debconf dependencies are
			// satisfied
			if (file.DepVer != "" &&
			    DebFile::Cache->VS->CheckDep(file.DepVer.c_str(), 
			                file.DepOp, debconfver) == false) 
				continue;
			if (file.PreDepVer != "" &&
			    DebFile::Cache->VS->CheckDep(file.PreDepVer.c_str(), 
			                file.PreDepOp, debconfver) == false) 
				continue;

			WriteConfig(file);
		}
	}
	

	delete Map;
	delete DebFile::Cache;

	return 0;
}
