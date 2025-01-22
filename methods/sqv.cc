#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/gpgv.h>
#include <apt-pkg/strutl.h>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>

using std::string;
using std::vector;

class SQVMethod : public aptMethod
{
   private:
   std::optional<std::string> policy{};
   void SetPolicy();
   bool VerifyGetSigners(const char *file, const char *outfile,
			 vector<string> keyFiles,
			 vector<string> &signers);

   protected:
   bool URIAcquire(std::string const &Message, FetchItem *Itm) override;

   public:
   SQVMethod();
};

SQVMethod::SQVMethod() : aptMethod("sqv", "1.1", SingleInstance | SendConfig | SendURIEncoded)
{
}

void SQVMethod::SetPolicy()
{
   constexpr const char *policies[] = {
      // APT overrides
      "APT_SEQUOIA_CRYPTO_POLICY",
      "/etc/crypto-policies/back-ends/apt-sequoia.config",
      "/var/lib/crypto-config/profiles/current/apt-sequoia.config",
      // Sequoia overrides
      "SEQUOIA_CRYPTO_POLICY",
      "/etc/crypto-policies/back-ends/sequoia.config",
      "/var/lib/crypto-config/profiles/current/sequoia.config",
      // Fallback APT defaults
      "/usr/share/apt/default-sequoia.config",
   };

   if (policy)
      return;

   policy = "";

   for (auto policy : policies)
   {
      if (not strchr(policy, '/'))
      {
	 if (auto value = getenv(policy))
	 {
	    this->policy = value;
	    break;
	 }
      }
      else if (FileExists(policy))
      {
	 this->policy = policy;
	 break;
      }
   }

   if (not policy->empty())
   {
      if (DebugEnabled())
	 std::clog << "Setting SEQUOIA_CRYPTO_POLICY=" << *policy << std::endl;
      setenv("SEQUOIA_CRYPTO_POLICY", policy->c_str(), 1);
   }
}
bool SQVMethod::VerifyGetSigners(const char *file, const char *outfile,
				 vector<string> keyFiles,
				 vector<string> &signers)
{
   bool const Debug = DebugEnabled();

   std::vector<std::string> args;

   SetPolicy();

   args.push_back(SQV_EXECUTABLE);
   auto dearmorKeyOrCheckFormat = [&](std::string const &k) -> bool
   {
      _error->PushToStack();
      FileFd keyFd(k, FileFd::ReadOnly);
      _error->RevertToStack();
      if (not keyFd.IsOpen())
	 return _error->Warning("The key(s) in the keyring %s are ignored as the file is not readable by user executing gpgv.\n", k.c_str());
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

	 return true;
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
	 return true;
      }
   err:
      return _error->Warning("The key(s) in the keyring %s are ignored as the file has an unsupported filetype.", k.c_str());
   };
   if (keyFiles.empty())
   {
      // Either trusted or trustedparts must exist
      _error->PushToStack();
      auto Parts = GetListOfFilesInDir(_config->FindDir("Dir::Etc::TrustedParts"), std::vector<std::string>{"gpg", "asc"}, true);
      if (auto trusted = _config->FindFile("Dir::Etc::Trusted"); not trusted.empty())
      {
	 std::string s;
	 strprintf(s, "Loading %s from deprecated option Dir::Etc::Trusted\n", trusted.c_str());
	 Warning(std::move(s));
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
	 if (struct stat st; stat(Part.c_str(), &st) != 0 || st.st_size == 0)
	    continue;
	 if (not dearmorKeyOrCheckFormat(Part))
	 {
	    std::string msg;
	    _error->PopMessage(msg);
	    if (not msg.empty())
	       Warning(std::move(msg));
	    continue;
	 }
	 keyFiles.push_back(Part);
      }
   }

   if (keyFiles.empty())
      return _error->Error("The signatures couldn't be verified because no keyring is specified");

   for (auto const &keyring : keyFiles)
   {
      args.push_back("--keyring");
      args.push_back(keyring);
   }

   FileFd signatureFd;
   FileFd messageFd;
   DEFER([&]
	 {
	 if (signatureFd.IsOpen()) RemoveFile("RemoveSignature", signatureFd.Name());
	 if (messageFd.IsOpen()) RemoveFile("RemoveMessage", messageFd.Name()); });

   if (strcmp(file, outfile) == 0)
   {
      if (GetTempFile("apt.sig", false, &signatureFd) == nullptr)
	 return false;
      if (GetTempFile("apt.data", false, &messageFd) == nullptr)
	 return false;

      // FIXME: The test suite only expects the final message.
      _error->PushToStack();
      if (signatureFd.Failed() || messageFd.Failed() ||
	  not SplitClearSignedFile(file, &messageFd, nullptr, &signatureFd))
	 return _error->RevertToStack(), _error->Error("Splitting up %s into data and signature failed", file);
      _error->RevertToStack();

      args.push_back(signatureFd.Name());
      args.push_back(messageFd.Name());
   }
   else
   {
      if (not VerifyDetachedSignatureFile(file))
	 return false;
      args.push_back(file);
      args.push_back(outfile);
   }

   // FIXME: Use a select() loop
   FileFd sqvout;
   FileFd sqverr;
   if (GetTempFile("apt.sqvout", false, &sqvout) == nullptr)
      return "Internal error: Cannot create temporary file";

   DEFER([&]
	 { RemoveFile("CleanSQVOut", sqvout.Name()); });

   if (GetTempFile("apt.sqverr", false, &sqverr) == nullptr)
      return "Internal error: Cannot create temporary file";

   DEFER([&]
	 { RemoveFile("CleanSQVErr", sqverr.Name()); });

   // Translate the argument list to a C array. This should happen before
   // the fork so we don't allocate money between fork() and execvp().
   if (Debug)
      std::clog << "Executing " << APT::String::Join(args, " ") << std::endl;
   std::vector<const char *> cArgs;
   cArgs.reserve(args.size() + 1);
   for (auto const &arg : args)
      cArgs.push_back(arg.c_str());
   cArgs.push_back(nullptr);
   pid_t pid = ExecFork({sqvout.Fd(), sqverr.Fd()});
   if (pid < 0)
      return _error->Errno("VerifyGetSigners", "Couldn't spawn new process");
   else if (pid == 0)
   {
      dup2(sqvout.Fd(), STDOUT_FILENO);
      dup2(sqverr.Fd(), STDERR_FILENO);
      execvp(cArgs[0], (char **)&cArgs[0]);
      _exit(123);
   }

   int status;
   waitpid(pid, &status, 0);
   sqverr.Seek(0);
   sqvout.Seek(0);

   if (Debug == true)
      ioprintf(std::clog, "sqv exited with status %i\n", WEXITSTATUS(status));
   if (WEXITSTATUS(status) != 0)
   {
      std::string msg;
      for (std::string err; sqverr.ReadLine(err);)
	 msg.append(err).append("\n");
      return _error->Error(_("Sub-process %s returned an error code (%u), error message is:\n%s"), cArgs[0], WEXITSTATUS(status), msg.c_str());
   }

   for (std::string signer; sqvout.ReadLine(signer);)
   {
      if (Debug)
	 std::clog << "Got GOODSIG " << signer << std::endl;
      signers.push_back(signer);
   }

   return true;
}

static std::string GenerateKeyFile(std::string const key)
{
   FileFd fd;
   GetTempFile("apt-key.XXXXXX.asc", false, &fd);
   fd.Write(key.data(), key.size());
   return fd.Name();
}

bool SQVMethod::URIAcquire(std::string const &Message, FetchItem *Itm)
{
   // Quick safety check: do we have left-over errors from a previous URL?
   if (unlikely(_error->PendingError()))
      return _error->Error("Internal error: Error set at start of verification");

   URI const Get(Itm->Uri);
   std::string const Path = DecodeSendURI(Get.Host + Get.Path); // To account for relative paths

   std::vector<std::string> Signers, keyFpts, keyFiles;
   struct TemporaryFile
   {
      std::string name = "";
      ~TemporaryFile() { RemoveFile("~TemporaryFile", name); }
   } tmpKey;

   std::string SignedBy = DeQuoteString(LookupTag(Message, "Signed-By"));

   if (SignedBy.find("-----BEGIN PGP PUBLIC KEY BLOCK-----") != std::string::npos)
   {
      tmpKey.name = GenerateKeyFile(SignedBy);
      keyFiles.emplace_back(tmpKey.name);
   }
   else
   {
      for (auto &&key : VectorizeString(SignedBy, ','))
	 if (key.empty() == false && key[0] == '/')
	    keyFiles.emplace_back(std::move(key));
	 else
	    keyFpts.emplace_back(std::move(key));
   }

   // Nothing should have failed here in the setup, if it did, don't bother verifying
   if (_error->PendingError())
      return false;

   // Run sqv on file, extract contents and get the key ID of the signer
   VerifyGetSigners(Path.c_str(), Itm->DestFile.c_str(), keyFiles, Signers);
   if (Signers.empty())
      return _error->PendingError() ? false : _error->Error("No good signature");

   if (not keyFpts.empty())
   {
      Signers.erase(std::remove_if(Signers.begin(), Signers.end(), [&](std::string const &signer)
				   {
	 bool allowedSigner = std::find(keyFpts.begin(), keyFpts.end(), signer) != keyFpts.end();
	 if (not allowedSigner && DebugEnabled())
	       std::cerr << "NoPubKey: GOODSIG " << signer << "\n";
	 return not allowedSigner; }),
		    Signers.end());

      if (Signers.empty())
      {
	 if (keyFpts.size() > 1)
	    return _error->Error(_("No good signature from required signers: %s"), APT::String::Join(keyFpts, ", ").c_str());
	 return _error->Error(_("No good signature from required signer: %s"), APT::String::Join(keyFpts, ", ").c_str());
      }
   }
   std::unordered_map<std::string, std::string> fields;
   fields.emplace("URI", Itm->Uri);
   fields.emplace("Filename", Itm->DestFile);
   fields.emplace("Signed-By", APT::String::Join(Signers, "\n"));
   SendMessage("201 URI Done", std::move(fields));
   Dequeue();

   if (DebugEnabled())
      std::clog << "sqv succeeded\n";

   // If we have a pending error somehow, we should still fail here...
   return not _error->PendingError();
}

int main()
{
   return SQVMethod().Run();
}
