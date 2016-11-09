// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: srcrecords.cc,v 1.7.2.2 2003/12/26 16:27:34 mdz Exp $
/* ######################################################################
   
   Source Package Records - Allows access to source package records
   
   Parses and allows access to the list of source records and searching by
   source name on that list.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/srcrecords.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/macros.h>

#include <string.h>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

// SrcRecords::pkgSrcRecords - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Open all the source index files */
pkgSrcRecords::pkgSrcRecords(pkgSourceList &List) : d(NULL), Files(0)
{
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); ++I)
   {
      std::vector<pkgIndexFile *> *Indexes = (*I)->GetIndexFiles();
      for (std::vector<pkgIndexFile *>::const_iterator J = Indexes->begin();
	   J != Indexes->end(); ++J)
      {
	 _error->PushToStack();
	 Parser* P = (*J)->CreateSrcParser();
	 bool const newError = _error->PendingError();
	 _error->MergeWithStack();
	 if (newError)
	    return;
	 if (P != 0)
	    Files.push_back(P);
      }
   }
   
   // Doesn't work without any source index files
   if (Files.empty() == true)
   {
      _error->Error(_("You must put some 'source' URIs"
		    " in your sources.list"));
      return;
   }   

   Restart();
}
									/*}}}*/
// SrcRecords::~pkgSrcRecords - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSrcRecords::~pkgSrcRecords()
{
   // Blow away all the parser objects
   for(std::vector<Parser*>::iterator I = Files.begin(); I != Files.end(); ++I)
      delete *I;
}
									/*}}}*/
// SrcRecords::Restart - Restart the search				/*{{{*/
// ---------------------------------------------------------------------
/* Return all of the parsers to their starting position */
bool pkgSrcRecords::Restart()
{
   Current = Files.begin();
   for (std::vector<Parser*>::iterator I = Files.begin();
        I != Files.end(); ++I)
      if ((*I)->Offset() != 0)
	 (*I)->Restart();

   return true;
}
									/*}}}*/
// SrcRecords::Step - Step to the next Source Record			/*{{{*/
// ---------------------------------------------------------------------
/* Step to the next source package record */
const pkgSrcRecords::Parser* pkgSrcRecords::Step()
{
   if (Current == Files.end())
      return 0;

   // Step to the next record, possibly switching files
   while ((*Current)->Step() == false)
   {
      ++Current;
      if (Current == Files.end())
         return 0;
   }

   return *Current;
}
									/*}}}*/
// SrcRecords::Find - Find the first source package with the given name	/*{{{*/
// ---------------------------------------------------------------------
/* This searches on both source package names and output binary names and
   returns the first found. A 'cursor' like system is used to allow this
   function to be called multiple times to get successive entries */
pkgSrcRecords::Parser *pkgSrcRecords::Find(const char *Package,bool const &SrcOnly)
{
   while (true)
   {
      if(Step() == 0)
         return 0;

      // Source name hit
      if ((*Current)->Package() == Package)
	 return *Current;
      
      if (SrcOnly == true)
	 continue;
      
      // Check for a binary hit
      const char **I = (*Current)->Binaries();
      for (; I != 0 && *I != 0; ++I)
	 if (strcmp(Package,*I) == 0)
	    return *Current;
   }
}
									/*}}}*/
// Parser::BuildDepType - Convert a build dep to a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgSrcRecords::Parser::BuildDepType(unsigned char const &Type)
{
   const char *fields[] = {"Build-Depends",
			   "Build-Depends-Indep",
			   "Build-Conflicts",
			   "Build-Conflicts-Indep",
			   "Build-Depends-Arch",
			   "Build-Conflicts-Arch"};
   if (unlikely(Type >= sizeof(fields)/sizeof(fields[0])))
      return "";
   return fields[Type];
}
									/*}}}*/
bool pkgSrcRecords::Parser::Files2(std::vector<pkgSrcRecords::File2> &F2)/*{{{*/
{
   debSrcRecordParser * const deb = dynamic_cast<debSrcRecordParser*>(this);
   if (deb != NULL)
      return deb->Files2(F2);

   std::vector<pkgSrcRecords::File> F;
   if (Files(F) == false)
      return false;
   for (std::vector<pkgSrcRecords::File>::const_iterator f = F.begin(); f != F.end(); ++f)
   {
      pkgSrcRecords::File2 f2;
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
      f2.MD5Hash = f->MD5Hash;
      f2.Size = f->Size;
      f2.Hashes.push_back(HashString("MD5Sum", f->MD5Hash));
      f2.FileSize = f->Size;
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
      f2.Path = f->Path;
      f2.Type = f->Type;
      F2.push_back(f2);
   }
   return true;
}
									/*}}}*/


pkgSrcRecords::Parser::Parser(const pkgIndexFile *Index) : d(NULL), iIndex(Index) {}
pkgSrcRecords::Parser::~Parser() {}
