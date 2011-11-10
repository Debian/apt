// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: extract.h,v 1.2 2001/02/20 07:03:16 jgg Exp $
/* ######################################################################

   Archive Extraction Directory Stream
   
   This Directory Stream implements extraction of an archive into the
   filesystem. It makes the choices on what files should be unpacked and
   replaces as well as guiding the actual unpacking.
   
   When the unpacking sequence is completed one of the two functions,
   Finished or Aborted must be called.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EXTRACT_H
#define PKGLIB_EXTRACT_H



#include <apt-pkg/dirstream.h>
#include <apt-pkg/filelist.h>
#include <apt-pkg/pkgcache.h>

class pkgExtract : public pkgDirStream
{
   pkgFLCache &FLCache;
   pkgCache::VerIterator Ver;
   pkgFLCache::PkgIterator FLPkg;
   char FileName[1024];
   bool Debug;
   
   bool HandleOverwrites(pkgFLCache::NodeIterator Nde,
			 bool DiverCheck = false);
   bool CheckDirReplace(std::string Dir,unsigned int Depth = 0);
   
   public:
   
   virtual bool DoItem(Item &Itm,int &Fd);
   virtual bool Fail(Item &Itm,int Fd);
   virtual bool FinishedFile(Item &Itm,int Fd);

   bool Finished();
   bool Aborted();
   
   pkgExtract(pkgFLCache &FLCache,pkgCache::VerIterator Ver);
};

#endif
