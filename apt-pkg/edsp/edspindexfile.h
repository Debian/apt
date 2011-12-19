// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   The scenario file is designed to work as an intermediate file between
   APT and the resolver. Its on propose very similar to a dpkg status file
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPINDEXFILE_H
#define PKGLIB_EDSPINDEXFILE_H

#include <apt-pkg/debindexfile.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#endif

class edspIndex : public debStatusIndex
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   public:

   virtual const Type *GetType() const;

   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;

   edspIndex(std::string File);
};

#endif
