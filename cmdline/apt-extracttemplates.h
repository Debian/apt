// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   apt-extracttemplate - tool to extract template and config data
   
   ##################################################################### */
									/*}}}*/
#ifndef _APTEXTRACTTEMPLATE_H_
#define _APTEXTRACTTEMPLATE_H_

#include <apt-pkg/dirstream.h>
#include <apt-pkg/fileutl.h>

#include <string>

class pkgCache;

class DebFile : public pkgDirStream
{
	FileFd File;
	char *Control;
	unsigned long ControlLen;
	
public:
	explicit DebFile(const char *FileName);
	~DebFile();
	bool DoItem(Item &I, int &fd) APT_OVERRIDE;
	bool Process(pkgDirStream::Item &I, const unsigned char *data, 
		unsigned long long size, unsigned long long pos) APT_OVERRIDE;

	bool Go();
	bool ParseInfo();

	static std::string GetInstalledVer(const std::string &package);

	std::string Package;
	std::string Version;
	std::string DepVer, PreDepVer;
	unsigned int DepOp, PreDepOp;

	char *Config;
	char *Template;

	static pkgCache *Cache;
	enum { None, IsControl, IsConfig, IsTemplate } Which;
};

#endif
