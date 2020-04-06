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


class OpProgress;
class pkgCacheGenerator;

class APT_HIDDEN edspLikeIndex : public pkgDebianIndexRealFile
{
protected:
   virtual bool OpenListFile(FileFd &Pkg, std::string const &File) APT_OVERRIDE;
   virtual uint8_t GetIndexFlags() const APT_OVERRIDE;
   virtual std::string GetArchitecture() const APT_OVERRIDE;

public:
   virtual bool Exists() const APT_OVERRIDE;
   virtual bool HasPackages() const APT_OVERRIDE;

   explicit edspLikeIndex(std::string const &File);
   virtual ~edspLikeIndex();
};

class APT_HIDDEN edspIndex : public edspLikeIndex
{
protected:
   APT_HIDDEN virtual pkgCacheListParser * CreateListParser(FileFd &Pkg) APT_OVERRIDE;
   virtual std::string GetComponent() const APT_OVERRIDE;

public:
   virtual const Type *GetType() const APT_OVERRIDE APT_PURE;

   explicit edspIndex(std::string const &File);
   virtual ~edspIndex();
};

class APT_HIDDEN eippIndex : public edspLikeIndex
{
protected:
   APT_HIDDEN virtual pkgCacheListParser * CreateListParser(FileFd &Pkg) APT_OVERRIDE;
   virtual std::string GetComponent() const APT_OVERRIDE;

public:
   virtual const Type *GetType() const APT_OVERRIDE APT_PURE;

   explicit eippIndex(std::string const &File);
   virtual ~eippIndex();
};

#endif
