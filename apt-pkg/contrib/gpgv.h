// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Helpers to deal with gpgv better and more easily

   ##################################################################### */
									/*}}}*/
#ifndef CONTRIB_GPGV_H
#define CONTRIB_GPGV_H

#include <string>

/** \brief generates and run the command to verify a file with gpgv */
bool ExecGPGV(std::string const &File, std::string const &FileOut,
      int const &statusfd, int fd[2]);

inline bool ExecGPGV(std::string const &File, std::string const &FileOut,
      int const &statusfd = -1) {
   int fd[2];
   return ExecGPGV(File, FileOut, statusfd, fd);
}

#endif
