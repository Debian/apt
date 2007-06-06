// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: clean.h,v 1.2 1999/07/20 05:53:33 jgg Exp $
/* ######################################################################

   Clean - Clean out downloaded directories
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_CLEAN_H
#define APTPKG_CLEAN_H


#include <apt-pkg/pkgcache.h>

class pkgArchiveCleaner
{
   protected:
   
   virtual void Erase(const char * /*File*/,string /*Pkg*/,string /*Ver*/,struct stat & /*St*/) {};

   public:   
   
   bool Go(string Dir,pkgCache &Cache);
   virtual ~pkgArchiveCleaner() {};
};

#endif
