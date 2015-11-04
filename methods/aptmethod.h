#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>

#include <string>

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

   aptMethod(char const * const Binary, char const * const Ver, unsigned long const Flags) :
      pkgAcqMethod(Ver, Flags), Binary(Binary)
   {}
};

#endif
