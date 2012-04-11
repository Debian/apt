// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.h,v 1.26 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   FileExists - Returns true if the file exists
   SafeGetCWD - Returns the CWD in a string with overrun protection 
   
   The file class is a handy abstraction for various functions+classes
   that need to accept filenames.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_FILEUTL_H
#define PKGLIB_FILEUTL_H

#include <apt-pkg/macros.h>
#include <apt-pkg/aptconfiguration.h>

#include <string>
#include <vector>

#include <zlib.h>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
#endif

/* Define this for python-apt */
#define APT_HAS_GZIP 1

class FileFdPrivate;
class FileFd
{
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

	WriteEmpty = ReadWrite | Create | Empty,
	WriteExists = ReadWrite,
	WriteAny = ReadWrite | Create,
	WriteTemp = ReadWrite | Create | Exclusive,
	ReadOnlyGzip,
	WriteAtomic = ReadWrite | Create | Atomic
   };
   enum CompressMode { Auto = 'A', None = 'N', Extension = 'E', Gzip = 'G', Bzip2 = 'B', Lzma = 'L', Xz = 'X' };
   
   inline bool Read(void *To,unsigned long long Size,bool AllowEof)
   {
      unsigned long long Jnk;
      if (AllowEof)
	 return Read(To,Size,&Jnk);
      return Read(To,Size);
   }   
   bool Read(void *To,unsigned long long Size,unsigned long long *Actual = 0);
   char* ReadLine(char *To, unsigned long long const Size);
   bool Write(const void *From,unsigned long long Size);
   bool static Write(int Fd, const void *From, unsigned long long Size);
   bool Seek(unsigned long long To);
   bool Skip(unsigned long long To);
   bool Truncate(unsigned long long To);
   unsigned long long Tell();
   unsigned long long Size();
   unsigned long long FileSize();
   time_t ModificationTime();

   /* You want to use 'unsigned long long' if you are talking about a file
      to be able to support large files (>2 or >4 GB) properly.
      This shouldn't happen all to often for the indexes, but deb's might be…
      And as the auto-conversation converts a 'unsigned long *' to a 'bool'
      instead of 'unsigned long long *' we need to provide this explicitely -
      otherwise applications magically start to fail… */
   __deprecated bool Read(void *To,unsigned long long Size,unsigned long *Actual)
   {
	unsigned long long R;
	bool const T = Read(To, Size, &R);
	*Actual = R;
	return T;
   }

   bool Open(std::string FileName,unsigned int const Mode,CompressMode Compress,unsigned long const Perms = 0666);
   bool Open(std::string FileName,unsigned int const Mode,APT::Configuration::Compressor const &compressor,unsigned long const Perms = 0666);
   inline bool Open(std::string const &FileName,unsigned int const Mode, unsigned long const Perms = 0666) {
      return Open(FileName, Mode, None, Perms);
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
   __deprecated gzFile gzFd();

   inline bool IsOpen() {return iFd >= 0;};
   inline bool Failed() {return (Flags & Fail) == Fail;};
   inline void EraseOnFailure() {Flags |= DelOnFail;};
   inline void OpFail() {Flags |= Fail;};
   inline bool Eof() {return (Flags & HitEof) == HitEof;};
   inline bool IsCompressed() {return (Flags & Compressed) == Compressed;};
   inline std::string &Name() {return FileName;};
   
   FileFd(std::string FileName,unsigned int const Mode,unsigned long Perms = 0666) : iFd(-1), Flags(0), d(NULL)
   {
      Open(FileName,Mode, None, Perms);
   };
   FileFd(std::string FileName,unsigned int const Mode, CompressMode Compress, unsigned long Perms = 0666) : iFd(-1), Flags(0), d(NULL)
   {
      Open(FileName,Mode, Compress, Perms);
   };
   FileFd() : iFd(-1), Flags(AutoClose), d(NULL) {};
   FileFd(int const Fd, unsigned int const Mode = ReadWrite, CompressMode Compress = None) : iFd(-1), Flags(0), d(NULL)
   {
      OpenDescriptor(Fd, Mode, Compress);
   };
   FileFd(int const Fd, bool const AutoClose) : iFd(-1), Flags(0), d(NULL)
   {
      OpenDescriptor(Fd, ReadWrite, None, AutoClose);
   };
   virtual ~FileFd();

   private:
   FileFdPrivate* d;
   bool OpenInternDescriptor(unsigned int const Mode, APT::Configuration::Compressor const &compressor);
};

bool RunScripts(const char *Cnf);
bool CopyFile(FileFd &From,FileFd &To);
int GetLock(std::string File,bool Errors = true);
bool FileExists(std::string File);
bool RealFileExists(std::string File);
bool DirectoryExists(std::string const &Path) __attrib_const;
bool CreateDirectory(std::string const &Parent, std::string const &Path);
time_t GetModificationTime(std::string const &Path);

/** \brief Ensure the existence of the given Path
 *
 *  \param Parent directory of the Path directory - a trailing
 *  /apt/ will be removed before CreateDirectory call.
 *  \param Path which should exist after (successful) call
 */
bool CreateAPTDirectoryIfNeeded(std::string const &Parent, std::string const &Path);

std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, std::string const &Ext,
					bool const &SortList, bool const &AllowNoExt=false);
std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, std::vector<std::string> const &Ext,
					bool const &SortList);
std::vector<std::string> GetListOfFilesInDir(std::string const &Dir, bool SortList);
std::string SafeGetCWD();
void SetCloseExec(int Fd,bool Close);
void SetNonBlock(int Fd,bool Block);
bool WaitFd(int Fd,bool write = false,unsigned long timeout = 0);
pid_t ExecFork();
bool ExecWait(pid_t Pid,const char *Name,bool Reap = false);

// File string manipulators
std::string flNotDir(std::string File);
std::string flNotFile(std::string File);
std::string flNoLink(std::string File);
std::string flExtension(std::string File);
std::string flCombine(std::string Dir,std::string File);

#endif
