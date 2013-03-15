// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Helpers to deal with gpgv better and more easily

   ##################################################################### */
									/*}}}*/
#ifndef CONTRIB_GPGV_H
#define CONTRIB_GPGV_H

#include <string>
#include <vector>

#if __GNUC__ >= 4
	#define APT_noreturn	__attribute__ ((noreturn))
#else
	#define APT_noreturn	/* no support */
#endif

/** \brief generates and run the command to verify a file with gpgv
 *
 * If File and FileSig specify the same file it is assumed that we
 * deal with a clear-signed message. In that case the file will be
 * rewritten to be in a good-known format without uneeded whitespaces
 * and additional messages (unsigned or signed).
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

/** \brief Split an inline signature into message and signature
 *
 *  Takes a clear-signed message and puts the first signed message
 *  in the content file and all signatures following it into the
 *  second. Unsigned messages, additional messages as well as
 *  whitespaces are discarded. The resulting files are suitable to
 *  be checked with gpgv.
 *
 *  If one or all Fds are -1 they will not be used and the content
 *  which would have been written to them is discarded.
 *
 *  The code doesn't support dash-encoded lines as these are not
 *  expected to be present in files we have to deal with.
 *
 *  @param InFile is the clear-signed file
 *  @param ContentFile is the Fd the message will be written to
 *  @param ContentHeader is a list of all required Amored Headers for the message
 *  @param SignatureFile is the Fd all signatures will be written to
 */
bool SplitClearSignedFile(std::string const &InFile, int const ContentFile,
      std::vector<std::string> * const ContentHeader, int const SignatureFile);

/** \brief recombines message and signature to an inline signature
 *
 *  Reverses the splitting down by #SplitClearSignedFile by writing
 *  a well-formed clear-signed message without unsigned messages,
 *  additional signed messages or just trailing whitespaces
 *
 *  @param OutFile will be clear-signed file
 *  @param ContentFile is the Fd the message will be read from
 *  @param ContentHeader is a list of all required Amored Headers for the message
 *  @param SignatureFile is the Fd all signatures will be read from
 */
bool RecombineToClearSignedFile(std::string const &OutFile, int const ContentFile,
      std::vector<std::string> const &ContentHeader, int const SignatureFile);

#endif
