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
   bool OpenListFile(FileFd &Pkg, std::string const &File) override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;
   [[nodiscard]] std::string GetArchitecture() const override;

   public:
   [[nodiscard]] bool Exists() const override;
   [[nodiscard]] bool HasPackages() const override;

   explicit edspLikeIndex(std::string const &File);
   ~edspLikeIndex() override;
};

class APT_HIDDEN edspIndex : public edspLikeIndex
{
protected:
   APT_HIDDEN pkgCacheListParser *CreateListParser(FileFd &Pkg) override;
   [[nodiscard]] std::string GetComponent() const override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   explicit edspIndex(std::string const &File);
   ~edspIndex() override;
};

class APT_HIDDEN eippIndex : public edspLikeIndex
{
protected:
   APT_HIDDEN pkgCacheListParser *CreateListParser(FileFd &Pkg) override;
   [[nodiscard]] std::string GetComponent() const override;

   public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   explicit eippIndex(std::string const &File);
   ~eippIndex() override;
};

#endif
