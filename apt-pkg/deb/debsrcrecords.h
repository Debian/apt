// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsrcrecords.h,v 1.6 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################
   
   Debian Source Package Records - Parser implementation for Debian style
                                   source indexes
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSRCRECORDS_H
#define PKGLIB_DEBSRCRECORDS_H

#ifdef __GNUG__
#pragma interface "apt-pkg/debsrcrecords.h"
#endif 

#include <apt-pkg/srcrecords.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/fileutl.h>

class debSrcRecordParser : public pkgSrcRecords::Parser
{
   FileFd Fd;
   pkgTagFile Tags;
   pkgTagSection Sect;
   char Buffer[10000];
   char *StaticBinList[400];
   unsigned long iOffset;
   
   public:

   virtual bool Restart() {return Tags.Jump(Sect,0);};
   virtual bool Step() {iOffset = Tags.Offset(); return Tags.Step(Sect);};
   virtual bool Jump(unsigned long Off) {iOffset = Off; return Tags.Jump(Sect,Off);};

   virtual string Package() const {return Sect.FindS("Package");};
   virtual string Version() const {return Sect.FindS("Version");};
   virtual string Maintainer() const {return Sect.FindS("Maintainer");};
   virtual string Section() const {return Sect.FindS("Section");};
   virtual const char **Binaries();
   virtual bool BuildDepends(vector<BuildDepRec> &BuildDeps);
   virtual unsigned long Offset() {return iOffset;};
   virtual string AsStr() 
   {
      const char *Start=0,*Stop=0;
      Sect.GetSection(Start,Stop);
      return string(Start,Stop);
   };
   virtual bool Files(vector<pkgSrcRecords::File> &F);

   debSrcRecordParser(string File,pkgIndexFile const *Index) :
                   Parser(Index),      
                   Fd(File,FileFd::ReadOnly),
                   Tags(&Fd,sizeof(Buffer)) {};
};

#endif
