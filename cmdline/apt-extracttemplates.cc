#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <wait.h>
#include <fstream.h>

#include <apt-pkg/init.h>
#if APT_PKG_MAJOR >= 3
#define APT_COMPATIBILITY 986
#include <apt-pkg/debversion.h>
#endif
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/version.h>
#include "debfile.h"
	
#define TMPDIR "/var/lib/debconf/"
#define STR(x) (x ? x : "")
//#define TMPDIR "tmp/"

void help(void)
{
	fprintf(stderr, "apt-extracttemplates deb [deb]\n");
	exit(0);
}

char *writefile(const char *prefix, const char *data)
{
	char fn[512];
	static int i;
	snprintf(fn, sizeof(fn), "%s%s.%u%d", TMPDIR, prefix, getpid(), i++);

	if (data == NULL) data = "";

	ofstream ofs(fn);
	if (!ofs) return NULL;
	ofs << data;
	ofs.close();
	return strdup(fn);
}

void writeconfig(const DebFile &file)
{
	char *templatefile = writefile("template", file.Template);
	char *configscript = writefile("config", file.Config);

	if (templatefile == 0 || configscript == 0)
	{
		fprintf(stderr, "Cannot write config script or templates\n");
		return;
	}
	printf("%s %s %s %s\n",
		STR(file.Package),	// Package
		STR(file.Version),	// Version
		templatefile,	// Template
		configscript 	// Config
	);
}

void init(MMap *&Map, pkgCache *&Cache)
{
	// Initialize the apt cache
	if (pkgInitConfig(*_config) == false || pkgInitSystem(*_config, _system) == false)
	{
		fprintf(stderr, "Cannot initialize apt cache\n");
		return;
	}
	pkgSourceList List;
	List.ReadMainList();
	OpProgress Prog;
	pkgMakeStatusCache(List,Prog,&Map,true);
	Cache = new pkgCache(Map);
}

int main(int argc, char **argv, char **env)
{
	int idx = 0;
	char **debs = 0;
	int numdebs = 0;
	MMap *Map = 0;
	const char *debconfver = NULL;

	init(Map, DebFile::Cache);
	if (Map == 0 || DebFile::Cache == 0)
	{
		fprintf(stderr, "Cannot initialize APT cache\n");
		return 1;
	}

	debconfver = DebFile::GetInstalledVer("debconf");
	
	if (debconfver == NULL) 
	{
		fprintf(stderr, "Cannot get debconf version. Is debconf installed?\n");
		return 1;
	}

	numdebs = argc - 1;
	debs = new char *[numdebs];
	memcpy(debs, &argv[1], sizeof(char *) * numdebs);

	if (numdebs < 1) 
	{
		fprintf(stderr, "apt-extracttemplates foo.deb [...]\n");
		return 0;
	}

	for (idx = 0; idx < numdebs; idx++)
	{
		DebFile file(debs[idx]);
		if (file.Go() == false) 
		{
			fprintf(stderr, "Cannot read %s\n", debs[idx]);
			continue;
		}
		if (file.Template != 0 && file.ParseInfo() == true)
		{
			if (file.DepVer != 0 && *file.DepVer != 0 &&
			    pkgCheckDep(file.DepVer, 
			                debconfver, file.DepOp) == false) 
				continue;
			if (file.PreDepVer != 0 && *file.PreDepVer != 0 &&
			    pkgCheckDep(file.PreDepVer, 
			                debconfver, file.PreDepOp) == false) 
				continue;

			writeconfig(file);
		}
	}
	

	delete Map;
	delete DebFile::Cache;

	return 0;
}
