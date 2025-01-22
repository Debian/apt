// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/strutl.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <apti18n.h>
									/*}}}*/


class LineBuffer							/*{{{*/
{
   char *buffer = nullptr;
   size_t buffer_size = 0;
   int line_length = 0;
   // a "normal" find_last_not_of returns npos if not found
   int find_last_not_of_length(std::string_view const bad) const
   {
      for (int result = line_length - 1; result >= 0; --result)
	 if (bad.find(buffer[result]) == std::string_view::npos)
	    return result + 1;
      return 0;
   }

   public:
   bool empty() const noexcept { return view().empty(); }
   std::string_view view() const noexcept { return {buffer, static_cast<size_t>(line_length)}; }
   bool starts_with(std::string_view const start) const { return view().substr(0, start.size()) == start; }

   bool writeTo(FileFd *const to, size_t offset = 0) const
   {
      if (to == nullptr)
	 return true;
      return to->Write(buffer + offset, line_length - offset);
   }
   bool writeLineTo(FileFd *const to) const
    {
       if (to == nullptr)
         return true;
       buffer[line_length] = '\n';
       bool const result = to->Write(buffer, line_length + 1);
       buffer[line_length] = '\0';
       return result;
    }
   bool writeNewLineIf(FileFd *const to, bool const condition) const
   {
      if (not condition || to == nullptr)
	 return true;
      return to->Write("\n", 1);
   }

   bool readFrom(FILE *stream, std::string const &InFile, bool acceptEoF = false)
   {
      errno = 0;
      line_length = getline(&buffer, &buffer_size, stream);
      if (errno != 0)
	 return _error->Errno("getline", "Could not read from %s", InFile.c_str());
      if (line_length == -1)
      {
	 if (acceptEoF)
	    return false;
	 return _error->Error("Splitting of clearsigned file %s failed as it doesn't contain all expected parts", InFile.c_str());
      }
      // a) remove newline characters, so we can work consistently with lines
      line_length = find_last_not_of_length("\n\r");
      // b) remove trailing whitespaces as defined by rfc4880 §7.1
      line_length = find_last_not_of_length(" \t");
      buffer[line_length] = '\0';
      return true;
   }

   ~LineBuffer() { free(buffer); }
};
static bool operator==(LineBuffer const &buf, std::string_view const exp) noexcept
{
   return buf.view() == exp;
}
static bool operator!=(LineBuffer const &buf, std::string_view const exp) noexcept
{
   return buf.view() != exp;
}
									/*}}}*/
// ExecGPGV - returns the command needed for verify			/*{{{*/
// ---------------------------------------------------------------------
/* Generating the commandline for calling gpg is somehow complicated as
   we need to add multiple keyrings and user supplied options.
   Also, as gpg has no options to enforce a certain reduced style of
   clear-signed files (=the complete content of the file is signed and
   the content isn't encoded) we do a divide and conquer approach here
   and split up the clear-signed file in message and signature for gpg.
*/
#define apt_error(...) apt_msg("ERROR", __VA_ARGS__)
#define apt_warning(...) apt_msg("WARNING", __VA_ARGS__)
static void APT_PRINTF(5) apt_msg(std::string const &tag, std::ostream &outterm, int const statusfd, int fd[2], const char *format, ...)
{
   std::ostringstream outstr;
   std::ostream &out = (statusfd == -1) ? outterm : outstr;
   va_list args;
   ssize_t size = 400;
   while (true) {
      bool ret;
      va_start(args,format);
      ret = iovprintf(out, format, args, size);
      va_end(args);
      if (ret)
	 break;
   }
   if (statusfd != -1)
   {
      auto const errtag = "[APTKEY:] " + tag + " ";
      outstr << '\n';
      auto const errtext = outstr.str();
      if (not FileFd::Write(fd[1], errtag.data(), errtag.size()) ||
	    not FileFd::Write(fd[1], errtext.data(), errtext.size()))
	 outterm << errtext << std::flush;
   }
}

static bool CheckGPGV(std::unordered_map<std::string, std::forward_list<std::string>> &checkedCommands, std::string gpgv, bool Debug)
{
   if (checkedCommands.find(gpgv) == checkedCommands.end())
   {
      // Create entry
      checkedCommands[gpgv];
      FileFd dumpOptions;
      pid_t child;
      const char *argv[] = {gpgv.c_str(), "--dump-options", nullptr};
      if (unlikely(Debug))
	 std::clog << "Executing " << gpgv << " --dump-options" << std::endl;
      if (not Popen(argv, dumpOptions, child, FileFd::ReadOnly) && Debug)
	 return false;

      for (std::string line; dumpOptions.ReadLine(line);)
      {
	 if (unlikely(Debug))
	    std::clog << "Read line: " << line << std::endl;
	 checkedCommands[gpgv].push_front(APT::String::Strip(line));
      }
      dumpOptions.Close();
      waitpid(child, NULL, 0);
   }
   return not checkedCommands[gpgv].empty();
}

/// Verifies a file containing a detached signature has the right format
/// @return 0 if succesful, or an exit code for ExecGPGV otherwise.
static int VerifyDetachedSignatureFile(std::string const &FileGPG, int fd[2], int statusfd = -1)
{
   auto detached = make_unique_FILE(FileGPG, "r");
   if (detached.get() == nullptr)
      return _error->Error("Detached signature file '%s' could not be opened", FileGPG.c_str()), 111;

   LineBuffer buf;
   bool open_signature = false;
   bool found_badcontent = false;
   size_t found_signatures = 0;
   while (buf.readFrom(detached.get(), FileGPG, true))
   {
      if (open_signature)
      {
	 if (buf == "-----END PGP SIGNATURE-----")
	    open_signature = false;
	 else if (buf.starts_with("-"))
	 {
	    // the used Radix-64 is not using dash for any value, so a valid line can't
	    // start with one. Header keys could, but no existent one does and seems unlikely.
	    // Instead it smells a lot like a header the parser didn't recognize.
	    _error->Error("Detached signature file '%s' contains unexpected line starting with a dash", FileGPG.c_str());
	    return 112;
	 }
      }
      else // if (not open_signature)
      {
	 if (buf == "-----BEGIN PGP SIGNATURE-----")
	 {
	    open_signature = true;
	    ++found_signatures;
	    if (found_badcontent)
	       break;
	 }
	 else
	 {
	    found_badcontent = true;
	    if (found_signatures != 0)
	       break;
	 }
      }
   }
   if (found_signatures == 0)
   {
      if (statusfd != -1 && fd)
      {
	 auto const errtag = "[GNUPG:] NODATA\n";
	 FileFd::Write(fd[1], errtag, strlen(errtag));
      }
      else
      {
	 _error->Error("Signed file isn't valid, got 'NODATA' (does the network require authentication?)");
      }
      // guess if this is a binary signature, we never officially supported them,
      // but silently accepted them via passing them unchecked to gpgv
      if (found_badcontent)
      {
	 rewind(detached.get());
	 auto ptag = fgetc(detached.get());
	 // §4.2 says that the first bit is always set and gpg seems to generate
	 // only old format which is indicated by the second bit not set
	 if (ptag != EOF && (ptag & 0x80) != 0 && (ptag & 0x40) == 0)
	 {
	    _error->Error("Detached signature file '%s' is in unsupported binary format", FileGPG.c_str());
	    return 112;
	 }
      }
      // This is not an attack attempt but a file even gpgv would complain about
      // likely the result of a paywall which is covered by the gpgv method
      return 113;
   }
   else if (found_badcontent)
   {
      _error->Error("Detached signature file '%s' contains lines not belonging to a signature", FileGPG.c_str());
      return 112;
   }
   if (open_signature)
   {
      _error->Error("Detached signature file '%s' contains unclosed signatures", FileGPG.c_str());
      return 112;
   }

   return 0;
}

bool VerifyDetachedSignatureFile(std::string const &DetachedSignatureFileName)
{
   return VerifyDetachedSignatureFile(DetachedSignatureFileName, nullptr, -1) == 0;
}

std::pair<std::string, std::forward_list<std::string>> APT::Internal::FindGPGV(bool Debug)
{
   static thread_local std::unordered_map<std::string, std::forward_list<std::string>> checkedCommands;
   const std::string gpgvVariants[] = {
      _config->Find("Apt::Key::gpgvcommand"),
      // Prefer absolute path
      "/usr/bin/gpgv-sq",
      "/usr/bin/gpgv",
      "gpgv-sq",
      "gpgv",
   };
   for (auto gpgv : gpgvVariants)
      if (CheckGPGV(checkedCommands, gpgv, Debug))
	 return std::make_pair(gpgv, checkedCommands[gpgv]);
   return {};
}

void ExecGPGV(std::string const &File, std::string const &FileGPG,
             int const &statusfd, int fd[2], std::string const &key)
{
   auto const keyFiles = VectorizeString(key, ',');
   ExecGPGV(File, FileGPG, statusfd, fd, keyFiles);
}

void ExecGPGV(std::string const &File, std::string const &FileGPG,
	      int const &statusfd, int fd[2], std::vector<std::string> const &KeyFiles)
{
#define EINTERNAL 111
   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);
   struct exiter {
      std::vector<std::string> files;
      [[noreturn]] void operator ()(int code) {
	 std::for_each(files.begin(), files.end(), [](auto f)
		       { unlink(f.c_str()); });
	 exit(code);
      }
   } local_exit;

   auto [gpgv, supportedOptions] = APT::Internal::FindGPGV(Debug);
   if (gpgv.empty())
   {
      apt_error(std::cerr, statusfd, fd, "Couldn't find a gpgv binary");
      local_exit(EINTERNAL);
   }

   std::vector<std::string> Args;
   Args.reserve(10);

   Args.push_back(gpgv);
   Args.push_back("--ignore-time-conflict");

   FileFd mergedFd;
   if (GetTempFile("apt.XXXXXX.gpg", false, &mergedFd) == nullptr)
      local_exit(EINTERNAL);
   local_exit.files.push_back(mergedFd.Name());

   auto dearmorKeyOrCheckFormat = [&](std::string const &k)
   {
      FileFd keyFd(k, FileFd::ReadOnly);
      if (not keyFd.IsOpen())
      {
	 apt_warning(std::cerr, statusfd, fd, "The key(s) in the keyring %s are ignored as the file is not readable by user executing gpgv.\n", k.c_str());
      }
      else if (APT::String::Endswith(k, ".asc"))
      {
	 std::string b64msg;
	 int state = 0;
	 for (std::string line; keyFd.ReadLine(line);)
	 {
	    line = APT::String::Strip(line);
	    if (APT::String::Startswith(line, "-----BEGIN PGP PUBLIC KEY BLOCK-----"))
	       state = 1;
	    else if (state == 1 && line == "")
	       state = 2;
	    else if (state == 2 && line != "" && line[0] != '=' && line[0] != '-')
	       b64msg += line;
	    else if (APT::String::Startswith(line, "-----END"))
	       state = 3;
	 }
	 if (state != 3)
	    goto err;

	 if (auto decoded = Base64Decode(b64msg); not decoded.empty())
	    if (not mergedFd.Write(decoded.data(), decoded.size()))
	       local_exit(EINTERNAL);
	 return;
      }
      else
      {
	 unsigned char c;
	 if (not keyFd.Read(&c, sizeof(c)))
	    goto err;
	 // Identify the leading byte of an OpenPGP public key packet
	 // 0x98 -- old-format OpenPGP public key packet, up to 255 octets
	 // 0x99 -- old-format OpenPGP public key packet, 256-65535 octets
	 // 0xc6 -- new-format OpenPGP public key packet, any length
	 if (c != 0x98 && c != 0x99 && c != 0xc6)
	    goto err;

	 if (not mergedFd.Write(&c, sizeof(c)))
	    local_exit(EINTERNAL);

	 if (not CopyFile(keyFd, mergedFd))
	    local_exit(EINTERNAL);

	 return;
      }
   err:
      apt_warning(std::cerr, statusfd, fd, "The key(s) in the keyring %s are ignored as the file has an unsupported filetype.", k.c_str());
   };
   auto maybeAddKeyring = [&](std::string const &k)
   {
      if (struct stat st; stat(k.c_str(), &st) != 0 || st.st_size == 0)
	 return;
      dearmorKeyOrCheckFormat(k);
      return;
   };

   bool FoundKeyring = false;
   for (auto const &k : KeyFiles)
   {
      if (unlikely(k.empty()))
	 continue;
      if (k[0] == '/')
      {
	 if (Debug)
	    std::clog << "Trying Signed-By: " << k << std::endl;

	 maybeAddKeyring(k);
	 FoundKeyring = true;
      }
      else
      {
	 Args.push_back("--keyid");
	 Args.push_back(k);
      }
   }

   if (not FoundKeyring)
   {
      // Either trusted or trustedparts must exist
      _error->PushToStack();
      auto Parts = GetListOfFilesInDir(_config->FindDir("Dir::Etc::TrustedParts"), std::vector<std::string>{"gpg", "asc"}, true);
      if (auto trusted = _config->FindFile("Dir::Etc::Trusted"); not trusted.empty())
      {
	 apt_warning(std::cerr, statusfd, fd, "Loading %s from deprecated option Dir::Etc::Trusted\n", trusted.c_str());
	 Parts.push_back(trusted);
      }
      if (Parts.empty())
	 _error->MergeWithStack();
      else
	 _error->RevertToStack();
      for (auto &Part : Parts)
      {
	 if (Debug)
	    std::clog << "Trying TrustedPart: " << Part << std::endl;
	 maybeAddKeyring(Part);
      }
   }

   // If we do not give it any keyring, gpgv shouts keydb errors at us
   Args.push_back("--keyring");
   Args.push_back(mergedFd.Name());

   char statusfdstr[10];
   if (statusfd != -1)
   {
      Args.push_back("--status-fd");
      snprintf(statusfdstr, sizeof(statusfdstr), "%i", statusfd);
      Args.push_back(statusfdstr);
   }

   if (auto assertPubkeyAlgo = _config->Find("Apt::Key::assert-pubkey-algo"); not assertPubkeyAlgo.empty())
   {
      if (std::find(supportedOptions.begin(), supportedOptions.end(), "--assert-pubkey-algo") != supportedOptions.end())
	 Args.push_back("--assert-pubkey-algo=" + assertPubkeyAlgo);
   }

   Configuration::Item const *Opts;
   Opts = _config->Tree("Acquire::gpgv::Options");
   if (Opts != 0)
   {
      Opts = Opts->Child;
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty())
	    continue;
	 Args.push_back(Opts->Value);
      }
   }

   enum  { DETACHED, CLEARSIGNED } releaseSignature = (FileGPG != File) ? DETACHED : CLEARSIGNED;

   if (releaseSignature == DETACHED)
   {
      // Collect the error and return it via apt_error()
      _error->PushToStack();
      auto exitCode = VerifyDetachedSignatureFile(FileGPG, fd, statusfd);
      std::string msg;
      _error->PopMessage(msg);
      _error->RevertToStack();
      if (exitCode != 0)
      {
	 if (not msg.empty())
	    apt_error(std::cerr, statusfd, fd, "%s", msg.c_str());
	 local_exit(exitCode);
      }
      Args.push_back(FileGPG);
      Args.push_back(File);
   }
   else // clear-signed file
   {
      FileFd signature;
      if (GetTempFile("apt.sig", false, &signature) == nullptr)
	 local_exit(EINTERNAL);
      local_exit.files.push_back(signature.Name());
      FileFd message;
      if (GetTempFile("apt.data", false, &message) == nullptr)
	 local_exit(EINTERNAL);
      local_exit.files.push_back(message.Name());

      if (signature.Failed() || message.Failed() ||
	  not SplitClearSignedFile(File, &message, nullptr, &signature))
      {
	 apt_error(std::cerr, statusfd, fd, "Splitting up %s into data and signature failed", File.c_str());
	 local_exit(112);
      }
      Args.push_back(signature.Name());
      Args.push_back(message.Name());
   }

   if (Debug)
   {
      std::clog << "Preparing to exec: ";
      for (auto const &a : Args)
	 std::clog << " " << a;
      std::clog << std::endl;
   }

   if (statusfd != -1)
   {
      int const nullfd = open("/dev/null", O_WRONLY);
      close(fd[0]);
      // Redirect output to /dev/null; we read from the status fd
      if (statusfd != STDOUT_FILENO)
	 dup2(nullfd, STDOUT_FILENO);
      if (statusfd != STDERR_FILENO)
	 dup2(nullfd, STDERR_FILENO);
      // Redirect the pipe to the status fd (3)
      dup2(fd[1], statusfd);

      putenv((char *)"LANG=");
      putenv((char *)"LC_ALL=");
      putenv((char *)"LC_MESSAGES=");
   }

   // Translate the argument list to a C array. This should happen before
   // the fork so we don't allocate money between fork() and execvp().
   std::vector<const char *> cArgs;
   cArgs.reserve(Args.size() + 1);
   for (auto const &arg : Args)
      cArgs.push_back(arg.c_str());
   cArgs.push_back(nullptr);

   // We have created tempfiles we have to clean up
   // and we do an additional check, so fork yet another time …
   pid_t pid = ExecFork();
   if(pid < 0) {
      apt_error(std::cerr, statusfd, fd, "Fork failed for %s to check %s", Args[0].c_str(), File.c_str());
      local_exit(EINTERNAL);
   }
   if(pid == 0)
   {
      if (statusfd != -1)
	 dup2(fd[1], statusfd);
      execvp(cArgs[0], (char **) &cArgs[0]);
      apt_error(std::cerr, statusfd, fd, "Couldn't execute %s to check %s", Args[0].c_str(), File.c_str());
      local_exit(EINTERNAL);
   }

   // Wait and collect the error code - taken from WaitPid as we need the exact Status
   int Status;
   while (waitpid(pid,&Status,0) != pid)
   {
      if (errno == EINTR)
	 continue;
      apt_error(std::cerr, statusfd, fd, _("Waited for %s but it wasn't there"), gpgv.c_str());
      local_exit(EINTERNAL);
   }

   // check if it exit'ed normally …
   if (not WIFEXITED(Status))
   {
      apt_error(std::cerr, statusfd, fd, _("Sub-process %s exited unexpectedly"), gpgv.c_str());
      local_exit(EINTERNAL);
   }

   // … and with a good exit code
   if (WEXITSTATUS(Status) != 0)
   {
      // we forward the statuscode, so don't generate a message on the fd in this case
      apt_error(std::cerr, -1, fd, _("Sub-process %s returned an error code (%u)"), gpgv.c_str(), WEXITSTATUS(Status));
      local_exit(WEXITSTATUS(Status));
   }

   // everything fine
   local_exit(0);
}
									/*}}}*/
// SplitClearSignedFile - split message into data/signature		/*{{{*/
bool SplitClearSignedFile(std::string const &InFile, FileFd * const ContentFile,
      std::vector<std::string> * const ContentHeader, FileFd * const SignatureFile)
{
   auto in = make_unique_FILE(InFile, "r");
   if (in.get() == nullptr)
      return _error->Errno("fopen", "can not open %s", InFile.c_str());

   struct ScopedErrors
   {
      ScopedErrors() { _error->PushToStack(); }
      ~ScopedErrors() { _error->MergeWithStack(); }
   } scoped;
   LineBuffer buf;

   // start of the message
   if (not buf.readFrom(in.get(), InFile))
      return false; // empty or read error
   if (buf != "-----BEGIN PGP SIGNED MESSAGE-----")
   {
      // this might be an unsigned file we don't want to report errors for,
      // but still finish unsuccessful none the less.
      while (buf.readFrom(in.get(), InFile, true))
	 if (buf == "-----BEGIN PGP SIGNED MESSAGE-----")
	    return _error->Error("Clearsigned file '%s' does not start with a signed message block.", InFile.c_str());

      return false;
   }

   // save "Hash" Armor Headers
   while (true)
   {
      if (not buf.readFrom(in.get(), InFile))
	 return false;
      if (buf.empty())
	 break; // empty line ends the Armor Headers
      if (buf.starts_with("-"))
	 // § 6.2 says unknown keys should be reported to the user. We don't go that far,
	 // but we assume that there will never be a header key starting with a dash
	 return _error->Error("Clearsigned file '%s' contains unexpected line starting with a dash (%s)", InFile.c_str(), "armor");
      if (ContentHeader != nullptr && buf.starts_with("Hash: "))
	 ContentHeader->emplace_back(buf.view());
   }

   // the message itself
   bool first_line = true;
   while (true)
   {
      if (not buf.readFrom(in.get(), InFile))
	 return false;

      if (buf.starts_with("-"))
      {
	 if (buf == "-----BEGIN PGP SIGNATURE-----")
	 {
	    if (not buf.writeLineTo(SignatureFile))
	       return false;
	    break;
	 }
	 else if (buf.starts_with("- "))
	 {
	    // we don't have any fields which need to be dash-escaped,
	    // but implementations are free to escape all lines …
	    if (not buf.writeNewLineIf(ContentFile, not first_line) || not buf.writeTo(ContentFile, 2))
	       return false;
	 }
	 else
	    // § 7.1 says a client should warn, but we don't really work with files which
	    // should contain lines starting with a dash, so it is a lot more likely that
	    // this is an attempt to trick our parser vs. gpgv parser into ignoring a header
	    return _error->Error("Clearsigned file '%s' contains unexpected line starting with a dash (%s)", InFile.c_str(), "msg");
      }
      else if (not buf.writeNewLineIf(ContentFile, not first_line) || not buf.writeTo(ContentFile))
	 return false;
      first_line = false;
   }

   // collect all signatures
   bool open_signature = true;
   while (true)
   {
      if (not buf.readFrom(in.get(), InFile, true))
	 break;

      if (open_signature)
      {
	 if (buf == "-----END PGP SIGNATURE-----")
	    open_signature = false;
	 else if (buf.starts_with("-"))
	    // the used Radix-64 is not using dash for any value, so a valid line can't
	    // start with one. Header keys could, but no existent one does and seems unlikely.
	    // Instead it smells a lot like a header the parser didn't recognize.
	    return _error->Error("Clearsigned file '%s' contains unexpected line starting with a dash (%s)", InFile.c_str(), "sig");
      }
      else //if (not open_signature)
      {
	 if (buf == "-----BEGIN PGP SIGNATURE-----")
	    open_signature = true;
	 else
	    return _error->Error("Clearsigned file '%s' contains unsigned lines.", InFile.c_str());
      }

      if (not buf.writeLineTo(SignatureFile))
	 return false;
   }
   if (open_signature)
      return _error->Error("Signature in file %s wasn't closed", InFile.c_str());

   // Flush the files
   if (SignatureFile != nullptr)
      SignatureFile->Flush();
   if (ContentFile != nullptr)
      ContentFile->Flush();

   // Catch-all for "unhandled" read/sync errors
   if (_error->PendingError())
      return false;
   return true;
}
									/*}}}*/
bool OpenMaybeClearSignedFile(std::string const &ClearSignedFileName, FileFd &MessageFile) /*{{{*/
{
   // Buffered file
   if (GetTempFile("clearsigned.message", true, &MessageFile, true) == nullptr)
      return false;
   if (MessageFile.Failed())
      return _error->Error("Couldn't open temporary file to work with %s", ClearSignedFileName.c_str());

   _error->PushToStack();
   bool const splitDone = SplitClearSignedFile(ClearSignedFileName, &MessageFile, NULL, NULL);
   bool const errorDone = _error->PendingError();
   _error->MergeWithStack();
   if (not splitDone)
   {
      MessageFile.Close();

      if (errorDone)
	 return false;

      // we deal with an unsigned file
      MessageFile.Open(ClearSignedFileName, FileFd::ReadOnly);
   }
   else // clear-signed
   {
      if (not MessageFile.Seek(0))
	 return _error->Errno("lseek", "Unable to seek back in message for file %s", ClearSignedFileName.c_str());
   }

   return not MessageFile.Failed();
}
									/*}}}*/
bool IsAssertedPubKeyAlgo(std::string const &pkstr, std::string const &option) /*{{{*/
{
   auto fullAss = APT::String::Startswith(option, "APT::Key") ? _config->Find(option) : option;
   for (auto &ass : VectorizeString(fullAss, ','))
   {
      if (ass == pkstr)
	 return true;
      // We only implement >= for rsa
      if (APT::String::Startswith(ass, ">=rsa"))
      {
	 if (not APT::String::Startswith(pkstr, "rsa"))
	    continue;
	 if (not std::all_of(ass.begin() + 5, ass.end(), isdigit))
	    return _error->Error("Unrecognized public key specification '%s' in option %s: expect only digits after >=rsa", ass.c_str(), option.c_str());

	 int assBits = std::stoi(ass.substr(5));
	 int pkBits = std::stoi(pkstr.substr(3));

	 if (pkBits >= assBits)
	    return true;

	 continue;
      }
      if (ass.empty())
	 return _error->Error("Empty item in public key assertion string option %s", option.c_str());
      if (not std::all_of(ass.begin(), ass.end(), [](char c)
			  { return isalpha(c) || isdigit(c); }))
	 return _error->Error("Unrecognized public key specification '%s' in option %s", ass.c_str(), option.c_str());
   }
   return false;
}
									/*}}}*/
