// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   MultiCompressor
   
   Multiple output class. Takes a single FILE* and writes it simultaneously
   to many compressed files. Then checks if the resulting output is 
   different from any previous output and overwrites the old files. Care is
   taken to ensure that the new files are not generally readable while they
   are being written.
   
   ##################################################################### */
									/*}}}*/
#ifndef MULTICOMPRESS_H
#define MULTICOMPRESS_H

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

class MultiCompress
{
   // An output file
   struct Files
   {
      std::string Output;
      APT::Configuration::Compressor CompressProg;
      Files *Next;
      FileFd TmpFile;
      pid_t CompressProc;
      time_t OldMTime;
   };
   
   Files *Outputs;
   pid_t Outputter;
   mode_t Permissions;

   bool Child(int const &Fd);
   bool Start();
   bool Die();
   
   public:
   
   // The FD to write to for compression.
   FileFd Input;
   unsigned long UpdateMTime;
   
   bool Finalize(unsigned long long &OutSize);
   bool OpenOld(FileFd &Fd);
   static bool GetStat(std::string const &Output,std::string const &Compress,struct stat &St);
   
   MultiCompress(std::string const &Output,std::string const &Compress,
		 mode_t const &Permissions, bool const &Write = true);
   ~MultiCompress();
};

#endif
