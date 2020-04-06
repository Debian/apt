// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Clean - Clean out downloaded directories
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_CLEAN_H
#define APTPKG_CLEAN_H


#include <string>

#include <apt-pkg/macros.h>

class pkgCache;

class APT_PUBLIC pkgArchiveCleaner
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   protected:
   virtual void Erase(int const dirfd, char const * const File,
	 std::string const &Pkg,std::string const &Ver,
	 struct stat const &St) = 0;

   public:

   bool Go(std::string Dir,pkgCache &Cache);

   pkgArchiveCleaner();
   virtual ~pkgArchiveCleaner();
};


#endif
