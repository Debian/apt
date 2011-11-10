// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: database.cc,v 1.2 2001/02/20 07:03:16 jgg Exp $
/* ######################################################################

   Data Base Abstraction
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/database.h>
#include <apt-pkg/filelist.h>
#include <apt-pkg/pkgcachegen.h>
									/*}}}*/

// DataBase::GetMetaTmp - Get the temp dir				/*{{{*/
// ---------------------------------------------------------------------
/* This re-initializes the meta temporary directory if it hasn't yet 
   been inited for this cycle. The flag is the emptyness of MetaDir */
bool pkgDataBase::GetMetaTmp(std::string &Dir)
{
   if (MetaDir.empty() == true)
      if (InitMetaTmp(MetaDir) == false)
	 return false;
   Dir = MetaDir;
   return true;
}
									/*}}}*/
pkgDataBase::~pkgDataBase()
{
   delete Cache;
   delete FList;
}
