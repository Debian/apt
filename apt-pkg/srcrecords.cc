// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: srcrecords.cc,v 1.2 1999/04/07 05:30:18 jgg Exp $
/* ######################################################################
   
   Source Package Records - Allows access to source package records
   
   Parses and allows access to the list of source records and searching by
   source name on that list.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/srcrecords.h"
#endif 

#include <apt-pkg/srcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/debsrcrecords.h>
									/*}}}*/

// SrcRecords::pkgSrcRecords - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Open all the source index files */
pkgSrcRecords::pkgSrcRecords(pkgSourceList &List) : Files(0), Current(0)
{
   pkgSourceList::const_iterator I = List.begin();
   
   // Count how many items we will need
   unsigned int Count = 0;
   for (; I != List.end(); I++)
      if (I->Type == pkgSourceList::Item::DebSrc)
	 Count++;

   // Doesnt work without any source index files
   if (Count == 0)
   {
      _error->Error("Sorry, you must put some 'source' uris"
		    " in your sources.list");
      return;
   }   

   Files = new Parser *[Count+1];
   memset(Files,0,sizeof(*Files)*(Count+1));
   
   // Create the parser objects
   Count = 0;
   string Dir = _config->FindDir("Dir::State::lists");
   for (I = List.begin(); I != List.end(); I++)
   {
      if (I->Type != pkgSourceList::Item::DebSrc)
	 continue;

      // Open the file
      FileFd *FD = new FileFd(Dir + URItoFileName(I->PackagesURI()),
			      FileFd::ReadOnly);
      if (_error->PendingError() == true)
      {
	 delete FD;
	 return;
      }
      
      Files[Count] = new debSrcRecordParser(FD,I);
      Count++;
   }
   
   Restart();
}
									/*}}}*/
// SrcRecords::~pkgSrcRecords - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSrcRecords::~pkgSrcRecords()
{
   if (Files == 0)
      return;

   // Blow away all the parser objects
   for (unsigned int Count = 0; Files[Count] != 0; Count++)
      delete Files[Count];
}
									/*}}}*/
// SrcRecords::Restart - Restart the search				/*{{{*/
// ---------------------------------------------------------------------
/* Return all of the parsers to their starting position */
bool pkgSrcRecords::Restart()
{
   Current = Files;
   for (Parser **I = Files; *I != 0; I++)
      if ((*I)->Restart() == false)
	 return false;
   return true;
}
									/*}}}*/
// SrcRecords::Find - Find the first source package with the given name	/*{{{*/
// ---------------------------------------------------------------------
/* This searches on both source package names and output binary names and
   returns the first found. A 'cursor' like system is used to allow this
   function to be called multiple times to get successive entries */
pkgSrcRecords::Parser *pkgSrcRecords::Find(const char *Package,bool SrcOnly)
{
   if (*Current == 0)
      return 0;
   
   while (true)
   {
      // Step to the next record, possibly switching files
      while ((*Current)->Step() == false)
      {
	 if (_error->PendingError() == true)
	    return 0;
	 Current++;
	 if (*Current == 0)
	    return 0;
      }
      
      // IO error somehow
      if (_error->PendingError() == true)
	 return 0;

      // Source name hit
      if ((*Current)->Package() == Package)
	 return *Current;
      
      if (SrcOnly == true)
	 continue;
      
      // Check for a binary hit
      const char **I = (*Current)->Binaries();
      for (; I != 0 && *I != 0; I++)
	 if (strcmp(Package,*I) == 0)
	    return *Current;
   }
}
									/*}}}*/

