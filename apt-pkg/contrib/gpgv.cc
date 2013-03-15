// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include<config.h>

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// RunGPGV - returns the command needed for verify			/*{{{*/
// ---------------------------------------------------------------------
/* Generating the commandline for calling gpgv is somehow complicated as
   we need to add multiple keyrings and user supplied options. */
bool ExecGPGV(std::string const &File, std::string const &FileGPG,
			int const &statusfd, int fd[2])
{
   if (File == FileGPG)
   {
      #define SIGMSG "-----BEGIN PGP SIGNED MESSAGE-----\n"
      char buffer[sizeof(SIGMSG)];
      FILE* gpg = fopen(File.c_str(), "r");
      if (gpg == NULL)
	 return _error->Errno("RunGPGV", _("Could not open file %s"), File.c_str());
      char const * const test = fgets(buffer, sizeof(buffer), gpg);
      fclose(gpg);
      if (test == NULL || strcmp(buffer, SIGMSG) != 0)
	 return _error->Error(_("File %s doesn't start with a clearsigned message"), File.c_str());
      #undef SIGMSG
   }


   string const gpgvpath = _config->Find("Dir::Bin::gpg", "/usr/bin/gpgv");
   // FIXME: remove support for deprecated APT::GPGV setting
   string const trustedFile = _config->Find("APT::GPGV::TrustedKeyring", _config->FindFile("Dir::Etc::Trusted"));
   string const trustedPath = _config->FindDir("Dir::Etc::TrustedParts");

   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);

   if (Debug == true)
   {
      std::clog << "gpgv path: " << gpgvpath << std::endl;
      std::clog << "Keyring file: " << trustedFile << std::endl;
      std::clog << "Keyring path: " << trustedPath << std::endl;
   }

   std::vector<string> keyrings;
   if (DirectoryExists(trustedPath))
     keyrings = GetListOfFilesInDir(trustedPath, "gpg", false, true);
   if (RealFileExists(trustedFile) == true)
     keyrings.push_back(trustedFile);

   std::vector<const char *> Args;
   Args.reserve(30);

   if (keyrings.empty() == true)
   {
      // TRANSLATOR: %s is the trusted keyring parts directory
      return _error->Error(_("No keyring installed in %s."),
			   _config->FindDir("Dir::Etc::TrustedParts").c_str());
   }

   Args.push_back(gpgvpath.c_str());
   Args.push_back("--ignore-time-conflict");

   if (statusfd != -1)
   {
      Args.push_back("--status-fd");
      char fd[10];
      snprintf(fd, sizeof(fd), "%i", statusfd);
      Args.push_back(fd);
   }

   for (vector<string>::const_iterator K = keyrings.begin();
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

   Args.push_back(FileGPG.c_str());
   if (FileGPG != File)
      Args.push_back(File.c_str());
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
      dup2(nullfd, STDOUT_FILENO);
      dup2(nullfd, STDERR_FILENO);
      // Redirect the pipe to the status fd (3)
      dup2(fd[1], statusfd);

      putenv((char *)"LANG=");
      putenv((char *)"LC_ALL=");
      putenv((char *)"LC_MESSAGES=");
   }

   execvp(gpgvpath.c_str(), (char **) &Args[0]);
   return true;
}
									/*}}}*/
