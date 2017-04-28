// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: clean.h,v 1.2 1999/07/20 05:53:33 jgg Exp $
/* ######################################################################

   Clean - Clean out downloaded directories
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_CLEAN_H
#define APTPKG_CLEAN_H

#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/pkgcache.h>
#endif

#include <string>

#include <apt-pkg/macros.h>

class pkgCache;

class pkgArchiveCleaner
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   protected:

   APT_DEPRECATED_MSG("Use pkgArchiveCleaner2 to avoid CWD expectations and chdir") virtual void Erase(const char * /*File*/,std::string /*Pkg*/,std::string /*Ver*/,struct stat & /*St*/) {};

   public:

   bool Go(std::string Dir,pkgCache &Cache);

   pkgArchiveCleaner();
   virtual ~pkgArchiveCleaner();
};
// TODO: merge classes and "erase" the old way
class pkgArchiveCleaner2: public pkgArchiveCleaner
{
   friend class pkgArchiveCleaner;
protected:
   using pkgArchiveCleaner::Erase;
   virtual void Erase(int const dirfd, char const * const File,
	 std::string const &Pkg,std::string const &Ver,
	 struct stat const &St) = 0;
};

#endif
