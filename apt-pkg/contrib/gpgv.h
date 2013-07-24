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

#include <apt-pkg/fileutl.h>

#if __GNUC__ >= 4
	#define APT_noreturn	__attribute__ ((noreturn))
#else
	#define APT_noreturn	/* no support */
#endif

/** \brief generates and run the command to verify a file with gpgv
 *
 * If File and FileSig specify the same file it is assumed that we
 * deal with a clear-signed message. Note that the method will accept
 * and validate files which include additional (unsigned) messages
 * without complaining. Do NOT open files accepted by this method
 * for reading. Use #OpenMaybeClearSignedFile to access the message
 * instead to ensure you are only reading signed data.
 *
 * The method does not return, but has some noteable exit-codes:
 * 111 signals an internal error like the inability to execute gpgv,
 * 112 indicates a clear-signed file which doesn't include a message,
 *  which can happen if APT is run while on a network requiring
 *  authentication before usage (e.g. in hotels)
 * All other exit-codes are passed-through from gpgv.
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
 *  If a FileFd pointers is NULL it will not be used and the content
 *  which would have been written to it is silently discarded.
 *
 *  The content of the split files is undefined if the splitting was
 *  unsuccessful.
 *
 *  Note that trying to split an unsigned file will fail, but
 *  not generate an error message.
 *
 *  @param InFile is the clear-signed file
 *  @param ContentFile is the FileFd the message will be written to
 *  @param ContentHeader is a list of all required Amored Headers for the message
 *  @param SignatureFile is the FileFd all signatures will be written to
 *  @return true if the splitting was successful, false otherwise
 */
bool SplitClearSignedFile(std::string const &InFile, FileFd * const ContentFile,
      std::vector<std::string> * const ContentHeader, FileFd * const SignatureFile);

/** \brief open a file which might be clear-signed
 *
 * This method tries to extract the (signed) message of a file.
 * If the file isn't signed it will just open the given filename.
 * Otherwise the message is extracted to a temporary file which
 * will be opened instead.
 *
 * @param ClearSignedFileName is the name of the file to open
 * @param[out] MessageFile is the FileFd in which the file will be opened
 * @return true if opening was successful, otherwise false
 */
bool OpenMaybeClearSignedFile(std::string const &ClearSignedFileName, FileFd &MessageFile);

#endif
