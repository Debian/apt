#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

#include <string>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <apti18n.h>

class aptMethod : public pkgAcqMethod
{
   char const * const Binary;

public:
   virtual bool Configuration(std::string Message) APT_OVERRIDE
   {
      if (pkgAcqMethod::Configuration(Message) == false)
	 return false;

      std::string const conf = std::string("Binary::") + Binary;
      _config->MoveSubTree(conf.c_str(), NULL);

      DropPrivsOrDie();

      return true;
   }

   bool CalculateHashes(FetchItem const * const Itm, FetchResult &Res) const
   {
      Hashes Hash(Itm->ExpectedHashes);
      FileFd Fd;
      if (Fd.Open(Res.Filename, FileFd::ReadOnly) == false || Hash.AddFD(Fd) == false)
	 return false;
      Res.TakeHashes(Hash);
      return true;
   }

   void Warning(const char *Format,...)
   {
      va_list args;
      va_start(args,Format);
      PrintStatus("104 Warning", Format, args);
      va_end(args);
   }

   bool TransferModificationTimes(char const * const From, char const * const To, time_t &LastModified)
   {
      if (strcmp(To, "/dev/null") == 0)
	 return true;

      struct stat Buf2;
      if (lstat(To, &Buf2) != 0 || S_ISLNK(Buf2.st_mode))
	 return true;

      struct stat Buf;
      if (stat(From, &Buf) != 0)
	 return _error->Errno("stat",_("Failed to stat"));

      // we don't use utimensat here for compatibility reasons: #738567
      struct timeval times[2];
      times[0].tv_sec = Buf.st_atime;
      LastModified = times[1].tv_sec = Buf.st_mtime;
      times[0].tv_usec = times[1].tv_usec = 0;
      if (utimes(To, times) != 0)
	 return _error->Errno("utimes",_("Failed to set modification time"));
      return true;
   }

   aptMethod(char const * const Binary, char const * const Ver, unsigned long const Flags) :
      pkgAcqMethod(Ver, Flags), Binary(Binary)
   {}
};

#endif
