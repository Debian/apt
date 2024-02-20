// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+
// Description								/*{{{*/
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   FileExists - Returns true if the file exists
   SafeGetCWD - Returns the CWD in a string with overrun protection 
   
   The file class is a handy abstraction for various functions+classes
   that need to accept filenames.
   
   This file had this historic note, but now includes further changes
   under the GPL-2.0+:

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_FILEUTL_H
#define PKGLIB_FILEUTL_H

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/macros.h>

#include <ctime>
#include <set>
#include <string>
#include <vector>
#include <sys/stat.h>

/* Define this for python-apt */
#define APT_HAS_GZIP 1

class FileFdPrivate;
class APT_PUBLIC FileFd
{
   friend class FileFdPrivate;
   friend class GzipFileFdPrivate;
   friend class Bz2FileFdPrivate;
   friend class LzmaFileFdPrivate;
   friend class Lz4FileFdPrivate;
   friend class ZstdFileFdPrivate;
   friend class DirectFileFdPrivate;
   friend class PipedFileFdPrivate;
   protected:
   int iFd;
 
   enum LocalFlags {AutoClose = (1<<0),Fail = (1<<1),DelOnFail = (1<<2),
                    HitEof = (1<<3), Replace = (1<<4), Compressed = (1<<5) };
   unsigned long Flags;
   std::string FileName;
   std::string TemporaryFileName;

   public:
   enum OpenMode {
	ReadOnly = (1 << 0),
	WriteOnly = (1 << 1),
	ReadWrite = ReadOnly | WriteOnly,

	Create = (1 << 2),
	Exclusive = (1 << 3),
	Atomic = Exclusive | (1 << 4),
	Empty = (1 << 5),
	BufferedWrite = (1 << 6),

	WriteEmpty = ReadWrite | Create | Empty,
	WriteExists = ReadWrite,
	WriteAny = ReadWrite | Create,
	WriteTemp = ReadWrite | Create | Exclusive,
	ReadOnlyGzip,
	WriteAtomic = ReadWrite | Create | Atomic
   };
   enum CompressMode
   {
      Auto = 'A',
      None = 'N',
      Extension = 'E',
      Gzip = 'G',
      Bzip2 = 'B',
      Lzma = 'L',
      Xz = 'X',
      Lz4 = '4',
      Zstd = 'Z'
   };

   inline bool Read(void *To,unsigned long long Size,bool AllowEof)
   {
      unsigned long long Jnk;
      if (AllowEof)
	 return Read(To,Size,&Jnk);
      return Read(To,Size);
   }   
   bool Read(void *To,unsigned long long Size,unsigned long long *Actual = 0);
   bool static Read(int const Fd, void *To, unsigned long long Size, unsigned long long * const Actual = 0);
   /** read a complete line or until buffer is full
    *
    * The buffer will always be \\0 terminated, so at most Size-1 characters are read.
    * If the buffer holds a complete line the last character (before \\0) will be
    * the newline character \\n otherwise the line was longer than the buffer.
    *
    * @param To buffer which will hold the line
    * @param Size of the buffer to fill
    * @param \b nullptr is returned in error cases, otherwise
    * the parameter \b To now filled with the line.
    */
   char* ReadLine(char *To, unsigned long long const Size);
   /** read a complete line from the file
    *
    *  Similar to std::getline() the string does \b not include
    *  the newline, but just the content of the line as the newline
    *  is not needed to distinguish cases as for the other #ReadLine method.
    *
    *  @param To string which will hold the line
    *  @return \b true if successful, otherwise \b false
    */
   bool ReadLine(std::string &To);
   bool Flush();
   bool Write(const void *From,unsigned long long Size);
   bool static Write(int Fd, const void *From, unsigned long long Size);
   bool Seek(unsigned long long To);
   bool Skip(unsigned long long To);
   bool Truncate(unsigned long long To);
   unsigned long long Tell();
   // the size of the file content (compressed files will be uncompressed first)
   unsigned long long Size();
   // the size of the file itself
   unsigned long long FileSize();
   time_t ModificationTime();

   bool Open(std::string FileName,unsigned int const Mode,CompressMode Compress,unsigned long const AccessMode = 0666);
   bool Open(std::string FileName,unsigned int const Mode,APT::Configuration::Compressor const &compressor,unsigned long const AccessMode = 0666);
   inline bool Open(std::string const &FileName,unsigned int const Mode, unsigned long const AccessMode = 0666) {
      return Open(FileName, Mode, None, AccessMode);
   };
   bool OpenDescriptor(int Fd, unsigned int const Mode, CompressMode Compress, bool AutoClose=false);
   bool OpenDescriptor(int Fd, unsigned int const Mode, APT::Configuration::Compressor const &compressor, bool AutoClose=false);
   inline bool OpenDescriptor(int Fd, unsigned int const Mode, bool AutoClose=false) {
      return OpenDescriptor(Fd, Mode, None, AutoClose);
   };
   bool Close();
   bool Sync();
   
   // Simple manipulators
   inline int Fd() {return iFd;};
   inline void Fd(int fd) { OpenDescriptor(fd, ReadWrite);};

   inline bool IsOpen() {return iFd >= 0;};
   inline bool Failed() {return (Flags & Fail) == Fail;};
   inline void EraseOnFailure() {Flags |= DelOnFail;};
   inline void OpFail() {Flags |= Fail;};
   inline bool Eof() {return (Flags & HitEof) == HitEof;};
   inline bool IsCompressed() {return (Flags & Compressed) == Compressed;};
   inline std::string &Name() {return FileName;};
   inline void SetFileName(std::string const &name) { FileName = name; };

   FileFd(std::string FileName,unsigned int const Mode,unsigned long AccessMode = 0666);
   FileFd(std::string FileName,unsigned int const Mode, CompressMode Compress, unsigned long AccessMode = 0666);
   FileFd();
   FileFd(int const Fd, unsigned int const Mode = ReadWrite, CompressMode Compress = None);
   FileFd(int const Fd, bool const AutoClose);
   virtual ~FileFd();

   private:
   FileFdPrivate * d;
   APT_HIDDEN FileFd(const FileFd &);
   APT_HIDDEN FileFd & operator=(const FileFd &);
   APT_HIDDEN bool OpenInternDescriptor(unsigned int const Mode, APT::Configuration::Compressor const &compressor);

   // private helpers to set Fail flag and call _error->Error
   APT_HIDDEN bool FileFdErrno(const char* Function, const char* Description,...) APT_PRINTF(3) APT_COLD;
   APT_HIDDEN bool FileFdError(const char* Description,...) APT_PRINTF(2) APT_COLD;
};

APT_PUBLIC bool RunScripts(const char *Cnf);
APT_PUBLIC bool CopyFile(FileFd &From,FileFd &To);
APT_PUBLIC bool RemoveFile(char const * const Function, std::string const &FileName);
APT_PUBLIC bool RemoveFileAt(char const * const Function, int const dirfd, std::string const &FileName);
APT_PUBLIC int GetLock(std::string File,bool Errors = true);
APT_PUBLIC bool FileExists(std::string File);
APT_PUBLIC bool RealFileExists(std::string File);
APT_PUBLIC bool DirectoryExists(std::string const &Path);
APT_PUBLIC bool CreateDirectory(std::string const &Parent, std::string const &Path);
APT_PUBLIC time_t GetModificationTime(std::string const &Path);
APT_PUBLIC bool Rename(std::string From, std::string To);

APT_PUBLIC std::string GetTempDir();
APT_PUBLIC std::string GetTempDir(std::string const &User);
APT_PUBLIC FileFd* GetTempFile(std::string const &Prefix = "",
                    bool ImmediateUnlink = true,
		    FileFd * const TmpFd = NULL);

// FIXME: GetTempFile should always return a buffered file
APT_HIDDEN FileFd* GetTempFile(std::string const &Prefix,
                    bool ImmediateUnlink ,
		    FileFd * const TmpFd,
          bool Buffered);

/** \brief Ensure the existence of the given Path
 *
 *  \param Parent directory of the Path directory - a trailing
 *  /apt/ will be removed before CreateDirectory call.
 *  \param Path which should exist after (successful) call
 */
APT_PUBLIC bool CreateAPTDirectoryIfNeeded(std::string const &Parent, std::string const &Path);

APT_PUBLIC std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, std::string const &Ext,
					bool const &SortList, bool const &AllowNoExt=false);
APT_PUBLIC std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, std::vector<std::string> const &Ext,
					bool const &SortList);
APT_PUBLIC std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, bool SortList);
APT_PUBLIC std::string SafeGetCWD();
APT_PUBLIC void SetCloseExec(int Fd,bool Close);
APT_PUBLIC void SetNonBlock(int Fd,bool Block);
APT_PUBLIC bool WaitFd(int Fd,bool write = false,unsigned long timeout = 0);
APT_PUBLIC pid_t ExecFork();
APT_PUBLIC pid_t ExecFork(std::set<int> keep_fds);
APT_PUBLIC void MergeKeepFdsFromConfiguration(std::set<int> &keep_fds);
APT_PUBLIC bool ExecWait(pid_t Pid,const char *Name,bool Reap = false);

// check if the given file starts with a PGP cleartext signature
APT_PUBLIC bool StartsWithGPGClearTextSignature(std::string const &FileName);

/** change file attributes to requested known good values
 *
 * The method skips the user:group setting if not root.
 *
 * @param requester is printed as functionname in error cases
 * @param file is the file to be modified
 * @param user is the (new) owner of the file, e.g. _apt
 * @param group is the (new) group owning the file, e.g. root
 * @param mode is the access mode of the file, e.g. 0644
 */
APT_PUBLIC bool ChangeOwnerAndPermissionOfFile(char const * const requester, char const * const file, char const * const user, char const * const group, mode_t const mode);

/**
 * \brief Drop privileges
 *
 * Drop the privileges to the user _apt (or the one specified in
 * APT::Sandbox::User). This does not set the supplementary group
 * ids up correctly, it only uses the default group. Also prevent
 * the process from gaining any new privileges afterwards, at least
 * on Linux.
 *
 * \return true on success, false on failure with _error set
 */
APT_PUBLIC bool DropPrivileges();

// File string manipulators
APT_PUBLIC std::string flNotDir(std::string File);
APT_PUBLIC std::string flNotFile(std::string File);
APT_PUBLIC std::string flNoLink(std::string File);
APT_PUBLIC std::string flExtension(std::string File);
APT_PUBLIC std::string flCombine(std::string Dir,std::string File);

/** \brief Takes a file path and returns the absolute path
 */
APT_PUBLIC std::string flAbsPath(std::string File);
/** \brief removes superfluous /./ and // from path */
APT_HIDDEN std::string flNormalize(std::string file);

// simple c++ glob
APT_PUBLIC std::vector<std::string> Glob(std::string const &pattern, int flags=0);

/** \brief Popen() implementation that execv() instead of using a shell
 *
 * \param Args the execv style command to run
 * \param FileFd is a reference to the FileFd to use for input or output
 * \param Child a reference to the integer that stores the child pid
 *        Note that you must call ExecWait() or similar to cleanup
 * \param Mode is either FileFd::ReadOnly or FileFd::WriteOnly
 * \param CaptureStderr True if we should capture stderr in addition to stdout.
 *                      (default: True).
 * \param Sandbox True if this should run sandboxed
 * \return true on success, false on failure with _error set
 */
APT_PUBLIC bool Popen(const char *Args[], FileFd &Fd, pid_t &Child, FileFd::OpenMode Mode, bool CaptureStderr = true, bool Sandbox = false);

APT_HIDDEN bool OpenConfigurationFileFd(std::string const &File, FileFd &Fd);

APT_HIDDEN int Inhibit(const char *what, const char *who, const char *why, const char *mode);

#endif
