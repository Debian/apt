// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-extracttemplates.h,v 1.2 2001/02/27 04:26:03 jgg Exp $
/* ######################################################################

   apt-extracttemplate - tool to extract template and config data
   
   ##################################################################### */
									/*}}}*/
#ifndef _APTEXTRACTTEMPLATE_H_
#define _APTEXTRACTTEMPLATE_H_

#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/dirstream.h>

class DebFile : public pkgDirStream
{
	FileFd File;
	unsigned long Size;
	char *Control;
	unsigned long ControlLen;
	
public:
	DebFile(const char *FileName);
	~DebFile();
	bool DoItem(Item &I, int &fd);
	bool Process(pkgDirStream::Item &I, const unsigned char *data, 
		unsigned long size, unsigned long pos);

	bool Go();
	bool ParseInfo();

	static string GetInstalledVer(const string &package);

	string Package;
	string Version;
	string DepVer, PreDepVer;
	unsigned int DepOp, PreDepOp;

	char *Config;
	char *Template;

	static pkgCache *Cache;
	enum { None, IsControl, IsConfig, IsTemplate } Which;
};

#endif
