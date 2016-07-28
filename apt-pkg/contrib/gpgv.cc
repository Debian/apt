// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include<config.h>

#include<apt-pkg/configuration.h>
#include<apt-pkg/error.h>
#include<apt-pkg/strutl.h>
#include<apt-pkg/fileutl.h>
#include<apt-pkg/gpgv.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stddef.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/
static char * GenerateTemporaryFileTemplate(const char *basename)	/*{{{*/
{
   std::string out;
   std::string tmpdir = GetTempDir();
   strprintf(out,  "%s/%s.XXXXXX", tmpdir.c_str(), basename);
   return strdup(out.c_str());
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
void ExecGPGV(std::string const &File, std::string const &FileGPG,
             int const &statusfd, int fd[2], std::string const &key)
{
   #define EINTERNAL 111
   std::string const aptkey = _config->FindFile("Dir::Bin::apt-key", "/usr/bin/apt-key");

   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);

   std::vector<const char *> Args;
   Args.reserve(10);

   Args.push_back(aptkey.c_str());
   Args.push_back("--quiet");
   Args.push_back("--readonly");
   if (key.empty() == false)
   {
      if (key[0] == '/')
      {
	 Args.push_back("--keyring");
	 Args.push_back(key.c_str());
      }
      else
      {
	 Args.push_back("--keyid");
	 Args.push_back(key.c_str());
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
	 if (Opts->Value.empty() == true)
	    continue;
	 Args.push_back(Opts->Value.c_str());
      }
   }

   enum  { DETACHED, CLEARSIGNED } releaseSignature = (FileGPG != File) ? DETACHED : CLEARSIGNED;
   std::vector<std::string> dataHeader;
   char * sig = NULL;
   char * data = NULL;

   if (releaseSignature == DETACHED)
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
	 ioprintf(std::cerr, "Couldn't create tempfile names for splitting up %s", File.c_str());
	 exit(EINTERNAL);
      }

      int const sigFd = mkstemp(sig);
      int const dataFd = mkstemp(data);
      if (sigFd == -1 || dataFd == -1)
      {
	 if (dataFd != -1)
	    unlink(data);
	 if (sigFd != -1)
	    unlink(sig);
	 ioprintf(std::cerr, "Couldn't create tempfiles for splitting up %s", File.c_str());
	 exit(EINTERNAL);
      }

      FileFd signature;
      signature.OpenDescriptor(sigFd, FileFd::WriteOnly, true);
      FileFd message;
      message.OpenDescriptor(dataFd, FileFd::WriteOnly, true);

      if (signature.Failed() == true || message.Failed() == true ||
	    SplitClearSignedFile(File, &message, &dataHeader, &signature) == false)
      {
	 if (dataFd != -1)
	    unlink(data);
	 if (sigFd != -1)
	    unlink(sig);
	 ioprintf(std::cerr, "Splitting up %s into data and signature failed", File.c_str());
	 exit(112);
      }
      Args.push_back(sig);
      Args.push_back(data);
   }

   Args.push_back(NULL);

   if (Debug == true)
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

   if (releaseSignature == DETACHED)
   {
      execvp(Args[0], (char **) &Args[0]);
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
	 execvp(Args[0], (char **) &Args[0]);
	 ioprintf(std::cerr, "Couldn't execute %s to check %s", Args[0], File.c_str());
	 UNLINK_EXIT(EINTERNAL);
      }

      // Wait and collect the error code - taken from WaitPid as we need the exact Status
      int Status;
      while (waitpid(pid,&Status,0) != pid)
      {
	 if (errno == EINTR)
	    continue;
	 ioprintf(std::cerr, _("Waited for %s but it wasn't there"), "apt-key");
	 UNLINK_EXIT(EINTERNAL);
      }
#undef UNLINK_EXIT
      // we don't need the files any longer
      unlink(sig);
      unlink(data);
      free(sig);
      free(data);

      // check if it exit'ed normally …
      if (WIFEXITED(Status) == false)
      {
	 ioprintf(std::cerr, _("Sub-process %s exited unexpectedly"), "apt-key");
	 exit(EINTERNAL);
      }

      // … and with a good exit code
      if (WEXITSTATUS(Status) != 0)
      {
	 ioprintf(std::cerr, _("Sub-process %s returned an error code (%u)"), "apt-key", WEXITSTATUS(Status));
	 exit(WEXITSTATUS(Status));
      }

      // everything fine
      exit(0);
   }
   exit(EINTERNAL); // unreachable safe-guard
}
									/*}}}*/
// SplitClearSignedFile - split message into data/signature		/*{{{*/
bool SplitClearSignedFile(std::string const &InFile, FileFd * const ContentFile,
      std::vector<std::string> * const ContentHeader, FileFd * const SignatureFile)
{
   FILE *in = fopen(InFile.c_str(), "r");
   if (in == NULL)
      return _error->Errno("fopen", "can not open %s", InFile.c_str());

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
	    if (SignatureFile != NULL)
	    {
	       SignatureFile->Write(buf, strlen(buf));
	       SignatureFile->Write("\n", 1);
	    }
	 }
	 else if (found_message_end == false) // we are in the message block
	 {
	    // we don't have any fields which need dash-escaped,
	    // but implementations are free to encode all lines …
	    char const * dashfree = buf;
	    if (strncmp(dashfree, "- ", 2) == 0)
	       dashfree += 2;
	    if(first_line == true) // first line does not need a newline
	       first_line = false;
	    else if (ContentFile != NULL)
	       ContentFile->Write("\n", 1);
	    else
	       continue;
	    if (ContentFile != NULL)
	       ContentFile->Write(dashfree, strlen(dashfree));
	 }
      }
      else if (found_signature == true)
      {
	 if (SignatureFile != NULL)
	 {
	    SignatureFile->Write(buf, strlen(buf));
	    SignatureFile->Write("\n", 1);
	 }
	 if (strcmp(buf, "-----END PGP SIGNATURE-----") == 0)
	    found_signature = false; // look for other signatures
      }
      // all the rest is whitespace, unsigned garbage or additional message blocks we ignore
   }
   fclose(in);
   if (buf != NULL)
      free(buf);

   if (found_signature == true)
      return _error->Error("Signature in file %s wasn't closed", InFile.c_str());

   // if we haven't found any of them, this an unsigned file,
   // so don't generate an error, but splitting was unsuccessful none-the-less
   if (first_line == true && found_message_start == false && found_message_end == false)
      return false;
   // otherwise one missing indicates a syntax error
   else if (first_line == true || found_message_start == false || found_message_end == false)
     return _error->Error("Splitting of file %s failed as it doesn't contain all expected parts %i %i %i", InFile.c_str(), first_line, found_message_start, found_message_end);

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

   MessageFile.OpenDescriptor(messageFd, FileFd::ReadWrite | FileFd::BufferedWrite, true);
   if (MessageFile.Failed() == true)
      return _error->Error("Couldn't open temporary file to work with %s", ClearSignedFileName.c_str());

   _error->PushToStack();
   bool const splitDone = SplitClearSignedFile(ClearSignedFileName, &MessageFile, NULL, NULL);
   bool const errorDone = _error->PendingError();
   _error->MergeWithStack();
   if (splitDone == false)
   {
      MessageFile.Close();

      if (errorDone == true)
	 return false;

      // we deal with an unsigned file
      MessageFile.Open(ClearSignedFileName, FileFd::ReadOnly);
   }
   else // clear-signed
   {
      if (MessageFile.Seek(0) == false)
	 return _error->Errno("lseek", "Unable to seek back in message for file %s", ClearSignedFileName.c_str());
   }

   return MessageFile.Failed() == false;
}
									/*}}}*/
