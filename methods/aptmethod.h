#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

#include <algorithm>
#include <locale>
#include <string>
#include <vector>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <apti18n.h>

static bool hasDoubleColon(std::string const &n)
{
   return n.find("::") != std::string::npos;
}

class aptMethod : public pkgAcqMethod
{
protected:
   std::string const Binary;

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

   bool CalculateHashes(FetchItem const * const Itm, FetchResult &Res) const APT_NONNULL(2)
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

   std::vector<std::string> methodNames;
   void setPostfixForMethodNames(char const * const postfix) APT_NONNULL(2)
   {
      methodNames.erase(std::remove_if(methodNames.begin(), methodNames.end(), hasDoubleColon), methodNames.end());
      decltype(methodNames) toAdd;
      for (auto && name: methodNames)
	 toAdd.emplace_back(name + "::" + postfix);
      std::move(toAdd.begin(), toAdd.end(), std::back_inserter(methodNames));
   }
   bool DebugEnabled() const
   {
      if (methodNames.empty())
	 return false;
      auto const sni = std::find_if_not(methodNames.crbegin(), methodNames.crend(), hasDoubleColon);
      if (unlikely(sni == methodNames.crend()))
	 return false;
      auto const ln = methodNames[methodNames.size() - 1];
      // worst case: all three are the same
      std::string confln, confsn, confpn;
      strprintf(confln, "Debug::Acquire::%s", ln.c_str());
      strprintf(confsn, "Debug::Acquire::%s", sni->c_str());
      auto const pni = sni->substr(0, sni->find('+'));
      strprintf(confpn, "Debug::Acquire::%s", pni.c_str());
      return _config->FindB(confln,_config->FindB(confsn, _config->FindB(confpn, false)));
   }
   std::string ConfigFind(char const * const postfix, std::string const &defValue) const APT_NONNULL(2)
   {
      for (auto name = methodNames.rbegin(); name != methodNames.rend(); ++name)
      {
	 std::string conf;
	 strprintf(conf, "Acquire::%s::%s", name->c_str(), postfix);
	 auto const value = _config->Find(conf);
	 if (value.empty() == false)
	    return value;
      }
      return defValue;
   }
   std::string ConfigFind(std::string const &postfix, std::string const &defValue) const
   {
      return ConfigFind(postfix.c_str(), defValue);
   }
   bool ConfigFindB(char const * const postfix, bool const defValue) const APT_NONNULL(2)
   {
      return StringToBool(ConfigFind(postfix, defValue ? "yes" : "no"), defValue);
   }
   int ConfigFindI(char const * const postfix, int const defValue) const APT_NONNULL(2)
   {
      char *End;
      std::string const value = ConfigFind(postfix, "");
      auto const Res = strtol(value.c_str(), &End, 0);
      if (value.c_str() == End)
	 return defValue;
      return Res;
   }

   bool TransferModificationTimes(char const * const From, char const * const To, time_t &LastModified) APT_NONNULL(2, 3)
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

   aptMethod(std::string &&Binary, char const * const Ver, unsigned long const Flags) APT_NONNULL(3) :
      pkgAcqMethod(Ver, Flags), Binary(Binary), methodNames({Binary})
   {
      try {
	 std::locale::global(std::locale(""));
      } catch (...) {
	 setlocale(LC_ALL, "");
      }
   }
};

#endif
