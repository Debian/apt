// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   The scenario file is designed to work as an intermediate file between
   APT and the resolver. Its on propose very similar to a dpkg status file
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPINDEXFILE_H
#define PKGLIB_EDSPINDEXFILE_H

#include <apt-pkg/debindexfile.h>
#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#endif

class OpProgress;
class pkgCacheGenerator;

class APT_HIDDEN edspIndex : public pkgDebianIndexRealFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

protected:
   APT_HIDDEN virtual pkgCacheListParser * CreateListParser(FileFd &Pkg) APT_OVERRIDE;
   virtual bool OpenListFile(FileFd &Pkg, std::string const &File) APT_OVERRIDE;
   virtual uint8_t GetIndexFlags() const APT_OVERRIDE;
   virtual std::string GetComponent() const APT_OVERRIDE;
   virtual std::string GetArchitecture() const APT_OVERRIDE;
public:

   virtual const Type *GetType() const APT_OVERRIDE APT_CONST;
   virtual bool Exists() const APT_OVERRIDE;
   virtual bool HasPackages() const APT_OVERRIDE;

   edspIndex(std::string const &File);
   virtual ~edspIndex();
};

#endif
