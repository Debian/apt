#ifndef _debfile_H
#define _debfile_H

#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/dirstream.h>

class DebFile : public pkgDirStream
{
	const char *ParseDepends(const char *Start,const char *Stop,
				char *&Package, char *&Ver,
				unsigned int &Op);

	char *CopyString(const char *start, unsigned int len);

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

	static char *GetInstalledVer(const char *package);

	char *Package;
	char *Version;
	char *DepVer, *PreDepVer;
	unsigned int DepOp, PreDepOp;

	char *Config;
	char *Template;

	static pkgCache *Cache;
	enum { None, IsControl, IsConfig, IsTemplate } Which;
};

#endif
