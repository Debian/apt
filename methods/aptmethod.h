#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include "config.h"

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/netrc.h>

#include <algorithm>
#include <locale>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <apti18n.h>

#ifdef HAVE_SECCOMP
#include <seccomp.h>
#endif

static bool hasDoubleColon(std::string const &n)
{
   return n.find("::") != std::string::npos;
}

class aptMethod : public pkgAcqMethod
{
protected:
   std::string const Binary;
   unsigned long SeccompFlags;
   enum Seccomp
   {
      BASE = (1 << 1),
      NETWORK = (1 << 2),
      DIRECTORY = (1 << 3),
   };

   public:
   virtual bool Configuration(std::string Message) APT_OVERRIDE
   {
      if (pkgAcqMethod::Configuration(Message) == false)
	 return false;

      std::string const conf = std::string("Binary::") + Binary;
      _config->MoveSubTree(conf.c_str(), NULL);

      DropPrivsOrDie();
      if (LoadSeccomp() == false)
	 return false;

      return true;
   }

   bool LoadSeccomp()
   {
#ifdef HAVE_SECCOMP
      int rc;
      scmp_filter_ctx ctx = NULL;

      if (SeccompFlags == 0)
	 return true;

      if (_config->FindB("APT::Sandbox::Seccomp", true) == false)
	 return true;

      ctx = seccomp_init(SCMP_ACT_TRAP);
      if (ctx == NULL)
	 return _error->FatalE("HttpMethod::Configuration", "Cannot init seccomp");

#define ALLOW(what)                                                     \
   if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(what), 0))) \
      return _error->FatalE("HttpMethod::Configuration", "Cannot allow %s: %s", #what, strerror(-rc));

      for (auto &custom : _config->FindVector("APT::Sandbox::Seccomp::Trap"))
      {
	 if ((rc = seccomp_rule_add(ctx, SCMP_ACT_TRAP, seccomp_syscall_resolve_name(custom.c_str()), 0)))
	    return _error->FatalE("HttpMethod::Configuration", "Cannot trap %s: %s", custom.c_str(), strerror(-rc));
      }

      ALLOW(access);
      ALLOW(arch_prctl);
      ALLOW(brk);
      ALLOW(chmod);
      ALLOW(chown);
      ALLOW(chown32);
      ALLOW(clock_getres);
      ALLOW(clock_gettime);
      ALLOW(close);
      ALLOW(creat);
      ALLOW(dup);
      ALLOW(dup2);
      ALLOW(dup3);
      ALLOW(exit);
      ALLOW(exit_group);
      ALLOW(faccessat);
      ALLOW(fchmod);
      ALLOW(fchmodat);
      ALLOW(fchown);
      ALLOW(fchown32);
      ALLOW(fchownat);
      ALLOW(fcntl);
      ALLOW(fcntl64);
      ALLOW(fdatasync);
      ALLOW(flock);
      ALLOW(fstat);
      ALLOW(fstat64);
      ALLOW(fstatat64);
      ALLOW(fstatfs);
      ALLOW(fstatfs64);
      ALLOW(fsync);
      ALLOW(ftime);
      ALLOW(ftruncate);
      ALLOW(ftruncate64);
      ALLOW(futex);
      ALLOW(futimesat);
      ALLOW(getegid);
      ALLOW(getegid32);
      ALLOW(geteuid);
      ALLOW(geteuid32);
      ALLOW(getgid);
      ALLOW(getgid32);
      ALLOW(getgroups);
      ALLOW(getgroups32);
      ALLOW(getpeername);
      ALLOW(getpgid);
      ALLOW(getpgrp);
      ALLOW(getpid);
      ALLOW(getppid);
      ALLOW(getrandom);
      ALLOW(getresgid);
      ALLOW(getresgid32);
      ALLOW(getresuid);
      ALLOW(getresuid32);
      ALLOW(getrlimit);
      ALLOW(get_robust_list);
      ALLOW(getrusage);
      ALLOW(gettid);
      ALLOW(gettimeofday);
      ALLOW(getuid);
      ALLOW(getuid32);
      ALLOW(ioctl);
      ALLOW(lchown);
      ALLOW(lchown32);
      ALLOW(_llseek);
      ALLOW(lseek);
      ALLOW(lstat);
      ALLOW(lstat64);
      ALLOW(madvise);
      ALLOW(mmap);
      ALLOW(mmap2);
      ALLOW(mprotect);
      ALLOW(mremap);
      ALLOW(msync);
      ALLOW(munmap);
      ALLOW(newfstatat);
      ALLOW(oldfstat);
      ALLOW(oldlstat);
      ALLOW(oldolduname);
      ALLOW(oldstat);
      ALLOW(olduname);
      ALLOW(open);
      ALLOW(openat);
      ALLOW(pipe);
      ALLOW(pipe2);
      ALLOW(poll);
      ALLOW(ppoll);
      ALLOW(prctl);
      ALLOW(prlimit64);
      ALLOW(pselect6);
      ALLOW(read);
      ALLOW(rename);
      ALLOW(renameat);
      ALLOW(rt_sigaction);
      ALLOW(rt_sigpending);
      ALLOW(rt_sigprocmask);
      ALLOW(rt_sigqueueinfo);
      ALLOW(rt_sigreturn);
      ALLOW(rt_sigsuspend);
      ALLOW(rt_sigtimedwait);
      ALLOW(sched_yield);
      ALLOW(select);
      ALLOW(set_robust_list);
      ALLOW(sigaction);
      ALLOW(sigpending);
      ALLOW(sigprocmask);
      ALLOW(sigreturn);
      ALLOW(sigsuspend);
      ALLOW(stat);
      ALLOW(statfs);
      ALLOW(sync);
      ALLOW(syscall);
      ALLOW(time);
      ALLOW(truncate);
      ALLOW(truncate64);
      ALLOW(ugetrlimit);
      ALLOW(umask);
      ALLOW(uname);
      ALLOW(unlink);
      ALLOW(unlinkat);
      ALLOW(utime);
      ALLOW(utimensat);
      ALLOW(utimes);
      ALLOW(write);

      if ((SeccompFlags & Seccomp::NETWORK) != 0)
      {
	 ALLOW(bind);
	 ALLOW(connect);
	 ALLOW(getsockname);
	 ALLOW(getsockopt);
	 ALLOW(recv);
	 ALLOW(recvfrom);
	 ALLOW(recvmsg);
	 ALLOW(send);
	 ALLOW(sendmsg);
	 ALLOW(sendto);
	 ALLOW(setsockopt);
	 ALLOW(socket);
      }

      if ((SeccompFlags & Seccomp::DIRECTORY) != 0)
      {
	 ALLOW(readdir);
	 ALLOW(getdents);
	 ALLOW(getdents64);
      }

      for (auto &custom : _config->FindVector("APT::Sandbox::Seccomp::Allow"))
      {
	 if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, seccomp_syscall_resolve_name(custom.c_str()), 0)))
	    return _error->FatalE("HttpMethod::Configuration", "Cannot allow %s: %s", custom.c_str(), strerror(-rc));
      }

#undef ALLOW

      rc = seccomp_load(ctx);
      if (rc != 0)
	 return _error->FatalE("HttpMethod::Configuration", "could not load seccomp policy: %s", strerror(-rc));
#endif
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

   aptMethod(std::string &&Binary, char const *const Ver, unsigned long const Flags) APT_NONNULL(3)
       : pkgAcqMethod(Ver, Flags), Binary(Binary), SeccompFlags(0), methodNames({Binary})
   {
      try {
	 std::locale::global(std::locale(""));
      } catch (...) {
	 setlocale(LC_ALL, "");
      }
   }
};
class aptAuthConfMethod : public aptMethod
{
   FileFd authconf;
public:
   virtual bool Configuration(std::string Message) APT_OVERRIDE
   {
      if (pkgAcqMethod::Configuration(Message) == false)
	 return false;

      std::string const conf = std::string("Binary::") + Binary;
      _config->MoveSubTree(conf.c_str(), NULL);

      auto const netrc = _config->FindFile("Dir::Etc::netrc");
      if (netrc.empty() == false)
      {
	 // ignore errors with opening the auth file as it doesn't need to exist
	 _error->PushToStack();
	 authconf.Open(netrc, FileFd::ReadOnly);
	 _error->RevertToStack();
      }

      DropPrivsOrDie();

      if (LoadSeccomp() == false)
	 return false;

      return true;
   }

   bool MaybeAddAuthTo(URI &uri)
   {
      if (uri.User.empty() == false || uri.Password.empty() == false)
	 return true;
      if (authconf.IsOpen() == false)
	 return true;
      if (authconf.Seek(0) == false)
	 return false;
      return MaybeAddAuth(authconf, uri);
   }

   aptAuthConfMethod(std::string &&Binary, char const *const Ver, unsigned long const Flags) APT_NONNULL(3)
       : aptMethod(std::move(Binary), Ver, Flags) {}
};
#endif
