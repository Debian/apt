// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgrecords.cc,v 1.1 1998/08/09 00:51:35 jgg Exp $
/* ######################################################################
   
   Package Records - Allows access to complete package description records
                     directly from the file.
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/pkgrecords.h"
#endif
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/debrecords.h>
#include <apt-pkg/error.h>
									/*}}}*/

// Records::pkgRecords - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This will create the necessary structures to access the status files */
pkgRecords::pkgRecords(pkgCache &Cache) : Cache(Cache), Files(0)
{
   Files = new PkgFile[Cache.HeaderP->PackageFileCount];   
   for (pkgCache::PkgFileIterator I = Cache.FileBegin(); 
	I.end() == false; I++)
   {
      Files[I->ID].File = new FileFd(I.FileName(),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return;
      Files[I->ID].Parse = new debRecordParser(*Files[I->ID].File);
      if (_error->PendingError() == true)
	 return;
   }   
}
									/*}}}*/
// Records::~pkgRecords - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::~pkgRecords()
{
   delete [] Files;
}
									/*}}}*/
// Records::Lookup - Get a parser for the package version file		/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::Parser &pkgRecords::Lookup(pkgCache::VerFileIterator &Ver)
{   
   PkgFile &File = Files[Ver.File()->ID];
   File.Parse->Jump(Ver);

   return *File.Parse;
}
									/*}}}*/
// Records::Pkgfile::~PkgFile - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::PkgFile::~PkgFile()
{
   delete Parse;
   delete File;
}
									/*}}}*/
