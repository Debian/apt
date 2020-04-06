// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <apt-pkg/macros.h>

#include <string>


class pkgDirStream;

class APT_PUBLIC ExtractTar
{
   protected:
   
   struct TarHeader;
   
   // The varios types items can be
   enum ItemType {NormalFile0 = '\0',NormalFile = '0',HardLink = '1',
                  SymbolicLink = '2',CharacterDevice = '3',
                  BlockDevice = '4',Directory = '5',FIFO = '6',
                  GNU_LongLink = 'K',GNU_LongName = 'L'};

   FileFd &File;
   unsigned long long MaxInSize;
   int GZPid;
   FileFd InFd;
   bool Eof;
   std::string DecompressProg;
   
   // Fork and reap gzip
   bool StartGzip();
   bool Done();

   public:

   bool Go(pkgDirStream &Stream);

   ExtractTar(FileFd &Fd,unsigned long long Max,std::string DecompressionProgram);
   virtual ~ExtractTar();
};

#endif
