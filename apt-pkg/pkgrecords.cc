// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgrecords.cc,v 1.8 2003/09/02 04:52:16 mdz Exp $
/* ######################################################################
   
   Package Records - Allows access to complete package description records
                     directly from the file.
     
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
    
#include <apti18n.h>   
									/*}}}*/

// Records::pkgRecords - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* This will create the necessary structures to access the status files */
pkgRecords::pkgRecords(pkgCache &Cache) : Cache(Cache), 
  Files(Cache.HeaderP->PackageFileCount)
{
   for (pkgCache::PkgFileIterator I = Cache.FileBegin();
        I.end() == false; I++)
   {
      const pkgIndexFile::Type *Type = pkgIndexFile::Type::GetType(I.IndexType());
      if (Type == 0)
      {
         _error->Error(_("Index file type '%s' is not supported"),I.IndexType());
         return;
      }

      Files[I->ID] = Type->CreatePkgParser(I);
      if (Files[I->ID] == 0)
         return;
   }
}
									/*}}}*/
// Records::~pkgRecords - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::~pkgRecords()
{
   for ( vector<Parser*>::iterator it = Files.begin();
     it != Files.end();
     ++it)
   {
      delete *it;
   }
}
									/*}}}*/
// Records::Lookup - Get a parser for the package version file		/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::Parser &pkgRecords::Lookup(pkgCache::VerFileIterator const &Ver)
{
   Files[Ver.File()->ID]->Jump(Ver);
   return *Files[Ver.File()->ID];
}
									/*}}}*/
// Records::Lookup - Get a parser for the package description file	/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgRecords::Parser &pkgRecords::Lookup(pkgCache::DescFileIterator const &Desc)
{
   Files[Desc.File()->ID]->Jump(Desc);
   return *Files[Desc.File()->ID];
}
									/*}}}*/
