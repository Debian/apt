// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include<config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include<apt-pkg/configuration.h>
#include<apt-pkg/error.h>
#include<apt-pkg/strutl.h>
#include<apt-pkg/fileutl.h>
#include<apt-pkg/gpgv.h>

#include <apti18n.h>
									/*}}}*/
static char * GenerateTemporaryFileTemplate(const char *basename)	/*{{{*/
{
   const char *tmpdir = getenv("TMPDIR");
#ifdef P_tmpdir
   if (!tmpdir)
      tmpdir = P_tmpdir;
#endif
   if (!tmpdir)
      tmpdir = "/tmp";

   std::string out;
   strprintf(out,  "%s/%s.XXXXXX", tmpdir, basename);
   return strdup(out.c_str());
}
									/*}}}*/
// ExecGPGV - returns the command needed for verify			/*{{{*/
// ---------------------------------------------------------------------
/* Generating the commandline for calling gpgv is somehow complicated as
   we need to add multiple keyrings and user supplied options.
   Also, as gpgv has no options to enforce a certain reduced style of
   clear-signed files (=the complete content of the file is signed and
   the content isn't encoded) we do a divide and conquer approach here
*/
void ExecGPGV(std::string const &File, std::string const &FileGPG,
             int const &statusfd, int fd[2])
{
   #define EINTERNAL 111
   std::string const gpgvpath = _config->Find("Dir::Bin::gpg", "/usr/bin/gpgv");
   // FIXME: remove support for deprecated APT::GPGV setting
   std::string const trustedFile = _config->Find("APT::GPGV::TrustedKeyring", _config->FindFile("Dir::Etc::Trusted"));
   std::string const trustedPath = _config->FindDir("Dir::Etc::TrustedParts");

   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);

   if (Debug == true)
   {
      std::clog << "gpgv path: " << gpgvpath << std::endl;
      std::clog << "Keyring file: " << trustedFile << std::endl;
      std::clog << "Keyring path: " << trustedPath << std::endl;
   }

   std::vector<std::string> keyrings;
   if (DirectoryExists(trustedPath))
     keyrings = GetListOfFilesInDir(trustedPath, "gpg", false, true);
   if (RealFileExists(trustedFile) == true)
     keyrings.push_back(trustedFile);

   std::vector<const char *> Args;
   Args.reserve(30);

   if (keyrings.empty() == true)
   {
      // TRANSLATOR: %s is the trusted keyring parts directory
      ioprintf(std::cerr, _("No keyring installed in %s."),
	    _config->FindDir("Dir::Etc::TrustedParts").c_str());
      exit(EINTERNAL);
   }

   Args.push_back(gpgvpath.c_str());
   Args.push_back("--ignore-time-conflict");

   char statusfdstr[10];
   if (statusfd != -1)
   {
      Args.push_back("--status-fd");
      snprintf(statusfdstr, sizeof(statusfdstr), "%i", statusfd);
      Args.push_back(statusfdstr);
   }

   for (std::vector<std::string>::const_iterator K = keyrings.begin();
	K != keyrings.end(); ++K)
   {
      Args.push_back("--keyring");
      Args.push_back(K->c_str());
   }

   Configuration::Item const *Opts;
   Opts = _config->Tree("Acquire::gpgv::Options");
   if (Opts != 0)
   {
      Opts = Opts->Child;
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 Args.push_back(Opts->Value.c_str());
      }
   }

   int sigFd = -1;
   int dataFd = -1;
   std::vector<std::string> dataHeader;
   char * sig = NULL;
   char * data = NULL;

   // file with detached signature
   if (FileGPG != File)
   {
      Args.push_back(FileGPG.c_str());
      Args.push_back(File.c_str());
   }
   else // clear-signed file
   {
      sig = GenerateTemporaryFileTemplate("apt.sig");
      data = GenerateTemporaryFileTemplate("apt.data");
      if (sig == NULL || data == NULL)
      {
	 ioprintf(std::cerr, "Couldn't create tempfiles for splitting up %s", File.c_str());
	 exit(EINTERNAL);
      }

      sigFd = mkstemp(sig);
      dataFd = mkstemp(data);
      int const duppedSigFd = dup(sigFd);
      int const duppedDataFd = dup(dataFd);

      if (dataFd == -1 || sigFd == -1 || duppedDataFd == -1 || duppedSigFd == -1 ||
	    SplitClearSignedFile(File, duppedDataFd, &dataHeader, duppedSigFd) == false)
      {
	 if (dataFd != -1)
	    unlink(sig);
	 if (sigFd != -1)
	    unlink(data);
	 ioprintf(std::cerr, "Splitting up %s into data and signature failed", File.c_str());
	 exit(EINTERNAL);
      }
      lseek(dataFd, 0, SEEK_SET);
      lseek(sigFd, 0, SEEK_SET);
      Args.push_back(sig);
      Args.push_back(data);
   }

   Args.push_back(NULL);

   if (Debug == true)
   {
      std::clog << "Preparing to exec: " << gpgvpath;
      for (std::vector<const char *>::const_iterator a = Args.begin(); *a != NULL; ++a)
	 std::clog << " " << *a;
      std::clog << std::endl;
   }

   if (statusfd != -1)
   {
      int const nullfd = open("/dev/null", O_RDONLY);
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

   if (FileGPG != File)
   {
      execvp(gpgvpath.c_str(), (char **) &Args[0]);
      ioprintf(std::cerr, "Couldn't execute %s to check %s", Args[0], File.c_str());
      exit(EINTERNAL);
   }
   else
   {
//#define UNLINK_EXIT(X) exit(X)
#define UNLINK_EXIT(X) unlink(sig);unlink(data);exit(X)

      // for clear-signed files we have created tempfiles we have to clean up
      // and we do an additional check, so fork yet another time …
      pid_t pid = ExecFork();
      if(pid < 0) {
	 ioprintf(std::cerr, "Fork failed for %s to check %s", Args[0], File.c_str());
	 UNLINK_EXIT(EINTERNAL);
      }
      if(pid == 0)
      {
	 if (statusfd != -1)
	    dup2(fd[1], statusfd);
	 execvp(gpgvpath.c_str(), (char **) &Args[0]);
	 ioprintf(std::cerr, "Couldn't execute %s to check %s", Args[0], File.c_str());
	 UNLINK_EXIT(EINTERNAL);
      }

      // Wait and collect the error code - taken from WaitPid as we need the exact Status
      int Status;
      while (waitpid(pid,&Status,0) != pid)
      {
	 if (errno == EINTR)
	    continue;
	 ioprintf(std::cerr, _("Waited for %s but it wasn't there"), "gpgv");
	 UNLINK_EXIT(EINTERNAL);
      }
#undef UNLINK_EXIT
      // we don't need the files any longer as we have the filedescriptors still open
      unlink(sig);
      unlink(data);
      free(sig);
      free(data);

      // check if it exit'ed normally …
      if (WIFEXITED(Status) == false)
      {
	 ioprintf(std::cerr, _("Sub-process %s exited unexpectedly"), "gpgv");
	 exit(EINTERNAL);
      }

      // … and with a good exit code
      if (WEXITSTATUS(Status) != 0)
      {
	 ioprintf(std::cerr, _("Sub-process %s returned an error code (%u)"), "gpgv", WEXITSTATUS(Status));
	 exit(WEXITSTATUS(Status));
      }

      /* looks like its fine. Our caller will check the status fd,
	 but we construct a good-known clear-signed file without garbage
	 and other non-sense. In a perfect world, we get the same file,
	 but empty lines, trailing whitespaces and stuff makes it inperfect … */
      if (RecombineToClearSignedFile(File, dataFd, dataHeader, sigFd) == false)
      {
	 _error->DumpErrors(std::cerr);
	 exit(EINTERNAL);
      }

      // everything fine, we have a clean file now!
      exit(0);
   }
   exit(EINTERNAL); // unreachable safe-guard
}
									/*}}}*/
// RecombineToClearSignedFile - combine data/signature to message	/*{{{*/
bool RecombineToClearSignedFile(std::string const &OutFile, int const ContentFile,
      std::vector<std::string> const &ContentHeader, int const SignatureFile)
{
   FILE *clean_file = fopen(OutFile.c_str(), "w");
   fputs("-----BEGIN PGP SIGNED MESSAGE-----\n", clean_file);
   for (std::vector<std::string>::const_iterator h = ContentHeader.begin(); h != ContentHeader.end(); ++h)
      fprintf(clean_file, "%s\n", h->c_str());
   fputs("\n", clean_file);

   FILE *data_file = fdopen(ContentFile, "r");
   FILE *sig_file = fdopen(SignatureFile, "r");
   if (data_file == NULL || sig_file == NULL)
   {
      fclose(clean_file);
      return _error->Error("Couldn't open splitfiles to recombine them into %s", OutFile.c_str());
   }
   char *buf = NULL;
   size_t buf_size = 0;
   while (getline(&buf, &buf_size, data_file) != -1)
      fputs(buf, clean_file);
   fclose(data_file);
   fputs("\n", clean_file);
   while (getline(&buf, &buf_size, sig_file) != -1)
      fputs(buf, clean_file);
   fclose(sig_file);
   fclose(clean_file);
   return true;
}
									/*}}}*/
// SplitClearSignedFile - split message into data/signature		/*{{{*/
bool SplitClearSignedFile(std::string const &InFile, int const ContentFile,
      std::vector<std::string> * const ContentHeader, int const SignatureFile)
{
   FILE *in = fopen(InFile.c_str(), "r");
   if (in == NULL)
      return _error->Errno("fopen", "can not open %s", InFile.c_str());

   FILE *out_content = NULL;
   FILE *out_signature = NULL;
   if (ContentFile != -1)
   {
      out_content = fdopen(ContentFile, "w");
      if (out_content == NULL)
      {
	 fclose(in);
	 return _error->Errno("fdopen", "Failed to open file to write content to from %s", InFile.c_str());
      }
   }
   if (SignatureFile != -1)
   {
      out_signature = fdopen(SignatureFile, "w");
      if (out_signature == NULL)
      {
	 fclose(in);
	 if (out_content != NULL)
	    fclose(out_content);
	 return _error->Errno("fdopen", "Failed to open file to write signature to from %s", InFile.c_str());
      }
   }

   bool found_message_start = false;
   bool found_message_end = false;
   bool skip_until_empty_line = false;
   bool found_signature = false;
   bool first_line = true;

   char *buf = NULL;
   size_t buf_size = 0;
   while (getline(&buf, &buf_size, in) != -1)
   {
      _strrstrip(buf);
      if (found_message_start == false)
      {
	 if (strcmp(buf, "-----BEGIN PGP SIGNED MESSAGE-----") == 0)
	 {
	    found_message_start = true;
	    skip_until_empty_line = true;
	 }
      }
      else if (skip_until_empty_line == true)
      {
	 if (strlen(buf) == 0)
	    skip_until_empty_line = false;
	 // save "Hash" Armor Headers, others aren't allowed
	 else if (ContentHeader != NULL && strncmp(buf, "Hash: ", strlen("Hash: ")) == 0)
	    ContentHeader->push_back(buf);
      }
      else if (found_signature == false)
      {
	 if (strcmp(buf, "-----BEGIN PGP SIGNATURE-----") == 0)
	 {
	    found_signature = true;
	    found_message_end = true;
	    if (out_signature != NULL)
	       fprintf(out_signature, "%s\n", buf);
	 }
	 else if (found_message_end == false)
	 {
	    // we are in the message block
	    if(first_line == true) // first line does not need a newline
	    {
	       if (out_content != NULL)
		  fprintf(out_content, "%s", buf);
	       first_line = false;
	    }
	    else if (out_content != NULL)
	       fprintf(out_content, "\n%s", buf);
	 }
      }
      else if (found_signature == true)
      {
	 if (out_signature != NULL)
	    fprintf(out_signature, "%s\n", buf);
	 if (strcmp(buf, "-----END PGP SIGNATURE-----") == 0)
	    found_signature = false; // look for other signatures
      }
      // all the rest is whitespace, unsigned garbage or additional message blocks we ignore
   }
   if (out_content != NULL)
      fclose(out_content);
   if (out_signature != NULL)
      fclose(out_signature);
   fclose(in);

   if (found_signature == true)
      return _error->Error("Signature in file %s wasn't closed", InFile.c_str());

   // if we haven't found any of them, this an unsigned file,
   // so don't generate an error, but splitting was unsuccessful none-the-less
   if (found_message_start == false && found_message_end == false)
      return false;
   // otherwise one missing indicates a syntax error
   else if (found_message_start == false || found_message_end == false)
      return _error->Error("Splitting of file %s failed as it doesn't contain all expected parts", InFile.c_str());

   return true;
}
									/*}}}*/
bool OpenMaybeClearSignedFile(std::string const &ClearSignedFileName, FileFd &MessageFile) /*{{{*/
{
   char * const message = GenerateTemporaryFileTemplate("fileutl.message");
   int const messageFd = mkstemp(message);
   if (messageFd == -1)
   {
      free(message);
      return _error->Errno("mkstemp", "Couldn't create temporary file to work with %s", ClearSignedFileName.c_str());
   }
   // we have the fd, thats enough for us
   unlink(message);
   free(message);

   int const duppedMsg = dup(messageFd);
   if (duppedMsg == -1)
      return _error->Errno("dup", "Couldn't duplicate FD to work with %s", ClearSignedFileName.c_str());

   _error->PushToStack();
   bool const splitDone = SplitClearSignedFile(ClearSignedFileName.c_str(), messageFd, NULL, -1);
   bool const errorDone = _error->PendingError();
   _error->MergeWithStack();
   if (splitDone == false)
   {
      close(duppedMsg);

      if (errorDone == true)
	 return false;

      // we deal with an unsigned file
      MessageFile.Open(ClearSignedFileName, FileFd::ReadOnly);
   }
   else // clear-signed
   {
      if (lseek(duppedMsg, 0, SEEK_SET) < 0)
	 return _error->Errno("lseek", "Unable to seek back in message fd for file %s", ClearSignedFileName.c_str());
      MessageFile.OpenDescriptor(duppedMsg, FileFd::ReadOnly, true);
   }

   return MessageFile.Failed() == false;
}
									/*}}}*/
