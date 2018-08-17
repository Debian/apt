#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/strutl.h>

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
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
   // VALIDSIG reports a fingerprint (40 = 24 + 16), GOODSIG can be longid (16) or
   // fingerprint according to documentation in DETAILS.gz
   if (goodsig.length() == 40 + strlen("GOODSIG "))
      return validsig.compare(0, 40, goodsig, strlen("GOODSIG "), 40) == 0;
   return validsig.compare(24, 16, goodsig, strlen("GOODSIG "), 16) == 0;
}

class GPGVMethod : public aptMethod
{
   private:
   string VerifyGetSigners(const char *file, const char *outfile,
				vector<string> const &keyFpts,
				vector<string> const &keyFiles,
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
					 vector<string> const &keyFpts,
					 vector<string> const &keyFiles,
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

   if (pipe(fd) < 0)
      return "Couldn't create pipe";

   pid_t pid = fork();
   if (pid < 0)
      return string("Couldn't spawn new process") + strerror(errno);
   else if (pid == 0)
   {
      std::ostringstream keys;
      if (keyFiles.empty() == false)
      {
	 std::copy(keyFiles.begin(), keyFiles.end()-1, std::ostream_iterator<std::string>(keys, ","));
	 keys << *keyFiles.rbegin();
      }
      ExecGPGV(outfile, file, 3, fd, keys.str());
   }
   close(fd[1]);

   FILE *pipein = fdopen(fd[0], "r");

   // Loop over the output of apt-key (which really is gnupg), and check the signatures.
   std::vector<std::string> ValidSigners;
   std::vector<std::string> ErrSigners;
   std::map<std::string, std::vector<std::string>> SubKeyMapping;
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

	 if (tokens.size() > 9 && sig != tokens[9])
	    SubKeyMapping[tokens[9]].emplace_back(sig);
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
   if (keyFpts.empty() == false)
   {
      if (Debug == true)
      {
	 std::clog << "GoodSigs needs to be limited to keyid(s): ";
	 std::copy(keyFpts.begin(), keyFpts.end(), std::ostream_iterator<std::string>(std::clog, ", "));
	 std::clog << "\n";
      }
      std::vector<std::string> filteredGood;
      for (auto &&good: GoodSigners)
      {
	 if (Debug == true)
	    std::clog << "Key " << good << " is good sig, is it also a valid and allowed one? ";
	 bool found = false;
	 for (auto l : keyFpts)
	 {
	    bool exactKey = false;
	    if (APT::String::Endswith(l, "!"))
	    {
	       exactKey = true;
	       l.erase(l.length() - 1);
	    }
	    if (IsTheSameKey(l, good))
	    {
	       // GOODSIG might be "just" a longid, so we check VALIDSIG which is always a fingerprint
	       if (std::find(ValidSigners.cbegin(), ValidSigners.cend(), l) == ValidSigners.cend())
		  continue;
	       found = true;
	       break;
	    }
	    else if (exactKey == false)
	    {
	       auto const master = SubKeyMapping.find(l);
	       if (master == SubKeyMapping.end())
		  continue;
	       for (auto const &sub : master->second)
		  if (IsTheSameKey(sub, good))
		  {
		     if (std::find(ValidSigners.cbegin(), ValidSigners.cend(), sub) == ValidSigners.cend())
			continue;
		     found = true;
		     break;
		  }
	       if (found)
		  break;
	    }
	 }
	 if (Debug)
	    std::clog << (found ? "yes" : "no") << "\n";
	 if (found)
	    filteredGood.emplace_back(std::move(good));
	 else
	    NoPubKeySigners.emplace_back(std::move(good));
      }
      GoodSigners = std::move(filteredGood);
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
      if (keyFpts.empty() == false)
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
   vector<string> GoodSigners;
   vector<string> BadSigners;
   // a worthless signature is a expired or revoked one
   vector<string> WorthlessSigners;
   vector<Signer> SoonWorthlessSigners;
   vector<string> NoPubKeySigners;
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   std::vector<std::string> keyFpts, keyFiles;
   for (auto &&key : VectorizeString(LookupTag(Message, "Signed-By"), ','))
      if (key.empty() == false && key[0] == '/')
	 keyFiles.emplace_back(std::move(key));
      else
	 keyFpts.emplace_back(std::move(key));

   // Run apt-key on file, extract contents and get the key ID of the signer
   string const msg = VerifyGetSigners(Path.c_str(), Itm->DestFile.c_str(), keyFpts, keyFiles,
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
