// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/strutl.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

// syntactic sugar to wrap a raw pointer with a custom deleter in a std::unique_ptr
static std::unique_ptr<char, decltype(&free)> make_unique_char(void *const str = nullptr)
{
   return {static_cast<char *>(str), &free};
}
static std::unique_ptr<FILE, decltype(&fclose)> make_unique_FILE(std::string const &filename, char const *const mode)
{
   return {fopen(filename.c_str(), mode), &fclose};
}

class LineBuffer							/*{{{*/
{
   char *buffer = nullptr;
   size_t buffer_size = 0;
   int line_length = 0;
   // a "normal" find_last_not_of returns npos if not found
   int find_last_not_of_length(APT::StringView const bad) const
   {
      for (int result = line_length - 1; result >= 0; --result)
	 if (bad.find(buffer[result]) == APT::StringView::npos)
	    return result + 1;
      return 0;
   }

   public:
   bool empty() const noexcept { return view().empty(); }
   APT::StringView view() const noexcept { return {buffer, static_cast<size_t>(line_length)}; }
   bool starts_with(APT::StringView const start) const { return view().substr(0, start.size()) == start; }

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
static bool operator==(LineBuffer const &buf, APT::StringView const exp) noexcept
{
   return buf.view() == exp;
}
static bool operator!=(LineBuffer const &buf, APT::StringView const exp) noexcept
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
   And as a cherry on the cake, we use our apt-key wrapper to do part
   of the lifting in regards to merging keyrings. Fun for the whole family.
*/
static void APT_PRINTF(4) apt_error(std::ostream &outterm, int const statusfd, int fd[2], const char *format, ...)
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
      auto const errtag = "[APTKEY:] ERROR ";
      outstr << '\n';
      auto const errtext = outstr.str();
      if (not FileFd::Write(fd[1], errtag, strlen(errtag)) ||
	    not FileFd::Write(fd[1], errtext.data(), errtext.size()))
	 outterm << errtext << std::flush;
   }
}
void ExecGPGV(std::string const &File, std::string const &FileGPG,
             int const &statusfd, int fd[2], std::string const &key)
{
   #define EINTERNAL 111
   std::string const aptkey = _config->Find("Dir::Bin::apt-key", CMAKE_INSTALL_FULL_BINDIR "/apt-key");

   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);
   struct exiter {
      std::vector<const char *> files;
      void operator ()(int code) APT_NORETURN {
	 std::for_each(files.begin(), files.end(), unlink);
	 exit(code);
      }
   } local_exit;


   std::vector<const char *> Args;
   Args.reserve(10);

   Args.push_back(aptkey.c_str());
   Args.push_back("--quiet");
   Args.push_back("--readonly");
   auto const keysFileFpr = VectorizeString(key, ',');
   for (auto const &k: keysFileFpr)
   {
      if (unlikely(k.empty()))
	 continue;
      if (k[0] == '/')
      {
	 Args.push_back("--keyring");
	 Args.push_back(k.c_str());
      }
      else
      {
	 Args.push_back("--keyid");
	 Args.push_back(k.c_str());
      }
   }
   Args.push_back("verify");

   char statusfdstr[10];
   if (statusfd != -1)
   {
      Args.push_back("--status-fd");
      snprintf(statusfdstr, sizeof(statusfdstr), "%i", statusfd);
      Args.push_back(statusfdstr);
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
	 Args.push_back(Opts->Value.c_str());
      }
   }

   enum  { DETACHED, CLEARSIGNED } releaseSignature = (FileGPG != File) ? DETACHED : CLEARSIGNED;
   auto sig = make_unique_char();
   auto data = make_unique_char();
   auto conf = make_unique_char();

   // Dump the configuration so apt-key picks up the correct Dir values
   {
      {
	 std::string tmpfile;
	 strprintf(tmpfile, "%s/apt.conf.XXXXXX", GetTempDir().c_str());
	 conf.reset(strdup(tmpfile.c_str()));
      }
      if (conf == nullptr) {
	 apt_error(std::cerr, statusfd, fd, "Couldn't create tempfile names for passing config to apt-key");
	 local_exit(EINTERNAL);
      }
      int confFd = mkstemp(conf.get());
      if (confFd == -1) {
	 apt_error(std::cerr, statusfd, fd, "Couldn't create temporary file %s for passing config to apt-key", conf.get());
	 local_exit(EINTERNAL);
      }
      local_exit.files.push_back(conf.get());

      std::ofstream confStream(conf.get());
      close(confFd);
      _config->Dump(confStream);
      confStream.close();
      setenv("APT_CONFIG", conf.get(), 1);
   }

   // Tell apt-key not to emit warnings
   setenv("APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE", "1", 1);

   if (releaseSignature == DETACHED)
   {
      auto detached = make_unique_FILE(FileGPG, "r");
      if (detached.get() == nullptr)
      {
	 apt_error(std::cerr, statusfd, fd, "Detached signature file '%s' could not be opened", FileGPG.c_str());
	 local_exit(EINTERNAL);
      }
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
	       apt_error(std::cerr, statusfd, fd, "Detached signature file '%s' contains unexpected line starting with a dash", FileGPG.c_str());
	       local_exit(112);
	    }
	 }
	 else //if (not open_signature)
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
      if (found_signatures == 0 && statusfd != -1)
      {
	 auto const errtag = "[GNUPG:] NODATA\n";
	 FileFd::Write(fd[1], errtag, strlen(errtag));
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
	       apt_error(std::cerr, statusfd, fd, "Detached signature file '%s' is in unsupported binary format", FileGPG.c_str());
	       local_exit(112);
	    }
	 }
	 // This is not an attack attempt but a file even gpgv would complain about
	 // likely the result of a paywall which is covered by the gpgv method
	 local_exit(113);
      }
      else if (found_badcontent)
      {
	 apt_error(std::cerr, statusfd, fd, "Detached signature file '%s' contains lines not belonging to a signature", FileGPG.c_str());
	 local_exit(112);
      }
      if (open_signature)
      {
	 apt_error(std::cerr, statusfd, fd, "Detached signature file '%s' contains unclosed signatures", FileGPG.c_str());
	 local_exit(112);
      }

      Args.push_back(FileGPG.c_str());
      Args.push_back(File.c_str());
   }
   else // clear-signed file
   {
      FileFd signature;
      if (GetTempFile("apt.sig", false, &signature) == nullptr)
	 local_exit(EINTERNAL);
      sig.reset(strdup(signature.Name().c_str()));
      local_exit.files.push_back(sig.get());
      FileFd message;
      if (GetTempFile("apt.data", false, &message) == nullptr)
	 local_exit(EINTERNAL);
      data.reset(strdup(message.Name().c_str()));
      local_exit.files.push_back(data.get());

      if (signature.Failed() || message.Failed() ||
	  not SplitClearSignedFile(File, &message, nullptr, &signature))
      {
	 apt_error(std::cerr, statusfd, fd, "Splitting up %s into data and signature failed", File.c_str());
	 local_exit(112);
      }
      Args.push_back(sig.get());
      Args.push_back(data.get());
   }

   Args.push_back(NULL);

   if (Debug)
   {
      std::clog << "Preparing to exec: ";
      for (std::vector<const char *>::const_iterator a = Args.begin(); *a != NULL; ++a)
	 std::clog << " " << *a;
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


   // We have created tempfiles we have to clean up
   // and we do an additional check, so fork yet another time …
   pid_t pid = ExecFork();
   if(pid < 0) {
      apt_error(std::cerr, statusfd, fd, "Fork failed for %s to check %s", Args[0], File.c_str());
      local_exit(EINTERNAL);
   }
   if(pid == 0)
   {
      if (statusfd != -1)
	 dup2(fd[1], statusfd);
      execvp(Args[0], (char **) &Args[0]);
      apt_error(std::cerr, statusfd, fd, "Couldn't execute %s to check %s", Args[0], File.c_str());
      local_exit(EINTERNAL);
   }

   // Wait and collect the error code - taken from WaitPid as we need the exact Status
   int Status;
   while (waitpid(pid,&Status,0) != pid)
   {
      if (errno == EINTR)
	 continue;
      apt_error(std::cerr, statusfd, fd, _("Waited for %s but it wasn't there"), "apt-key");
      local_exit(EINTERNAL);
   }

   // check if it exit'ed normally …
   if (not WIFEXITED(Status))
   {
      apt_error(std::cerr, statusfd, fd, _("Sub-process %s exited unexpectedly"), "apt-key");
      local_exit(EINTERNAL);
   }

   // … and with a good exit code
   if (WEXITSTATUS(Status) != 0)
   {
      // we forward the statuscode, so don't generate a message on the fd in this case
      apt_error(std::cerr, -1, fd, _("Sub-process %s returned an error code (%u)"), "apt-key", WEXITSTATUS(Status));
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
	 ContentHeader->push_back(buf.view().to_string());
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
