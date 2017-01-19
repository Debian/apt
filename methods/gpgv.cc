#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include "aptmethod.h"

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>

#include <apti18n.h>

using std::string;
using std::vector;

#define GNUPGPREFIX "[GNUPG:]"
#define GNUPGBADSIG "[GNUPG:] BADSIG"
#define GNUPGERRSIG "[GNUPG:] ERRSIG"
#define GNUPGNOPUBKEY "[GNUPG:] NO_PUBKEY"
#define GNUPGVALIDSIG "[GNUPG:] VALIDSIG"
#define GNUPGGOODSIG "[GNUPG:] GOODSIG"
#define GNUPGEXPKEYSIG "[GNUPG:] EXPKEYSIG"
#define GNUPGEXPSIG "[GNUPG:] EXPSIG"
#define GNUPGREVKEYSIG "[GNUPG:] REVKEYSIG"
#define GNUPGNODATA "[GNUPG:] NODATA"
#define APTKEYWARNING "[APTKEY:] WARNING"
#define APTKEYERROR "[APTKEY:] ERROR"

struct Digest {
   enum class State {
      Untrusted,
      Weak,
      Trusted,
   } state;
   char name[32];

   State getState() const {
      std::string optionUntrusted;
      std::string optionWeak;
      strprintf(optionUntrusted, "APT::Hashes::%s::Untrusted", name);
      strprintf(optionWeak, "APT::Hashes::%s::Weak", name);
      if (_config->FindB(optionUntrusted, false) == true)
	 return State::Untrusted;
      if (_config->FindB(optionWeak, false) == true)
	 return State::Weak;

      return state;
   }
};

static constexpr Digest Digests[] = {
   {Digest::State::Untrusted, "Invalid digest"},
   {Digest::State::Untrusted, "MD5"},
   {Digest::State::Untrusted, "SHA1"},
   {Digest::State::Untrusted, "RIPE-MD/160"},
   {Digest::State::Trusted, "Reserved digest"},
   {Digest::State::Trusted, "Reserved digest"},
   {Digest::State::Trusted, "Reserved digest"},
   {Digest::State::Trusted, "Reserved digest"},
   {Digest::State::Trusted, "SHA256"},
   {Digest::State::Trusted, "SHA384"},
   {Digest::State::Trusted, "SHA512"},
   {Digest::State::Trusted, "SHA224"},
};

static Digest FindDigest(std::string const & Digest)
{
   int id = atoi(Digest.c_str());
   if (id >= 0 && static_cast<unsigned>(id) < _count(Digests)) {
      return Digests[id];
   } else {
      return Digests[0];
   }
}

struct Signer {
   std::string key;
   std::string note;
};
static bool IsTheSameKey(std::string const &validsig, std::string const &goodsig) {
   // VALIDSIG reports a keyid (40 = 24 + 16), GOODSIG is a longid (16) only
   return validsig.compare(24, 16, goodsig, strlen("GOODSIG "), 16) == 0;
}

class GPGVMethod : public aptMethod
{
   private:
   string VerifyGetSigners(const char *file, const char *outfile,
				std::string const &key,
				vector<string> &GoodSigners,
                                vector<string> &BadSigners,
                                vector<string> &WorthlessSigners,
                                vector<Signer> &SoonWorthlessSigners,
				vector<string> &NoPubKeySigners);
   protected:
   virtual bool URIAcquire(std::string const &Message, FetchItem *Itm) APT_OVERRIDE;
   public:
   GPGVMethod() : aptMethod("gpgv","1.0",SingleInstance | SendConfig) {};
};
static void PushEntryWithKeyID(std::vector<std::string> &Signers, char * const buffer, bool const Debug)
{
   char * const msg = buffer + sizeof(GNUPGPREFIX);
   char *p = msg;
   // skip the message
   while (*p && !isspace(*p))
      ++p;
   // skip the separator whitespace
   ++p;
   // skip the hexdigit fingerprint
   while (*p && isxdigit(*p))
      ++p;
   // cut the rest from the message
   *p = '\0';
   if (Debug == true)
      std::clog << "Got " << msg << " !" << std::endl;
   Signers.push_back(msg);
}
static void PushEntryWithUID(std::vector<std::string> &Signers, char * const buffer, bool const Debug)
{
   std::string msg = buffer + sizeof(GNUPGPREFIX);
   auto const nuke = msg.find_last_not_of("\n\t\r");
   if (nuke != std::string::npos)
      msg.erase(nuke + 1);
   if (Debug == true)
      std::clog << "Got " << msg << " !" << std::endl;
   Signers.push_back(msg);
}
string GPGVMethod::VerifyGetSigners(const char *file, const char *outfile,
					 std::string const &key,
					 vector<string> &GoodSigners,
					 vector<string> &BadSigners,
					 vector<string> &WorthlessSigners,
					 vector<Signer> &SoonWorthlessSigners,
					 vector<string> &NoPubKeySigners)
{
   bool const Debug = DebugEnabled();

   if (Debug == true)
      std::clog << "inside VerifyGetSigners" << std::endl;

   int fd[2];
   bool const keyIsID = (key.empty() == false && key[0] != '/');

   if (pipe(fd) < 0)
      return "Couldn't create pipe";

   pid_t pid = fork();
   if (pid < 0)
      return string("Couldn't spawn new process") + strerror(errno);
   else if (pid == 0)
      ExecGPGV(outfile, file, 3, fd, (keyIsID ? "" : key));
   close(fd[1]);

   FILE *pipein = fdopen(fd[0], "r");

   // Loop over the output of apt-key (which really is gnupg), and check the signatures.
   std::vector<std::string> ValidSigners;
   std::vector<std::string> ErrSigners;
   size_t buffersize = 0;
   char *buffer = NULL;
   bool gotNODATA = false;
   while (1)
   {
      if (getline(&buffer, &buffersize, pipein) == -1)
	 break;
      if (Debug == true)
         std::clog << "Read: " << buffer << std::endl;

      // Push the data into three separate vectors, which
      // we later concatenate.  They're kept separate so
      // if we improve the apt method communication stuff later
      // it will be better.
      if (strncmp(buffer, GNUPGBADSIG, sizeof(GNUPGBADSIG)-1) == 0)
	 PushEntryWithUID(BadSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGERRSIG, sizeof(GNUPGERRSIG)-1) == 0)
	 PushEntryWithKeyID(ErrSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGNOPUBKEY, sizeof(GNUPGNOPUBKEY)-1) == 0)
      {
	 PushEntryWithKeyID(NoPubKeySigners, buffer, Debug);
	 ErrSigners.erase(std::remove_if(ErrSigners.begin(), ErrSigners.end(), [&](std::string const &errsig) {
		  return errsig.compare(strlen("ERRSIG "), 16, buffer, sizeof(GNUPGNOPUBKEY), 16) == 0;  }), ErrSigners.end());
      }
      else if (strncmp(buffer, GNUPGNODATA, sizeof(GNUPGNODATA)-1) == 0)
	 gotNODATA = true;
      else if (strncmp(buffer, GNUPGEXPKEYSIG, sizeof(GNUPGEXPKEYSIG)-1) == 0)
	 PushEntryWithUID(WorthlessSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGEXPSIG, sizeof(GNUPGEXPSIG)-1) == 0)
	 PushEntryWithUID(WorthlessSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGREVKEYSIG, sizeof(GNUPGREVKEYSIG)-1) == 0)
	 PushEntryWithUID(WorthlessSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGGOODSIG, sizeof(GNUPGGOODSIG)-1) == 0)
	 PushEntryWithKeyID(GoodSigners, buffer, Debug);
      else if (strncmp(buffer, GNUPGVALIDSIG, sizeof(GNUPGVALIDSIG)-1) == 0)
      {
         std::istringstream iss(buffer + sizeof(GNUPGVALIDSIG));
         vector<string> tokens{std::istream_iterator<string>{iss},
                               std::istream_iterator<string>{}};
         auto const sig = tokens[0];
         // Reject weak digest algorithms
         Digest digest = FindDigest(tokens[7]);
         switch (digest.getState()) {
         case Digest::State::Weak:
            // Treat them like an expired key: For that a message about expiry
            // is emitted, a VALIDSIG, but no GOODSIG.
            SoonWorthlessSigners.push_back({sig, digest.name});
	    if (Debug == true)
	       std::clog << "Got weak VALIDSIG, key ID: " << sig << std::endl;
            break;
         case Digest::State::Untrusted:
            // Treat them like an expired key: For that a message about expiry
            // is emitted, a VALIDSIG, but no GOODSIG.
            WorthlessSigners.push_back(sig);
            GoodSigners.erase(std::remove_if(GoodSigners.begin(), GoodSigners.end(), [&](std::string const &goodsig) {
		     return IsTheSameKey(sig, goodsig); }), GoodSigners.end());
	    if (Debug == true)
	       std::clog << "Got untrusted VALIDSIG, key ID: " << sig << std::endl;
            break;

	 case Digest::State::Trusted:
	    if (Debug == true)
	       std::clog << "Got trusted VALIDSIG, key ID: " << sig << std::endl;
            break;
         }

         ValidSigners.push_back(sig);
      }
      else if (strncmp(buffer, APTKEYWARNING, sizeof(APTKEYWARNING)-1) == 0)
         Warning("%s", buffer + sizeof(APTKEYWARNING));
      else if (strncmp(buffer, APTKEYERROR, sizeof(APTKEYERROR)-1) == 0)
	 _error->Error("%s", buffer + sizeof(APTKEYERROR));
   }
   fclose(pipein);
   free(buffer);
   std::move(ErrSigners.begin(), ErrSigners.end(), std::back_inserter(WorthlessSigners));

   // apt-key has a --keyid parameter, but this requires gpg, so we call it without it
   // and instead check after the fact which keyids where used for verification
   if (keyIsID == true)
   {
      if (Debug == true)
	 std::clog << "GoodSigs needs to be limited to keyid " << key << std::endl;
      bool foundGood = false;
      for (auto const &k: VectorizeString(key, ','))
      {
	 if (std::find(ValidSigners.begin(), ValidSigners.end(), k) == ValidSigners.end())
	    continue;
	 // we look for GOODSIG here as well as an expired sig is a valid sig as well (but not a good one)
	 std::string const goodfingerprint = "GOODSIG " + k;
	 std::string const goodlongkeyid = "GOODSIG " + k.substr(24, 16);
	 foundGood = std::find(GoodSigners.begin(), GoodSigners.end(), goodfingerprint) != GoodSigners.end();
	 if (Debug == true)
	    std::clog << "Key " << k << " is valid sig, is " << goodfingerprint << " also a good one? " << (foundGood ? "yes" : "no") << std::endl;
	 std::string goodsig;
	 if (foundGood == false)
	 {
	    foundGood = std::find(GoodSigners.begin(), GoodSigners.end(), goodlongkeyid) != GoodSigners.end();
	    if (Debug == true)
	       std::clog << "Key " << k << " is valid sig, is " << goodlongkeyid << " also a good one? " << (foundGood ? "yes" : "no") << std::endl;
	    goodsig = goodlongkeyid;
	 }
	 else
	    goodsig = goodfingerprint;
	 if (foundGood == false)
	    continue;
	 std::copy(GoodSigners.begin(), GoodSigners.end(), std::back_insert_iterator<std::vector<std::string> >(NoPubKeySigners));
	 GoodSigners.clear();
	 GoodSigners.push_back(goodsig);
	 NoPubKeySigners.erase(
	    std::remove(NoPubKeySigners.begin(),
	       std::remove(NoPubKeySigners.begin(), NoPubKeySigners.end(), goodfingerprint),
	       goodlongkeyid),
	    NoPubKeySigners.end()
	 );
	 break;
      }
      if (foundGood == false)
      {
	 std::copy(GoodSigners.begin(), GoodSigners.end(), std::back_insert_iterator<std::vector<std::string> >(NoPubKeySigners));
	 GoodSigners.clear();
      }
   }

   int status;
   waitpid(pid, &status, 0);
   if (Debug == true)
   {
      ioprintf(std::clog, "gpgv exited with status %i\n", WEXITSTATUS(status));
   }

   if (Debug)
   {
      std::cerr << "Summary:" << std::endl << "  Good: ";
      std::copy(GoodSigners.begin(), GoodSigners.end(), std::ostream_iterator<std::string>(std::cerr, ", "));
      std::cerr << std::endl << "  Bad: ";
      std::copy(BadSigners.begin(), BadSigners.end(), std::ostream_iterator<std::string>(std::cerr, ", "));
      std::cerr << std::endl << "  Worthless: ";
      std::copy(WorthlessSigners.begin(), WorthlessSigners.end(), std::ostream_iterator<std::string>(std::cerr, ", "));
      std::cerr << std::endl << "  SoonWorthless: ";
      std::for_each(SoonWorthlessSigners.begin(), SoonWorthlessSigners.end(), [](Signer const &sig) { std::cerr << sig.key << ", "; });
      std::cerr << std::endl << "  NoPubKey: ";
      std::copy(NoPubKeySigners.begin(), NoPubKeySigners.end(), std::ostream_iterator<std::string>(std::cerr, ", "));
      std::cerr << std::endl << "  NODATA: " << (gotNODATA ? "yes" : "no") << std::endl;
   }

   if (WEXITSTATUS(status) == 112)
   {
      // acquire system checks for "NODATA" to generate GPG errors (the others are only warnings)
      std::string errmsg;
      //TRANSLATORS: %s is a single techy word like 'NODATA'
      strprintf(errmsg, _("Clearsigned file isn't valid, got '%s' (does the network require authentication?)"), "NODATA");
      return errmsg;
   }
   else if (gotNODATA)
   {
      // acquire system checks for "NODATA" to generate GPG errors (the others are only warnings)
      std::string errmsg;
      //TRANSLATORS: %s is a single techy word like 'NODATA'
      strprintf(errmsg, _("Signed file isn't valid, got '%s' (does the network require authentication?)"), "NODATA");
      return errmsg;
   }
   else if (WEXITSTATUS(status) == 0)
   {
      if (keyIsID)
      {
	 // gpgv will report success, but we want to enforce a certain keyring
	 // so if we haven't found the key the valid we found is in fact invalid
	 if (GoodSigners.empty())
	    return _("At least one invalid signature was encountered.");
      }
      else
      {
	 if (GoodSigners.empty())
	    return _("Internal error: Good signature, but could not determine key fingerprint?!");
      }
      return "";
   }
   else if (WEXITSTATUS(status) == 1)
      return _("At least one invalid signature was encountered.");
   else if (WEXITSTATUS(status) == 111)
      return _("Could not execute 'apt-key' to verify signature (is gnupg installed?)");
   else
      return _("Unknown error executing apt-key");
}

bool GPGVMethod::URIAcquire(std::string const &Message, FetchItem *Itm)
{
   URI const Get = Itm->Uri;
   string const Path = Get.Host + Get.Path; // To account for relative paths
   std::string const key = LookupTag(Message, "Signed-By");
   vector<string> GoodSigners;
   vector<string> BadSigners;
   // a worthless signature is a expired or revoked one
   vector<string> WorthlessSigners;
   vector<Signer> SoonWorthlessSigners;
   vector<string> NoPubKeySigners;
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   // Run apt-key on file, extract contents and get the key ID of the signer
   string const msg = VerifyGetSigners(Path.c_str(), Itm->DestFile.c_str(), key,
                                 GoodSigners, BadSigners, WorthlessSigners,
                                 SoonWorthlessSigners, NoPubKeySigners);
   if (_error->PendingError())
      return false;

   // Check if all good signers are soon worthless and warn in that case
   if (std::all_of(GoodSigners.begin(), GoodSigners.end(), [&](std::string const &good) {
	    return std::any_of(SoonWorthlessSigners.begin(), SoonWorthlessSigners.end(), [&](Signer const &weak) {
		  return IsTheSameKey(weak.key, good);
		  });
	    }))
   {
      for (auto const & Signer : SoonWorthlessSigners)
         // TRANSLATORS: The second %s is the reason and is untranslated for repository owners.
         Warning(_("Signature by key %s uses weak digest algorithm (%s)"), Signer.key.c_str(), Signer.note.c_str());
   }

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
		 I != BadSigners.end(); ++I)
               errmsg += (*I + "\n");
         }
         if (!WorthlessSigners.empty())
         {
            errmsg += _("The following signatures were invalid:\n");
            for (vector<string>::iterator I = WorthlessSigners.begin();
		 I != WorthlessSigners.end(); ++I)
               errmsg += (*I + "\n");
         }
         if (!NoPubKeySigners.empty())
         {
             errmsg += _("The following signatures couldn't be verified because the public key is not available:\n");
            for (vector<string>::iterator I = NoPubKeySigners.begin();
		 I != NoPubKeySigners.end(); ++I)
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
   std::move(BadSigners.begin(), BadSigners.end(), std::back_inserter(Res.GPGVOutput));
   std::move(NoPubKeySigners.begin(), NoPubKeySigners.end(), std::back_inserter(Res.GPGVOutput));
   URIDone(Res);

   if (DebugEnabled())
      std::clog << "apt-key succeeded\n";

   return true;
}


int main()
{
   return GPGVMethod().Run();
}
