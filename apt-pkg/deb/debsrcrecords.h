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

   virtual bool Restart() APT_OVERRIDE {return Jump(0);};
   virtual bool Step() APT_OVERRIDE {iOffset = Tags.Offset(); return Tags.Step(Sect);};
   virtual bool Jump(unsigned long const &Off) APT_OVERRIDE {iOffset = Off; return Tags.Jump(Sect,Off);};

   virtual std::string Package() const APT_OVERRIDE;
   virtual std::string Version() const APT_OVERRIDE {return Sect.Find(pkgTagSection::Key::Version).to_string();};
   virtual std::string Maintainer() const APT_OVERRIDE {return Sect.Find(pkgTagSection::Key::Maintainer).to_string();};
   virtual std::string Section() const APT_OVERRIDE {return Sect.Find(pkgTagSection::Key::Section).to_string();};
   virtual const char **Binaries() APT_OVERRIDE;
   virtual bool BuildDepends(std::vector<BuildDepRec> &BuildDeps, bool const &ArchOnly, bool const &StripMultiArch = true) APT_OVERRIDE;
   virtual unsigned long Offset() APT_OVERRIDE {return iOffset;};
   virtual std::string AsStr() APT_OVERRIDE 
   {
      const char *Start=0,*Stop=0;
      Sect.GetSection(Start,Stop);
      return std::string(Start,Stop);
   };
   virtual bool Files(std::vector<pkgSrcRecords::File> &F) APT_OVERRIDE;

   debSrcRecordParser(std::string const &File,pkgIndexFile const *Index);
   virtual ~debSrcRecordParser();
};

class APT_HIDDEN debDscRecordParser : public debSrcRecordParser
{
 public:
   debDscRecordParser(std::string const &DscFile, pkgIndexFile const *Index);
};

#endif
