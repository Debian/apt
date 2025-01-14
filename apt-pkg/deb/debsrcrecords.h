// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSRCRECORDS_H
#define PKGLIB_DEBSRCRECORDS_H

#include <apt-pkg/fileutl.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/tagfile-keys.h>
#include <apt-pkg/tagfile.h>

#include <cstddef>
#include <string>
#include <vector>

class pkgIndexFile;

class APT_HIDDEN debSrcRecordParser : public pkgSrcRecords::Parser
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

 protected:
   FileFd Fd;
   pkgTagFile Tags;
   pkgTagSection Sect;
   std::vector<const char*> StaticBinList;
   unsigned long iOffset;
   char *Buffer;

   public:
   bool Restart() override { return Jump(0); }
   bool Step() override {iOffset = Tags.Offset(); return Tags.Step(Sect);}
   bool Jump(unsigned long const &Off) override {iOffset = Off; return Tags.Jump(Sect,Off);}

   [[nodiscard]] std::string Package() const override;
   [[nodiscard]] std::string Version() const override { return std::string{Sect.Find(pkgTagSection::Key::Version)}; }
   [[nodiscard]] std::string Maintainer() const override { return std::string{Sect.Find(pkgTagSection::Key::Maintainer)}; }
   [[nodiscard]] std::string Section() const override { return std::string{Sect.Find(pkgTagSection::Key::Section)}; }
   const char **Binaries() override;
   bool BuildDepends(std::vector<BuildDepRec> &BuildDeps, bool const &ArchOnly, bool const &StripMultiArch = true) override;
   unsigned long Offset() override { return iOffset; }
   std::string AsStr() override
   {
      const char *Start=0,*Stop=0;
      Sect.GetSection(Start,Stop);
      return std::string(Start,Stop);
   };
   bool Files(std::vector<pkgSrcRecords::File> &F) override;

   debSrcRecordParser(std::string const &File,pkgIndexFile const *Index);
   ~debSrcRecordParser() override;
};

class APT_HIDDEN debDscRecordParser : public debSrcRecordParser
{
 public:
   debDscRecordParser(std::string const &DscFile, pkgIndexFile const *Index);
};

#endif
