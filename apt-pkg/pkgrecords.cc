// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgrecords.cc,v 1.2 1998/08/19 06:16:10 jgg Exp $
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
#include <apt-pkg/configuration.h>
									/*}}}*/

// Records::pkgRecords - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This will create the necessary structures to access the status files */
pkgRecords::pkgRecords(pkgCache &Cache) : Cache(Cache), Files(0)
{
   string ListDir = _config->FindDir("Dir::State::lists");
   
   Files = new PkgFile[Cache.HeaderP->PackageFileCount];   
   for (pkgCache::PkgFileIterator I = Cache.FileBegin(); 
	I.end() == false; I++)
   {
      // We can not initialize if the cache is out of sync.
      if (I.IsOk() == false)
      {
	 _error->Error("Package file %s is out of sync.",I.FileName());
	 return;
      }
   
      // Create the file
      Files[I->ID].File = new FileFd(ListDir + I.FileName(),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return;
      
      // Create the parser
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
