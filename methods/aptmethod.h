#ifndef APT_APTMETHOD_H
#define APT_APTMETHOD_H

#include "config.h"

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/netrc.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <locale>
#include <memory>
#include <string>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <apti18n.h>

#ifdef HAVE_SECCOMP
#include <signal.h>

#include <seccomp.h>
#endif

enum class ResultState
{
   TRANSIENT_ERROR,
   FATAL_ERROR,
   SUCCESSFUL
};

static bool hasDoubleColon(std::string const &n)
{
   return n.find("::") != std::string::npos;
}

class aptConfigWrapperForMethods
{
protected:
   std::vector<std::string> methodNames;
public:
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
	 auto value = _config->Find(conf);
	 if (not value.empty())
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

   explicit aptConfigWrapperForMethods(std::string const &name) : methodNames{{name}} {}
   explicit aptConfigWrapperForMethods(std::vector<std::string> &&names) : methodNames{std::move(names)} {}
};

class aptMethod : public pkgAcqMethod, public aptConfigWrapperForMethods
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

   bool RunningInQemu(void)
   {
      int status;
      pid_t pid;

      pid = fork();
      if (pid == 0)
      {
	 close(0);
	 close(1);
	 close(2);
	 setenv("QEMU_VERSION", "meow", 1);
	 char path[] = LIBEXEC_DIR "/apt-helper";
	 char *const argv[] = {path, NULL};
	 execv(argv[0], argv);
	 _exit(255);
      }

      // apt-helper is supposed to exit with an error. If it exited with 0,
      // qemu-user had problems with QEMU_VERSION and returned 0 => running in
      // qemu-user.

      if (waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0)
	 return true;

      return false;
   }

   bool LoadSeccomp()
   {
#ifdef HAVE_SECCOMP
      int rc;
      scmp_filter_ctx ctx = NULL;

      if (SeccompFlags == 0)
	 return true;

      if (_config->FindB("APT::Sandbox::Seccomp", false) == false)
	 return true;

      if (RunningInQemu() == true)
      {
	 Warning("Running in qemu-user, not using seccomp");
	 return true;
      }

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
      ALLOW(clock_getres_time64);
      ALLOW(clock_gettime);
      ALLOW(clock_gettime64);
      ALLOW(clock_nanosleep);
      ALLOW(clock_nanosleep_time64);
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
      ALLOW(futex_time64);
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
      ALLOW(nanosleep);
      ALLOW(newfstatat);
      ALLOW(_newselect);
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
      ALLOW(ppoll_time64);
      ALLOW(prctl);
      ALLOW(prlimit64);
      ALLOW(pselect6);
      ALLOW(pselect6_time64);
      ALLOW(read);
      ALLOW(readv);
      ALLOW(rename);
      ALLOW(renameat);
      ALLOW(renameat2);
      ALLOW(restart_syscall);
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
      ALLOW(stat64);
      ALLOW(statfs);
      ALLOW(statfs64);
#ifdef __NR_statx
      ALLOW(statx);
#endif
      ALLOW(sync);
      ALLOW(syscall);
      ALLOW(sysinfo);
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
      ALLOW(utimensat_time64);
      ALLOW(utimes);
      ALLOW(write);
      ALLOW(writev);

      if ((SeccompFlags & Seccomp::NETWORK) != 0)
      {
	 ALLOW(bind);
	 ALLOW(connect);
	 ALLOW(getsockname);
	 ALLOW(getsockopt);
	 ALLOW(recv);
	 ALLOW(recvfrom);
	 ALLOW(recvmmsg);
	 ALLOW(recvmmsg_time64);
	 ALLOW(recvmsg);
	 ALLOW(send);
	 ALLOW(sendmmsg);
	 ALLOW(sendmsg);
	 ALLOW(sendto);
	 ALLOW(setsockopt);
	 ALLOW(shutdown);
	 ALLOW(socket);
	 ALLOW(socketcall);
      }

      if ((SeccompFlags & Seccomp::DIRECTORY) != 0)
      {
	 ALLOW(readdir);
	 ALLOW(getdents);
	 ALLOW(getdents64);
      }

      if (getenv("FAKED_MODE"))
      {
	 ALLOW(semop);
	 ALLOW(semget);
	 ALLOW(msgsnd);
	 ALLOW(msgrcv);
	 ALLOW(msgget);
	 ALLOW(msgctl);
	 ALLOW(ipc);
      }

      for (auto &custom : _config->FindVector("APT::Sandbox::Seccomp::Allow"))
      {
	 if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, seccomp_syscall_resolve_name(custom.c_str()), 0)))
	    return _error->FatalE("aptMethod::Configuration", "Cannot allow %s: %s", custom.c_str(), strerror(-rc));
      }

#undef ALLOW

      rc = seccomp_load(ctx);
      if (rc == -EINVAL)
      {
	 std::string msg;
	 strprintf(msg, "aptMethod::Configuration: could not load seccomp policy: %s", strerror(-rc));
	 Warning(std::move(msg));
      }
      else if (rc != 0)
	 return _error->FatalE("aptMethod::Configuration", "could not load seccomp policy: %s", strerror(-rc));

      if (_config->FindB("APT::Sandbox::Seccomp::Print", true))
      {
	 struct sigaction action;
	 memset(&action, 0, sizeof(action));
	 sigemptyset(&action.sa_mask);
	 action.sa_sigaction = [](int, siginfo_t *info, void *) {
	    // Formats a number into a 10 digit ASCII string
	    char buffer[10];
	    int number = info->si_syscall;

	    for (int i = sizeof(buffer) - 1; i >= 0; i--)
	    {
	       buffer[i] = (number % 10) + '0';
	       number /= 10;
	    }

	    constexpr const char *str1 = "\n **** Seccomp prevented execution of syscall ";
	    constexpr const char *str2 = " on architecture ";
	    constexpr const char *str3 = " ****\n";
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
	    write(2, str1, strlen(str1));
	    write(2, buffer, sizeof(buffer));
	    write(2, str2, strlen(str2));
	    write(2, COMMON_ARCH, strlen(COMMON_ARCH));
	    write(2, str3, strlen(str3));
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	    _exit(31);
	 };
	 action.sa_flags = SA_SIGINFO;

	 sigaction(SIGSYS, &action, nullptr);
      }
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

   void Warning(std::string &&msg)
   {
      std::unordered_map<std::string, std::string> fields;
      if (Queue != 0)
	 fields.emplace("URI", Queue->Uri);
      else
	 fields.emplace("URI", "<UNKNOWN>");
      if (not UsedMirror.empty())
	 fields.emplace("UsedMirror", UsedMirror);
      fields.emplace("Message", std::move(msg));
      SendMessage("104 Warning", std::move(fields));
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

   // This is a copy of #pkgAcqMethod::Dequeue which is private & hidden
   void Dequeue()
   {
      FetchItem const *const Tmp = Queue;
      Queue = Queue->Next;
      if (Tmp == QueueBack)
	 QueueBack = Queue;
      delete Tmp;
   }
   static std::string URIEncode(std::string const &part)
   {
      // The "+" is encoded as a workaround for an S3 bug (LP#1003633 and LP#1086997)
      return QuoteString(part, _config->Find("Acquire::URIEncode", "+~ ").c_str());
   }

   static std::string DecodeSendURI(std::string const &part)
   {
      if (_config->FindB("Acquire::Send-URI-Encoded", false))
	 return DeQuoteString(part);
      return part;
   }

   aptMethod(std::string &&Binary, char const *const Ver, unsigned long const Flags) APT_NONNULL(3)
       : pkgAcqMethod(Ver, Flags), aptConfigWrapperForMethods(Binary), Binary(std::move(Binary)), SeccompFlags(0)
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
   std::vector<std::unique_ptr<FileFd>> authconfs;

   public:
   virtual bool Configuration(std::string Message) APT_OVERRIDE
   {
      if (pkgAcqMethod::Configuration(Message) == false)
	 return false;

      std::string const conf = std::string("Binary::") + Binary;
      _config->MoveSubTree(conf.c_str(), NULL);

      // ignore errors with opening the auth file as it doesn't need to exist
      _error->PushToStack();
      auto const netrc = _config->FindFile("Dir::Etc::netrc");
      if (netrc.empty() == false)
      {
	 authconfs.emplace_back(new FileFd());
	 authconfs.back()->Open(netrc, FileFd::ReadOnly);
      }

      auto const netrcparts = _config->FindDir("Dir::Etc::netrcparts");
      if (netrcparts.empty() == false)
      {
	 for (auto &&netrcpart : GetListOfFilesInDir(netrcparts, "conf", true, true))
	 {
	    authconfs.emplace_back(new FileFd());
	    authconfs.back()->Open(netrcpart, FileFd::ReadOnly);
	 }
      }
      _error->RevertToStack();

      DropPrivsOrDie();

      if (LoadSeccomp() == false)
	 return false;

      return true;
   }

   bool MaybeAddAuthTo(URI &uri)
   {
      bool result = true;

      if (uri.User.empty() == false || uri.Password.empty() == false)
	 return true;

      _error->PushToStack();
      for (auto &authconf : authconfs)
      {
	 if (authconf->IsOpen() == false)
	    continue;
	 if (authconf->Seek(0) == false)
	 {
	    result = false;
	    continue;
	 }

	 result &= MaybeAddAuth(*authconf, uri);
      }

      while (not _error->empty())
      {
	 std::string message;
	 _error->PopMessage(message);
	 Warning(std::move(message));
      }
      _error->RevertToStack();

      return result;
   }

   aptAuthConfMethod(std::string &&Binary, char const *const Ver, unsigned long const Flags) APT_NONNULL(3)
       : aptMethod(std::move(Binary), Ver, Flags) {}
};
#endif
