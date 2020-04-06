// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Debian Archive File (.deb)

   This Class handles all the operations performed directly on .deb 
   files. It makes use of the AR and TAR classes to give the necessary
   external interface.
   
   There are only two things that can be done with a raw package, 
   extract it's control information and extract the contents itself. 

   This should probably subclass an as-yet unwritten super class to
   produce a generic archive mechanism.
  
   The memory control file extractor is useful to extract a single file
   into memory from the control.tar.gz
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBFILE_H
#define PKGLIB_DEBFILE_H

#include <apt-pkg/arfile.h>
#include <apt-pkg/dirstream.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/tagfile.h>

#include <string>


class FileFd;

class APT_PUBLIC debDebFile
{
   protected:
   
   FileFd &File;
   ARArchive AR;
   
   bool CheckMember(const char *Name);
   
   public:
   class ControlExtract;
   class MemControlExtract;

   bool ExtractTarMember(pkgDirStream &Stream, const char *Name);
   bool ExtractArchive(pkgDirStream &Stream);
   const ARArchive::Member *GotoMember(const char *Name);
   inline FileFd &GetFile() {return File;};
   
   explicit debDebFile(FileFd &File);
};

class APT_PUBLIC debDebFile::ControlExtract : public pkgDirStream
{
   public:
   
   virtual bool DoItem(Item &Itm,int &Fd) APT_OVERRIDE;
};

class APT_PUBLIC debDebFile::MemControlExtract : public pkgDirStream
{
   bool IsControl;
   
   public:
   
   char *Control;
   pkgTagSection Section;
   unsigned long Length;
   std::string Member;
   
   // Members from DirStream
   virtual bool DoItem(Item &Itm,int &Fd) APT_OVERRIDE;
   virtual bool Process(Item &Itm,const unsigned char *Data,
			unsigned long long Size,unsigned long long Pos) APT_OVERRIDE;

   // Helpers
   bool Read(debDebFile &Deb);
   bool TakeControl(const void *Data,unsigned long long Size);

   MemControlExtract() : IsControl(false), Control(0), Length(0), Member("control") {};
   explicit MemControlExtract(std::string Member) : IsControl(false), Control(0), Length(0), Member(Member) {};
   ~MemControlExtract() {delete [] Control;};   
};
									/*}}}*/

#endif
