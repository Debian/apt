// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.h,v 1.8 2004/03/17 05:58:54 mdz Exp $
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSRCRECORDS_H
#define PKGLIB_DEBSRCRECORDS_H


#include <apt-pkg/srcrecords.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>

class debSrcRecordParser : public pkgSrcRecords::Parser
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   FileFd Fd;
   pkgTagFile Tags;
   pkgTagSection Sect;
   std::vector<const char*> StaticBinList;
   unsigned long iOffset;
   char *Buffer;
   
   public:

   virtual bool Restart() {return Tags.Jump(Sect,0);};
   virtual bool Step() {iOffset = Tags.Offset(); return Tags.Step(Sect);};
   virtual bool Jump(unsigned long const &Off) {iOffset = Off; return Tags.Jump(Sect,Off);};

   virtual std::string Package() const {return Sect.FindS("Package");};
   virtual std::string Version() const {return Sect.FindS("Version");};
   virtual std::string Maintainer() const {return Sect.FindS("Maintainer");};
   virtual std::string Section() const {return Sect.FindS("Section");};
   virtual const char **Binaries();
   virtual bool BuildDepends(std::vector<BuildDepRec> &BuildDeps, bool const &ArchOnly, bool const &StripMultiArch = true);
   virtual unsigned long Offset() {return iOffset;};
   virtual std::string AsStr() 
   {
      const char *Start=0,*Stop=0;
      Sect.GetSection(Start,Stop);
      return std::string(Start,Stop);
   };
   virtual bool Files(std::vector<pkgSrcRecords::File> &F);

   debSrcRecordParser(std::string const &File,pkgIndexFile const *Index) 
      : Parser(Index), Fd(File,FileFd::ReadOnly, FileFd::Extension), Tags(&Fd,102400), 
        Buffer(NULL) {}
   virtual ~debSrcRecordParser();
};

#endif
