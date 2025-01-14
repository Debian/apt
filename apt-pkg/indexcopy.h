// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Index Copying - Aid for copying and verifying the index files
   
   ##################################################################### */
									/*}}}*/
#ifndef INDEXCOPY_H
#define INDEXCOPY_H

#include <vector>
#ifndef APT_11_CLEAN_HEADERS
#include <cstdio>
#include <string>
#endif

#include <apt-pkg/macros.h>


class pkgTagSection;
class pkgCdromStatus;
class FileFd;
class metaIndex;

class APT_PUBLIC IndexCopy								/*{{{*/
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   protected:
   
   pkgTagSection *Section;
   
   std::string ChopDirs(std::string Path,unsigned int Depth);
   bool ReconstructPrefix(std::string &Prefix,std::string OrigPath,std::string CD,
			  std::string File);
   bool ReconstructChop(unsigned long &Chop,std::string Dir,std::string File);
   void ConvertToSourceList(std::string CD,std::string &Path);
   bool GrabFirst(std::string Path,std::string &To,unsigned int Depth);
   virtual bool GetFile(std::string &Filename,unsigned long long &Size) = 0;
   virtual bool RewriteEntry(FileFd &Target, std::string const &File) = 0;
   virtual const char *GetFileName() = 0;
   virtual const char *Type() = 0;
   
   public:

   bool CopyPackages(std::string CDROM,std::string Name,std::vector<std::string> &List,
		     pkgCdromStatus *log);
   IndexCopy();
   virtual ~IndexCopy();
};
									/*}}}*/
class APT_PUBLIC PackageCopy : public IndexCopy					/*{{{*/
{
   void * const d;
   protected:
   bool GetFile(std::string &Filename, unsigned long long &Size) override;
   bool RewriteEntry(FileFd &Target, std::string const &File) override;
   const char *GetFileName() override { return "Packages"; }
   const char *Type() override { return "Package"; }

   public:
   PackageCopy();
   ~PackageCopy() override;
};
									/*}}}*/
class APT_PUBLIC SourceCopy : public IndexCopy					/*{{{*/
{
   void * const d;
   protected:
   bool GetFile(std::string &Filename, unsigned long long &Size) override;
   bool RewriteEntry(FileFd &Target, std::string const &File) override;
   const char *GetFileName() override { return "Sources"; }
   const char *Type() override { return "Source"; };

   public:
   SourceCopy();
   ~SourceCopy() override;
};
									/*}}}*/
class APT_PUBLIC  TranslationsCopy							/*{{{*/
{
   void * const d;
   protected:
   pkgTagSection *Section;

   public:
   bool CopyTranslations(std::string CDROM,std::string Name,std::vector<std::string> &List,
			 pkgCdromStatus *log);

   TranslationsCopy();
   virtual ~TranslationsCopy();
};
									/*}}}*/
class APT_PUBLIC SigVerify								/*{{{*/
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   APT_HIDDEN bool Verify(std::string prefix,std::string file, metaIndex *records);
   APT_HIDDEN bool CopyMetaIndex(std::string CDROM, std::string CDName,
		      std::string prefix, std::string file);

 public:
   bool CopyAndVerify(std::string CDROM,std::string Name,std::vector<std::string> &SigList,
		      std::vector<std::string> PkgList,std::vector<std::string> SrcList);

   SigVerify();
   virtual ~SigVerify();
};
									/*}}}*/

#endif
