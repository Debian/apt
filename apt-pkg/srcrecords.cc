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
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
    
#include <apti18n.h>    
									/*}}}*/

// SrcRecords::pkgSrcRecords - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Open all the source index files */
pkgSrcRecords::pkgSrcRecords(pkgSourceList &List) : Files(0), Current(0)
{
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      vector<pkgIndexFile *> *Indexes = (*I)->GetIndexFiles();
      for (vector<pkgIndexFile *>::const_iterator J = Indexes->begin();
	   J != Indexes->end(); J++)
      {
         Parser* P = (*J)->CreateSrcParser();
	 if (_error->PendingError() == true)
            return;
         if (P != 0)
            Files.push_back(P);
      }
   }
   
   // Doesn't work without any source index files
   if (Files.size() == 0)
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
   for(vector<Parser*>::iterator I = Files.begin(); I != Files.end(); ++I)
      delete *I;
}
									/*}}}*/
// SrcRecords::Restart - Restart the search				/*{{{*/
// ---------------------------------------------------------------------
/* Return all of the parsers to their starting position */
bool pkgSrcRecords::Restart()
{
   Current = Files.begin();
   for (vector<Parser*>::iterator I = Files.begin();
        I != Files.end(); I++)
      (*I)->Restart();
   
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
   if (Current == Files.end())
      return 0;
   
   while (true)
   {
      // Step to the next record, possibly switching files
      while ((*Current)->Step() == false)
      {
	 if (_error->PendingError() == true)
	    return 0;
	 Current++;
	 if (Current == Files.end())
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
// Parser::BuildDepType - Convert a build dep to a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgSrcRecords::Parser::BuildDepType(unsigned char Type)
{
   const char *fields[] = {"Build-Depends", 
                           "Build-Depends-Indep",
			   "Build-Conflicts",
			   "Build-Conflicts-Indep"};
   if (Type < 4) 
      return fields[Type]; 
   else 
      return "";
}
									/*}}}*/


