// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: multicompress.h,v 1.2 2001/02/20 07:03:18 jgg Exp $
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



#include <string>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <stdio.h>
#include <sys/types.h>
    
class MultiCompress
{
   // An output file
   struct Files
   {
      string Output;
      APT::Configuration::Compressor CompressProg;
      Files *Next;
      FileFd TmpFile;
      pid_t CompressProc;
      time_t OldMTime;
      int Fd;
   };
   
   Files *Outputs;
   pid_t Outputter;
   mode_t Permissions;

   bool OpenCompress(APT::Configuration::Compressor const &Prog,
		     pid_t &Pid,int const &FileFd, int &OutFd,bool const &Comp);
   bool Child(int const &Fd);
   bool Start();
   bool Die();
   
   public:
   
   // The FD to write to for compression.
   FILE *Input;
   unsigned long UpdateMTime;
   
   bool Finalize(unsigned long &OutSize);
   bool OpenOld(int &Fd,pid_t &Proc);
   bool CloseOld(int Fd,pid_t Proc);
   static bool GetStat(string const &Output,string const &Compress,struct stat &St);
   
   MultiCompress(string const &Output,string const &Compress,
		 mode_t const &Permissions, bool const &Write = true);
   ~MultiCompress();
};

#endif
