// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Helpers to deal with gpgv better and more easily

   ##################################################################### */
									/*}}}*/
#ifndef CONTRIB_GPGV_H
#define CONTRIB_GPGV_H

#include <string>

#if __GNUC__ >= 4
	#define APT_noreturn	__attribute__ ((noreturn))
#else
	#define APT_noreturn	/* no support */
#endif

/** \brief generates and run the command to verify a file with gpgv
 *
 * If File and FileSig specify the same file it is assumed that we
 * deal with a clear-signed message.
 *
 * @param File is the message (unsigned or clear-signed)
 * @param FileSig is the signature (detached or clear-signed)
 */
void ExecGPGV(std::string const &File, std::string const &FileSig,
      int const &statusfd, int fd[2]) APT_noreturn;
inline void ExecGPGV(std::string const &File, std::string const &FileSig,
      int const &statusfd = -1) {
   int fd[2];
   ExecGPGV(File, FileSig, statusfd, fd);
};

#undef APT_noreturn

#endif
