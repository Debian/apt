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
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   protected:
   
   virtual void Erase(const char * /*File*/,std::string /*Pkg*/,std::string /*Ver*/,struct stat & /*St*/) {};

   public:   
   
   bool Go(std::string Dir,pkgCache &Cache);
   virtual ~pkgArchiveCleaner() {};
};

#endif
