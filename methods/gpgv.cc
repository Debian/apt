#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexcopy.h>
#include <apti18n.h>

#include <utime.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>

#include <vector>

#define GNUPGPREFIX "[GNUPG:]"
#define GNUPGBADSIG "[GNUPG:] BADSIG"
#define GNUPGNOPUBKEY "[GNUPG:] NO_PUBKEY"
#define GNUPGVALIDSIG "[GNUPG:] VALIDSIG"
#define GNUPGGOODSIG "[GNUPG:] GOODSIG"
#define GNUPGKEYEXPIRED "[GNUPG:] KEYEXPIRED"
#define GNUPGREVKEYSIG "[GNUPG:] REVKEYSIG"
#define GNUPGNODATA "[GNUPG:] NODATA"

class GPGVMethod : public pkgAcqMethod
{
   private:
   string VerifyGetSigners(const char *file, const char *outfile,
				vector<string> &GoodSigners, 
                                vector<string> &BadSigners,
                                vector<string> &WorthlessSigners,
				vector<string> &NoPubKeySigners);
   
   protected:
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   GPGVMethod() : pkgAcqMethod("1.0",SingleInstance | SendConfig) {};
};

string GPGVMethod::VerifyGetSigners(const char *file, const char *outfile,
					 vector<string> &GoodSigners,
					 vector<string> &BadSigners,
					 vector<string> &WorthlessSigners,
					 vector<string> &NoPubKeySigners)
{
   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);
   // setup a (empty) stringstream for formating the return value
   std::stringstream ret;
   ret.str("");

   if (Debug == true)
      std::clog << "inside VerifyGetSigners" << std::endl;

   int fd[2];

   if (pipe(fd) < 0)
      return "Couldn't create pipe";

   pid_t pid = fork();
   if (pid < 0)
      return string("Couldn't spawn new process") + strerror(errno);
   else if (pid == 0)
   {
      if (SigVerify::RunGPGV(outfile, file, 3, fd) == false)
      {
	 // TRANSLATOR: %s is the trusted keyring parts directory
	 ioprintf(ret, _("No keyring installed in %s."),
		  _config->FindDir("Dir::Etc::TrustedParts").c_str());
	 return ret.str();
      }
      exit(111);
   }
   close(fd[1]);

   FILE *pipein = fdopen(fd[0], "r");

   // Loop over the output of gpgv, and check the signatures.
   size_t buffersize = 64;
   char *buffer = (char *) malloc(buffersize);
   size_t bufferoff = 0;
   while (1)
   {
      int c;

      // Read a line.  Sigh.
      while ((c = getc(pipein)) != EOF && c != '\n')
      {
         if (bufferoff == buffersize)
            buffer = (char *) realloc(buffer, buffersize *= 2);
         *(buffer+bufferoff) = c;
         bufferoff++;
      }
      if (bufferoff == 0 && c == EOF)
         break;
      *(buffer+bufferoff) = '\0';
      bufferoff = 0;
      if (Debug == true)
         std::clog << "Read: " << buffer << std::endl;

      // Push the data into three separate vectors, which
      // we later concatenate.  They're kept separate so
      // if we improve the apt method communication stuff later
      // it will be better.
      if (strncmp(buffer, GNUPGBADSIG, sizeof(GNUPGBADSIG)-1) == 0)
      {
         if (Debug == true)
            std::clog << "Got BADSIG! " << std::endl;
         BadSigners.push_back(string(buffer+sizeof(GNUPGPREFIX)));
      }
      
      if (strncmp(buffer, GNUPGNOPUBKEY, sizeof(GNUPGNOPUBKEY)-1) == 0)
      {
         if (Debug == true)
            std::clog << "Got NO_PUBKEY " << std::endl;
         NoPubKeySigners.push_back(string(buffer+sizeof(GNUPGPREFIX)));
      }
      if (strncmp(buffer, GNUPGNODATA, sizeof(GNUPGBADSIG)-1) == 0)
      {
         if (Debug == true)
            std::clog << "Got NODATA! " << std::endl;
         BadSigners.push_back(string(buffer+sizeof(GNUPGPREFIX)));
      }
      if (strncmp(buffer, GNUPGKEYEXPIRED, sizeof(GNUPGKEYEXPIRED)-1) == 0)
      {
         if (Debug == true)
            std::clog << "Got KEYEXPIRED! " << std::endl;
         WorthlessSigners.push_back(string(buffer+sizeof(GNUPGPREFIX)));
      }
      if (strncmp(buffer, GNUPGREVKEYSIG, sizeof(GNUPGREVKEYSIG)-1) == 0)
      {
         if (Debug == true)
            std::clog << "Got REVKEYSIG! " << std::endl;
         WorthlessSigners.push_back(string(buffer+sizeof(GNUPGPREFIX)));
      }
      if (strncmp(buffer, GNUPGGOODSIG, sizeof(GNUPGGOODSIG)-1) == 0)
      {
         char *sig = buffer + sizeof(GNUPGPREFIX);
         char *p = sig + sizeof("GOODSIG");
         while (*p && isxdigit(*p)) 
            p++;
         *p = 0;
         if (Debug == true)
            std::clog << "Got GOODSIG, key ID:" << sig << std::endl;
         GoodSigners.push_back(string(sig));
      }
   }
   fclose(pipein);

   int status;
   waitpid(pid, &status, 0);
   if (Debug == true)
   {
      std::clog << "gpgv exited\n";
   }
   
   if (WEXITSTATUS(status) == 0)
   {
      if (GoodSigners.empty())
         return _("Internal error: Good signature, but could not determine key fingerprint?!");
      return "";
   }
   else if (WEXITSTATUS(status) == 1)
   {
      return _("At least one invalid signature was encountered.");
   }
   else if (WEXITSTATUS(status) == 111)
   {
      ioprintf(ret, _("Could not execute 'gpgv' to verify signature (is gpgv installed?)"));
      return ret.str();
   }
   else
   {
      return _("Unknown error executing gpgv");
   }
}

bool GPGVMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths
   string keyID;
   vector<string> GoodSigners;
   vector<string> BadSigners;
   // a worthless signature is a expired or revoked one
   vector<string> WorthlessSigners;
   vector<string> NoPubKeySigners;
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   // Run gpgv on file, extract contents and get the key ID of the signer
   string msg = VerifyGetSigners(Path.c_str(), Itm->DestFile.c_str(),
                                 GoodSigners, BadSigners, WorthlessSigners,
                                 NoPubKeySigners);
   if (GoodSigners.empty() || !BadSigners.empty() || !NoPubKeySigners.empty())
   {
      string errmsg;
      // In this case, something bad probably happened, so we just go
      // with what the other method gave us for an error message.
      if (BadSigners.empty() && WorthlessSigners.empty() && NoPubKeySigners.empty())
         errmsg = msg;
      else
      {
         if (!BadSigners.empty())
         {
            errmsg += _("The following signatures were invalid:\n");
            for (vector<string>::iterator I = BadSigners.begin();
		 I != BadSigners.end(); I++)
               errmsg += (*I + "\n");
         }
         if (!WorthlessSigners.empty())
         {
            errmsg += _("The following signatures were invalid:\n");
            for (vector<string>::iterator I = WorthlessSigners.begin();
		 I != WorthlessSigners.end(); I++)
               errmsg += (*I + "\n");
         }
         if (!NoPubKeySigners.empty())
         {
             errmsg += _("The following signatures couldn't be verified because the public key is not available:\n");
            for (vector<string>::iterator I = NoPubKeySigners.begin();
		 I != NoPubKeySigners.end(); I++)
               errmsg += (*I + "\n");
         }
      }
      // this is only fatal if we have no good sigs or if we have at
      // least one bad signature. good signatures and NoPubKey signatures
      // happen easily when a file is signed with multiple signatures
      if(GoodSigners.empty() or !BadSigners.empty())
      	 return _error->Error("%s", errmsg.c_str());
   }
      
   // Just pass the raw output up, because passing it as a real data
   // structure is too difficult with the method stuff.  We keep it
   // as three separate vectors for future extensibility.
   Res.GPGVOutput = GoodSigners;
   Res.GPGVOutput.insert(Res.GPGVOutput.end(),BadSigners.begin(),BadSigners.end());
   Res.GPGVOutput.insert(Res.GPGVOutput.end(),NoPubKeySigners.begin(),NoPubKeySigners.end());
   URIDone(Res);

   if (_config->FindB("Debug::Acquire::gpgv", false))
   {
      std::clog << "gpgv succeeded\n";
   }

   return true;
}


int main()
{
   setlocale(LC_ALL, "");
   
   GPGVMethod Mth;

   return Mth.Run();
}
