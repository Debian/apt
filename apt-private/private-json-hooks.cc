/*
 * private-json-hooks.cc - 2nd generation, JSON-RPC, hooks for APT
 *
 * Copyright (c) 2018 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <apt-pkg/debsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>
#include <apt-private/private-json-hooks.h>
#include <apt-private/private-output.h>

#include <iomanip>
#include <ostream>
#include <sstream>
#include <stack>
#include <unordered_map>

#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/**
 * @brief Simple JSON writer
 *
 * This performs no error checking, so be careful.
 */
class APT_HIDDEN JsonWriter
{
   std::ostream &os;
   std::locale old_locale;

   enum write_state
   {
      empty,
      in_array_first_element,
      in_array,
      in_object_first_key,
      in_object_key,
      in_object_val
   } state = empty;

   std::stack<write_state> old_states;

   void maybeComma()
   {
      switch (state)
      {
      case empty:
	 break;
      case in_object_val:
	 state = in_object_key;
	 break;
      case in_object_key:
	 state = in_object_val;
	 os << ',';
	 break;
      case in_array:
	 os << ',';
	 break;
      case in_array_first_element:
	 state = in_array;
	 break;
      case in_object_first_key:
	 state = in_object_val;
	 break;
      default:
	 abort();
      }
   }

   void pushState(write_state state)
   {
      old_states.push(this->state);
      this->state = state;
   }

   void popState()
   {
      this->state = old_states.top();
      old_states.pop();
   }

   public:
   explicit JsonWriter(std::ostream &os) : os(os), old_locale{os.imbue(std::locale::classic())} {}
   ~JsonWriter() { os.imbue(old_locale); }
   JsonWriter &beginArray()
   {
      maybeComma();
      pushState(in_array_first_element);
      os << '[';
      return *this;
   }
   JsonWriter &endArray()
   {
      popState();
      os << ']';
      return *this;
   }
   JsonWriter &beginObject()
   {
      maybeComma();
      pushState(in_object_first_key);
      os << '{';
      return *this;
   }
   JsonWriter &endObject()
   {
      popState();
      os << '}';
      return *this;
   }
   std::ostream &encodeString(std::ostream &out, std::string const &str)
   {
      out << '"';

      for (std::string::const_iterator c = str.begin(); c != str.end(); c++)
      {
	 if (*c <= 0x1F || *c == '"' || *c == '\\')
	    ioprintf(out, "\\u%04X", *c);
	 else
	    out << *c;
      }

      out << '"';
      return out;
   }
   JsonWriter &name(std::string const &name)
   {
      maybeComma();
      encodeString(os, name) << ':';
      return *this;
   }
   JsonWriter &value(std::string const &value)
   {
      maybeComma();
      encodeString(os, value);
      return *this;
   }
   JsonWriter &value(const char *value)
   {
      maybeComma();
      if (value == nullptr)
	 os << "null";
      else
	 encodeString(os, value);
      return *this;
   }
   JsonWriter &value(int value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(long value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(long long value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(unsigned long long value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(unsigned long value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(unsigned int value)
   {
      maybeComma();
      os << value;
      return *this;
   }
   JsonWriter &value(bool value)
   {
      maybeComma();
      os << (value ? "true" : "false");
      return *this;
   }
   JsonWriter &value(double value)
   {
      maybeComma();
      os << value;
      return *this;
   }
};

/**
 * @brief Write a VerFileIterator to a JsonWriter
 */
static void verFiletoJson(JsonWriter &writer, CacheFile &, pkgCache::VerFileIterator const &vf)
{
   auto pf = vf.File(); // Packages file
   auto rf = pf.ReleaseFile(); // release file

   writer.beginObject();
   if (not rf.end()) {
      if (rf->Archive != 0)
	 writer.name("archive").value(rf.Archive());
      if (rf->Codename != 0)
	 writer.name("codename").value(rf.Codename());
      if (rf->Version != 0)
	 writer.name("version").value(rf.Version());
      if (rf->Origin != 0)
	 writer.name("origin").value(rf.Origin());
      if (rf->Label != 0)
	 writer.name("label").value(rf.Label());
      if (rf->Site != 0)
	 writer.name("site").value(rf.Site());
   }

   writer.endObject();
}

/**
 * @brief Write a VerIterator to a JsonWriter
 */
static void verIterToJson(JsonWriter &writer, CacheFile &Cache, pkgCache::VerIterator const &Ver)
{
   writer.beginObject();
   writer.name("id").value(Ver->ID);
   writer.name("version").value(Ver.VerStr());
   writer.name("architecture").value(Ver.Arch());
   writer.name("pin").value(Cache->GetPolicy().GetPriority(Ver));

   writer.name("origins");
   writer.beginArray();
   for (auto vf = Ver.FileList(); !vf.end(); vf++)
      if ((vf.File()->Flags & pkgCache::Flag::NotSource) == 0)
         verFiletoJson(writer, Cache, vf);
   writer.endArray();

   writer.endObject();
}

/**
 * @brief Copy of debSystem::DpkgChrootDirectory()
 * @todo Remove
 */
static void DpkgChrootDirectory()
{
   std::string const chrootDir = _config->FindDir("DPkg::Chroot-Directory");
   if (chrootDir == "/")
      return;
   std::cerr << "Chrooting into " << chrootDir << std::endl;
   if (chroot(chrootDir.c_str()) != 0)
      _exit(100);
   if (chdir("/") != 0)
      _exit(100);
}

/**
 * @brief Send a notification to the hook's stream
 */
static void NotifyHook(std::ostream &os, std::string const &method, const char **FileList, CacheFile &Cache, std::set<std::string> const &UnknownPackages, int hookVersion)
{
   SortedPackageUniverse Universe(Cache);
   JsonWriter jsonWriter{os};

   jsonWriter.beginObject();

   jsonWriter.name("jsonrpc").value("2.0");
   jsonWriter.name("method").value(method);

   /* Build params */
   jsonWriter.name("params").beginObject();
   if (FileList != nullptr)
   {
      jsonWriter.name("command").value(FileList[0]);
      jsonWriter.name("search-terms").beginArray();
      for (int i = 1; FileList[i] != NULL; i++)
	 jsonWriter.value(FileList[i]);
      jsonWriter.endArray();
   }
   jsonWriter.name("unknown-packages").beginArray();
   for (auto const &PkgName : UnknownPackages)
      jsonWriter.value(PkgName);
   jsonWriter.endArray();

   jsonWriter.name("packages").beginArray();
   for (auto const &Pkg : Universe)
   {
      switch (Cache[Pkg].Mode)
      {
      case pkgDepCache::ModeInstall:
      case pkgDepCache::ModeDelete:
	 break;
      default:
	 continue;
      }

      jsonWriter.beginObject();

      jsonWriter.name("id").value(Pkg->ID);
      jsonWriter.name("name").value(Pkg.Name());
      jsonWriter.name("architecture").value(Pkg.Arch());

      switch (Cache[Pkg].Mode)
      {
      case pkgDepCache::ModeInstall:
	 if (Pkg->CurrentVer != 0 && Cache[Pkg].Upgrade() && hookVersion >= 0x020)
	    jsonWriter.name("mode").value("upgrade");
	 else if (Pkg->CurrentVer != 0 && Cache[Pkg].Downgrade() && hookVersion >= 0x020)
	    jsonWriter.name("mode").value("downgrade");
	 else if (Pkg->CurrentVer != 0 && Cache[Pkg].ReInstall() && hookVersion >= 0x020)
	    jsonWriter.name("mode").value("reinstall");
	 else
	    jsonWriter.name("mode").value("install");
	 break;
      case pkgDepCache::ModeDelete:
	 jsonWriter.name("mode").value(Cache[Pkg].Purge() ? "purge" : "deinstall");
	 break;
      default:
	 continue;
      }
      jsonWriter.name("automatic").value(bool(Cache[Pkg].Flags & pkgCache::Flag::Auto));

      jsonWriter.name("versions").beginObject();

      if (Cache[Pkg].CandidateVer != nullptr)
	 verIterToJson(jsonWriter.name("candidate"), Cache, Cache[Pkg].CandidateVerIter(Cache));
      if (Cache[Pkg].InstallVer != nullptr)
	 verIterToJson(jsonWriter.name("install"), Cache, Cache[Pkg].InstVerIter(Cache));
      if (Pkg->CurrentVer != 0)
	 verIterToJson(jsonWriter.name("current"), Cache, Pkg.CurrentVer());

      jsonWriter.endObject();

      jsonWriter.endObject();
   }

   jsonWriter.endArray();  // packages
   jsonWriter.endObject(); // params
   jsonWriter.endObject(); // main
}

/// @brief Build the hello handshake message for 0.1 protocol
static std::string BuildHelloMessage()
{
   std::stringstream Hello;
   JsonWriter(Hello).beginObject().name("jsonrpc").value("2.0").name("method").value("org.debian.apt.hooks.hello").name("id").value(0).name("params").beginObject().name("versions").beginArray().value("0.1").value("0.2").endArray().endObject().endObject();

   return Hello.str();
}

/// @brief Build the bye notification for 0.1 protocol
static std::string BuildByeMessage()
{
   std::stringstream Bye;
   JsonWriter(Bye).beginObject().name("jsonrpc").value("2.0").name("method").value("org.debian.apt.hooks.bye").name("params").beginObject().endObject().endObject();

   return Bye.str();
}

/// @brief Run the Json hook processes in the given option.
bool RunJsonHook(std::string const &option, std::string const &method, const char **FileList, CacheFile &Cache, std::set<std::string> const &UnknownPackages)
{
   std::unordered_map<int, std::string> notifications;
   std::string HelloData = BuildHelloMessage();
   std::string ByeData = BuildByeMessage();
   int hookVersion;

   bool result = true;

   Configuration::Item const *Opts = _config->Tree(option.c_str());
   if (Opts == 0 || Opts->Child == 0)
      return true;
   Opts = Opts->Child;

   // Flush output before calling hooks
   std::clog.flush();
   std::cerr.flush();
   std::cout.flush();
   c2out.flush();
   c1out.flush();

   sighandler_t old_sigpipe = signal(SIGPIPE, SIG_IGN);
   sighandler_t old_sigint = signal(SIGINT, SIG_IGN);
   sighandler_t old_sigquit = signal(SIGQUIT, SIG_IGN);

   unsigned int Count = 1;
   for (; Opts != 0; Opts = Opts->Next, Count++)
   {
      if (Opts->Value.empty() == true)
	 continue;

      if (_config->FindB("Debug::RunScripts", false) == true)
	 std::clog << "Running external script with list of all .deb file: '"
		   << Opts->Value << "'" << std::endl;

      // Create the pipes
      std::set<int> KeepFDs;
      MergeKeepFdsFromConfiguration(KeepFDs);
      int Pipes[2];
      if (socketpair(AF_UNIX, SOCK_STREAM, 0, Pipes) != 0)
      {
	 result = _error->Errno("pipe", "Failed to create IPC pipe to subprocess");
	 break;
      }

      int InfoFD = 3;

      if (InfoFD != Pipes[0])
	 SetCloseExec(Pipes[0], true);
      else
	 KeepFDs.insert(Pipes[0]);

      SetCloseExec(Pipes[1], true);

      // Purified Fork for running the script
      pid_t Process = ExecFork(KeepFDs);
      if (Process == 0)
      {
	 // Setup the FDs
	 dup2(Pipes[0], InfoFD);
	 SetCloseExec(STDOUT_FILENO, false);
	 SetCloseExec(STDIN_FILENO, false);
	 SetCloseExec(STDERR_FILENO, false);

	 std::string hookfd;
	 strprintf(hookfd, "%d", InfoFD);
	 setenv("APT_HOOK_SOCKET", hookfd.c_str(), 1);

	 DpkgChrootDirectory();
	 const char *Args[4];
	 Args[0] = "/bin/sh";
	 Args[1] = "-c";
	 Args[2] = Opts->Value.c_str();
	 Args[3] = 0;
	 execv(Args[0], (char **)Args);
	 _exit(100);
      }
      close(Pipes[0]);
      FILE *F = fdopen(Pipes[1], "w+");
      if (F == 0)
      {
	 result = _error->Errno("fdopen", "Failed to open new FD");
	 break;
      }

      fwrite(HelloData.data(), HelloData.size(), 1, F);
      fwrite("\n\n", 2, 1, F);
      fflush(F);

      char *line = nullptr;
      size_t linesize = 0;
      ssize_t size = getline(&line, &linesize, F);

      if (size < 0)
      {
	 if (errno != ECONNRESET && errno != EPIPE)
	    _error->Error("Could not read response to hello message from hook %s: %s", Opts->Value.c_str(), strerror(errno));
	 goto out;
      }
      else if (strstr(line, "error") != nullptr)
      {
	 _error->Error("Hook %s reported an error during hello: %s", Opts->Value.c_str(), line);
	 goto out;
      }

      if (strstr(line, "\"0.1\""))
      {
	 hookVersion = 0x010;
      }
      else if (strstr(line, "\"0.2\""))
      {
	 hookVersion = 0x020;
      }
      else
      {
	 _error->Error("Unknown hook version in handshake from hook %s: %s", Opts->Value.c_str(), line);
	 goto out;
      }

      size = getline(&line, &linesize, F);
      if (size < 0)
      {
	 _error->Error("Could not read message separator line after handshake from %s: %s", Opts->Value.c_str(), feof(F) ? "end of file" : strerror(errno));
	 goto out;
      }
      else if (size == 0 || line[0] != '\n')
      {
	 _error->Error("Expected empty line after handshake from %s, received %s", Opts->Value.c_str(), line);
	 goto out;
      }
      {
	 std::string &data = notifications[hookVersion];
	 if (data.empty())
	 {
	    std::stringstream ss;
	    NotifyHook(ss, method, FileList, Cache, UnknownPackages, hookVersion);
	    ;
	    data = ss.str();
	 }
	 fwrite(data.data(), data.size(), 1, F);
	 fwrite("\n\n", 2, 1, F);
      }

      fwrite(ByeData.data(), ByeData.size(), 1, F);
      fwrite("\n\n", 2, 1, F);
   out:
      fclose(F);
      // Clean up the sub process
      if (ExecWait(Process, Opts->Value.c_str()) == false)
      {
	 result = _error->Error("Failure running hook %s", Opts->Value.c_str());
	 break;
      }

   }
   signal(SIGINT, old_sigint);
   signal(SIGPIPE, old_sigpipe);
   signal(SIGQUIT, old_sigquit);

   return result;
}
