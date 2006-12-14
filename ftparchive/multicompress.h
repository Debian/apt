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
#include <stdio.h>
#include <sys/types.h>
    
class MultiCompress
{
   // Enumeration of all supported compressors
   struct CompType
   {
      const char *Name;
      const char *Extension;
      const char *Binary;
      const char *CompArgs;
      const char *UnCompArgs;
      unsigned char Cost;
   };

   // An output file
   struct Files
   {
      string Output;
      const CompType *CompressProg;
      Files *Next;      
      FileFd TmpFile;
      pid_t CompressProc;
      time_t OldMTime;
      int Fd;
   };
   
   Files *Outputs;
   pid_t Outputter;
   mode_t Permissions;
   static const CompType Compressors[];

   bool OpenCompress(const CompType *Prog,pid_t &Pid,int FileFd,
		     int &OutFd,bool Comp);
   bool Child(int Fd);
   bool Start();
   bool Die();
   
   public:
   
   // The FD to write to for compression.
   FILE *Input;
   unsigned long UpdateMTime;
   
   bool Finalize(unsigned long &OutSize);
   bool OpenOld(int &Fd,pid_t &Proc);
   bool CloseOld(int Fd,pid_t Proc);
   static bool GetStat(string Output,string Compress,struct stat &St);
   
   MultiCompress(string Output,string Compress,mode_t Permissions,
		 bool Write = true);
   ~MultiCompress();
};

#endif
