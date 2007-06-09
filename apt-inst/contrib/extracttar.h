// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: extracttar.h,v 1.2 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   Extract a Tar - Tar Extractor
   
   The tar extractor takes an ordinary gzip compressed tar stream from 
   the given file and explodes it, passing the individual items to the
   given Directory Stream for processing.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EXTRACTTAR_H
#define PKGLIB_EXTRACTTAR_H

#include <apt-pkg/fileutl.h>
#include <apt-pkg/dirstream.h>

#include <algorithm>

using std::min;

class ExtractTar
{
   protected:
   
   struct TarHeader;
   
   // The varios types items can be
   enum ItemType {NormalFile0 = '\0',NormalFile = '0',HardLink = '1',
                  SymbolicLink = '2',CharacterDevice = '3',
                  BlockDevice = '4',Directory = '5',FIFO = '6',
                  GNU_LongLink = 'K',GNU_LongName = 'L'};

   FileFd &File;
   unsigned long MaxInSize;
   int GZPid;
   FileFd InFd;
   bool Eof;
   string DecompressProg;
   
   // Fork and reap gzip
   bool StartGzip();
   bool Done(bool Force);
   
   public:

   bool Go(pkgDirStream &Stream);
   
   ExtractTar(FileFd &Fd,unsigned long Max,string DecompressionProgram);
   virtual ~ExtractTar();
};

#endif
