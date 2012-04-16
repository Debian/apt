// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexcopy.h,v 1.3 2001/05/27 04:46:54 jgg Exp $
/* ######################################################################

   Index Copying - Aid for copying and verifying the index files
   
   ##################################################################### */
									/*}}}*/
#ifndef INDEXCOPY_H
#define INDEXCOPY_H

#include <vector>
#include <string>
#include <stdio.h>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
using std::vector;
#endif

class pkgTagSection;
class FileFd;
class indexRecords;
class pkgCdromStatus;

class IndexCopy								/*{{{*/
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   protected:
   
   pkgTagSection *Section;
   
   std::string ChopDirs(std::string Path,unsigned int Depth);
   bool ReconstructPrefix(std::string &Prefix,std::string OrigPath,std::string CD,
			  std::string File);
   bool ReconstructChop(unsigned long &Chop,std::string Dir,std::string File);
   void ConvertToSourceList(std::string CD,std::string &Path);
   bool GrabFirst(std::string Path,std::string &To,unsigned int Depth);
   virtual bool GetFile(std::string &Filename,unsigned long long &Size) = 0;
   virtual bool RewriteEntry(FILE *Target,std::string File) = 0;
   virtual const char *GetFileName() = 0;
   virtual const char *Type() = 0;
   
   public:

   bool CopyPackages(std::string CDROM,std::string Name,std::vector<std::string> &List,
		     pkgCdromStatus *log);
   virtual ~IndexCopy() {};
};
									/*}}}*/
class PackageCopy : public IndexCopy					/*{{{*/
{
   protected:
   
   virtual bool GetFile(std::string &Filename,unsigned long long &Size);
   virtual bool RewriteEntry(FILE *Target,std::string File);
   virtual const char *GetFileName() {return "Packages";};
   virtual const char *Type() {return "Package";};
   
};
									/*}}}*/
class SourceCopy : public IndexCopy					/*{{{*/
{
   protected:
   
   virtual bool GetFile(std::string &Filename,unsigned long long &Size);
   virtual bool RewriteEntry(FILE *Target,std::string File);
   virtual const char *GetFileName() {return "Sources";};
   virtual const char *Type() {return "Source";};
   
};
									/*}}}*/
class TranslationsCopy							/*{{{*/
{
   protected:
   pkgTagSection *Section;

   public:
   bool CopyTranslations(std::string CDROM,std::string Name,std::vector<std::string> &List,
			 pkgCdromStatus *log);
};
									/*}}}*/
class SigVerify								/*{{{*/
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   bool Verify(std::string prefix,std::string file, indexRecords *records);
   bool CopyMetaIndex(std::string CDROM, std::string CDName, 
		      std::string prefix, std::string file);

 public:
   bool CopyAndVerify(std::string CDROM,std::string Name,std::vector<std::string> &SigList,
		      std::vector<std::string> PkgList,std::vector<std::string> SrcList);

   /** \brief generates and run the command to verify a file with gpgv */
   static bool RunGPGV(std::string const &File, std::string const &FileOut,
		       int const &statusfd, int fd[2]);
   inline static bool RunGPGV(std::string const &File, std::string const &FileOut,
			      int const &statusfd = -1) {
      int fd[2];
      return RunGPGV(File, FileOut, statusfd, fd);
   };
};
									/*}}}*/

#endif
