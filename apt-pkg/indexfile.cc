// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexfile.cc,v 1.2.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Index File - Abstraction for an index of archive/souce file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/indexfile.h"
#endif

#include <apt-pkg/indexfile.h>
#include <apt-pkg/error.h>
									/*}}}*/

// Global list of Item supported
static  pkgIndexFile::Type *ItmList[10];
pkgIndexFile::Type **pkgIndexFile::Type::GlobalList = ItmList;
unsigned long pkgIndexFile::Type::GlobalListLen = 0;

// Type::Type - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgIndexFile::Type::Type()
{
   ItmList[GlobalListLen] = this;
   GlobalListLen++;   
}
									/*}}}*/
// Type::GetType - Locate the type by name				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgIndexFile::Type *pkgIndexFile::Type::GetType(const char *Type)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(GlobalList[I]->Label,Type) == 0)
	 return GlobalList[I];
   return 0;
}
									/*}}}*/
    
// IndexFile::ArchiveInfo - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgIndexFile::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   return string();
}
									/*}}}*/
// IndexFile::FindInCache - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator pkgIndexFile::FindInCache(pkgCache &Cache) const
{
   return pkgCache::PkgFileIterator(Cache);
}
									/*}}}*/
// IndexFile::SourceIndex - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgIndexFile::SourceInfo(pkgSrcRecords::Parser const &Record,
				pkgSrcRecords::File const &File) const
{
   return string();
}
									/*}}}*/
