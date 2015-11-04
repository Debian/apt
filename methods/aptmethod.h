#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include <apt-pkg/acquire-method.h>

#include <string>

class aptMethod : public pkgAcqMethod
{
   char const * const Binary;
   public:
   virtual bool Configuration(std::string Message) APT_OVERRIDE;

   bool CalculateHashes(FetchItem const * const Itm, FetchResult &Res) const;

   aptMethod(char const * const Binary, char const * const Ver, unsigned long const Flags) : pkgAcqMethod(Ver, Flags), Binary(Binary) {};
};
bool aptMethod::Configuration(std::string Message)
{
   if (pkgAcqMethod::Configuration(Message) == false)
      return false;

   DropPrivsOrDie();

   return true;
}
bool aptMethod::CalculateHashes(FetchItem const * const Itm, FetchResult &Res) const
{
   Hashes Hash(Itm->ExpectedHashes);
   FileFd Fd;
   if (Fd.Open(Res.Filename, FileFd::ReadOnly) == false || Hash.AddFD(Fd) == false)
      return false;
   Res.TakeHashes(Hash);
   return true;
}

#endif
