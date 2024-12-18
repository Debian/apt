// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Hashes - Simple wrapper around the hash functions

   This is just used to make building the methods simpler, this is the
   only interface required..

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile-keys.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <unistd.h>

#ifdef HAVE_GNUTLS
#include <gnutls/crypto.h>
#else
#include <gcrypt.h>
#endif
									/*}}}*/

const char * HashString::_SupportedHashes[] =
{
   "SHA512", "SHA256", "SHA1", "MD5Sum", "Checksum-FileSize", NULL
};
std::vector<HashString::HashSupportInfo> HashString::SupportedHashesInfo()
{
   return {{
      { "SHA512",  pkgTagSection::Key::SHA512,"Checksums-Sha512", pkgTagSection::Key::Checksums_Sha512},
      { "SHA256", pkgTagSection::Key::SHA256, "Checksums-Sha256", pkgTagSection::Key::Checksums_Sha256},
      { "SHA1", pkgTagSection::Key::SHA1, "Checksums-Sha1", pkgTagSection::Key::Checksums_Sha1 },
      { "MD5Sum", pkgTagSection::Key::MD5sum, "Files", pkgTagSection::Key::Files },
   }};
}

HashString::HashString()
{
}

HashString::HashString(std::string Type, std::string Hash) : Type(Type), Hash(Hash)
{
}

#if APT_PKG_ABI > 600
HashString::HashString(std::string_view StringedHash)			/*{{{*/
#else
HashString::HashString(std::string StringedHash)        /*{{{*/
#endif
{
   if (StringedHash.find(":") == std::string::npos)
   {
      // legacy: md5sum without "MD5Sum:" prefix
      if (StringedHash.size() == 32)
      {
	 Type = "MD5Sum";
	 Hash = StringedHash;
      }
      if(_config->FindB("Debug::Hashes",false) == true)
	 std::clog << "HashString(string): invalid StringedHash " << StringedHash << std::endl;
      return;
   }
   auto pos = StringedHash.find(":");
   Type = StringedHash.substr(0,pos);
   Hash = StringedHash.substr(pos+1, StringedHash.size() - pos);

   if(_config->FindB("Debug::Hashes",false) == true)
      std::clog << "HashString(string): " << Type << " : " << Hash << std::endl;
}
									/*}}}*/
bool HashString::VerifyFile(std::string filename) const			/*{{{*/
{
   std::string fileHash = GetHashForFile(filename);

   if(_config->FindB("Debug::Hashes",false) == true)
      std::clog << "HashString::VerifyFile: got: " << fileHash << " expected: " << toStr() << std::endl;

   return (fileHash == Hash);
}
									/*}}}*/
bool HashString::FromFile(std::string filename)          		/*{{{*/
{
   // pick the strongest hash
   if (Type == "")
      Type = _SupportedHashes[0];

   Hash = GetHashForFile(filename);
   return true;
}
									/*}}}*/
std::string HashString::GetHashForFile(std::string filename) const      /*{{{*/
{
   std::string fileHash;

   FileFd Fd(filename, FileFd::ReadOnly);
   if(strcasecmp(Type.c_str(), "MD5Sum") == 0)
   {
      Hashes MD5(Hashes::MD5SUM);
      MD5.AddFD(Fd);
      fileHash = MD5.GetHashString(Hashes::MD5SUM).Hash;
   }
   else if (strcasecmp(Type.c_str(), "SHA1") == 0)
   {
      Hashes SHA1(Hashes::SHA1SUM);
      SHA1.AddFD(Fd);
      fileHash = SHA1.GetHashString(Hashes::SHA1SUM).Hash;
   }
   else if (strcasecmp(Type.c_str(), "SHA256") == 0)
   {
      Hashes SHA256(Hashes::SHA256SUM);
      SHA256.AddFD(Fd);
      fileHash = SHA256.GetHashString(Hashes::SHA256SUM).Hash;
   }
   else if (strcasecmp(Type.c_str(), "SHA512") == 0)
   {
      Hashes SHA512(Hashes::SHA512SUM);
      SHA512.AddFD(Fd);
      fileHash = SHA512.GetHashString(Hashes::SHA512SUM).Hash;
   }
   else if (strcasecmp(Type.c_str(), "Checksum-FileSize") == 0)
      strprintf(fileHash, "%llu", Fd.FileSize());
   Fd.Close();

   return fileHash;
}
									/*}}}*/
const char** HashString::SupportedHashes()				/*{{{*/
{
   return _SupportedHashes;
}
									/*}}}*/
APT_PURE bool HashString::empty() const					/*{{{*/
{
   return (Type.empty() || Hash.empty());
}
									/*}}}*/

APT_PURE static bool IsConfigured(const char *name, const char *what)
{
   std::string option;
   strprintf(option, "APT::Hashes::%s::%s", name, what);
   return _config->FindB(option, false);
}

APT_PURE bool HashString::usable() const				/*{{{*/
{
   return (
      (Type != "Checksum-FileSize") &&
      (Type != "MD5Sum") &&
      (Type != "SHA1") &&
      !IsConfigured(Type.c_str(), "Untrusted")
   );
}
									/*}}}*/
std::string HashString::toStr() const					/*{{{*/
{
   return Type + ":" + Hash;
}
									/*}}}*/
APT_PURE bool HashString::operator==(HashString const &other) const	/*{{{*/
{
   return (strcasecmp(Type.c_str(), other.Type.c_str()) == 0 && Hash == other.Hash);
}
APT_PURE bool HashString::operator!=(HashString const &other) const
{
   return !(*this == other);
}
									/*}}}*/

bool HashStringList::usable() const					/*{{{*/
{
   if (empty() == true)
      return false;
   std::string const forcedType = _config->Find("Acquire::ForceHash", "");
   if (forcedType.empty() == true)
   {
      // See if there is at least one usable hash
      return std::any_of(list.begin(), list.end(), [](auto const &hs) { return hs.usable(); });
   }
   return find(forcedType) != NULL;
}
									/*}}}*/
HashString const * HashStringList::find(char const * const type) const /*{{{*/
{
   if (type == NULL || type[0] == '\0')
   {
      std::string const forcedType = _config->Find("Acquire::ForceHash", "");
      if (forcedType.empty() == false)
	 return find(forcedType.c_str());
      for (char const * const * t = HashString::SupportedHashes(); *t != NULL; ++t)
	 for (std::vector<HashString>::const_iterator hs = list.begin(); hs != list.end(); ++hs)
	    if (strcasecmp(hs->HashType().c_str(), *t) == 0)
	       return &*hs;
      return NULL;
   }
   for (std::vector<HashString>::const_iterator hs = list.begin(); hs != list.end(); ++hs)
      if (strcasecmp(hs->HashType().c_str(), type) == 0)
	 return &*hs;
   return NULL;
}
									/*}}}*/
unsigned long long HashStringList::FileSize() const			/*{{{*/
{
   HashString const * const hsf = find("Checksum-FileSize");
   if (hsf == NULL)
      return 0;
   std::string const hv = hsf->HashValue();
   return strtoull(hv.c_str(), NULL, 10);
}
									/*}}}*/
bool HashStringList::FileSize(unsigned long long const Size)		/*{{{*/
{
   return push_back(HashString("Checksum-FileSize", std::to_string(Size)));
}
									/*}}}*/
bool HashStringList::supported(char const * const type)			/*{{{*/
{
   for (char const * const * t = HashString::SupportedHashes(); *t != NULL; ++t)
      if (strcasecmp(*t, type) == 0)
	 return true;
   return false;
}
									/*}}}*/
bool HashStringList::push_back(const HashString &hashString)		/*{{{*/
{
   if (hashString.HashType().empty() == true ||
	 hashString.HashValue().empty() == true ||
	 supported(hashString.HashType().c_str()) == false)
      return false;

   // ensure that each type is added only once
   HashString const * const hs = find(hashString.HashType().c_str());
   if (hs != NULL)
      return *hs == hashString;

   list.push_back(hashString);
   return true;
}
									/*}}}*/
bool HashStringList::VerifyFile(std::string filename) const		/*{{{*/
{
   if (usable() == false)
      return false;

   Hashes hashes(*this);
   FileFd file(filename, FileFd::ReadOnly);
   HashString const * const hsf = find("Checksum-FileSize");
   if (hsf != NULL)
   {
      std::string fileSize;
      strprintf(fileSize, "%llu", file.FileSize());
      if (hsf->HashValue() != fileSize)
	 return false;
   }
   hashes.AddFD(file);
   HashStringList const hsl = hashes.GetHashStringList();
   return hsl == *this;
}
									/*}}}*/
bool HashStringList::operator==(HashStringList const &other) const	/*{{{*/
{
   std::string const forcedType = _config->Find("Acquire::ForceHash", "");
   if (forcedType.empty() == false)
   {
      HashString const * const hs = find(forcedType);
      HashString const * const ohs = other.find(forcedType);
      if (hs == NULL || ohs == NULL)
	 return false;
      return *hs == *ohs;
   }
   short matches = 0;
   for (const_iterator hs = begin(); hs != end(); ++hs)
   {
      HashString const * const ohs = other.find(hs->HashType());
      if (ohs == NULL)
	 continue;
      if (*hs != *ohs)
	 return false;
      ++matches;
   }
   if (matches == 0)
      return false;
   return true;
}
bool HashStringList::operator!=(HashStringList const &other) const
{
   return !(*this == other);
}
									/*}}}*/
static APT_PURE std::string HexDigest(std::basic_string_view<unsigned char> const &Sum)
{
   char Conv[16] =
      {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
       'c', 'd', 'e', 'f'};
   std::string Result(Sum.size() * 2, 0);

   // Convert each char into two letters
   size_t J = 0;
   size_t I = 0;
   for (; I != (Sum.size()) * 2; J++, I += 2)
   {
      Result[I] = Conv[Sum[J] >> 4];
      Result[I + 1] = Conv[Sum[J] & 0xF];
   }
   return Result;
};

// PrivateHashes							/*{{{*/
class PrivateHashes
{
   public:
   unsigned long long FileSize{0};

   private:
#ifdef HAVE_GNUTLS
   std::array<std::optional<gnutls_hash_hd_t>, 4> digs{};

   public:
   struct HashAlgo
   {
      size_t index;
      const char *name;
      gnutls_digest_algorithm_t gnuTlsAlgo;
      Hashes::SupportedHashes ourAlgo;
   };

   static constexpr std::array<HashAlgo, 4> Algorithms{
      HashAlgo{0, "MD5Sum", GNUTLS_DIG_MD5, Hashes::MD5SUM},
      HashAlgo{1, "SHA1", GNUTLS_DIG_SHA1, Hashes::SHA1SUM},
      HashAlgo{2, "SHA256", GNUTLS_DIG_SHA256, Hashes::SHA256SUM},
      HashAlgo{3, "SHA512", GNUTLS_DIG_SHA512, Hashes::SHA512SUM},
   };

   bool Write(unsigned char const *Data, size_t Size)
   {
      for (auto &dig : digs)
      {
	 if (dig)
	    gnutls_hash(*dig, Data, Size);
      }
      return true;
   }

   std::string HexDigest(HashAlgo const &algo)
   {
      auto Size = gnutls_hash_get_len(algo.gnuTlsAlgo);
      unsigned char Sum[Size];
      if (auto copy = gnutls_hash_copy(*digs[algo.index]))
	 gnutls_hash_deinit(copy, &Sum);
      return ::HexDigest(std::basic_string_view<unsigned char>(Sum, Size));
   }
   bool Enable(HashAlgo const &algo)
   {
      digs[algo.index].emplace();
      if (gnutls_hash_init(&*digs[algo.index], algo.gnuTlsAlgo) == 0)
	 return true;
      digs[algo.index] = std::nullopt;
      return false;
   }
   bool IsEnabled(HashAlgo const &algo)
   {
      return bool{digs[algo.index]};
   }

   explicit PrivateHashes() {}
   ~PrivateHashes()
   {
      for (auto &dig : digs)
      {
	 if (dig)
	    gnutls_hash_deinit(*dig, nullptr);
      }
   }
#else
   gcry_md_hd_t hd;

   void maybeInit()
   {

      // Yikes, we got to initialize libgcrypt, or we get warnings. But we
      // abstract away libgcrypt in Hashes from our users - they are not
      // supposed to know what the hashing backend is, so we can't force
      // them to init themselves as libgcrypt folks want us to. So this
      // only leaves us with this option...
      if (!gcry_control(GCRYCTL_INITIALIZATION_FINISHED_P))
      {
	 if (!gcry_check_version(nullptr))
	 {
	    fprintf(stderr, "libgcrypt is too old (need %s, have %s)\n",
		    "nullptr", gcry_check_version(NULL));
	    exit(2);
	 }

	 gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
      }
   }

   public:
   struct HashAlgo
   {
      const char *name;
      int gcryAlgo;
      Hashes::SupportedHashes ourAlgo;
   };

   static constexpr std::array<HashAlgo, 4> Algorithms{
      HashAlgo{"MD5Sum", GCRY_MD_MD5, Hashes::MD5SUM},
      HashAlgo{"SHA1", GCRY_MD_SHA1, Hashes::SHA1SUM},
      HashAlgo{"SHA256", GCRY_MD_SHA256, Hashes::SHA256SUM},
      HashAlgo{"SHA512", GCRY_MD_SHA512, Hashes::SHA512SUM},
   };

   bool Write(unsigned char const *Data, size_t Size)
   {
      gcry_md_write(hd, Data, Size);
      return true;
   }

   std::string HexDigest(HashAlgo const &algo)
   {
      auto Size = gcry_md_get_algo_dlen(algo.gcryAlgo);
      auto Sum = gcry_md_read(hd, algo.gcryAlgo);
      return ::HexDigest(std::basic_string_view<unsigned char>(Sum, Size));
   }

   bool Enable(HashAlgo const &Algo)
   {
      gcry_md_enable(hd, Algo.gcryAlgo);
      return true;
   }
   bool IsEnabled(HashAlgo const &algo)
   {
      return gcry_md_is_enabled(hd, algo.gcryAlgo);
   }

   explicit PrivateHashes()
   {
      maybeInit();
      gcry_md_open(&hd, 0, 0);
   }

   ~PrivateHashes()
   {
      gcry_md_close(hd);
   }
#endif

   explicit PrivateHashes(unsigned int const CalcHashes) : PrivateHashes()
   {
      for (auto & Algo : Algorithms)
      {
	 if ((CalcHashes & Algo.ourAlgo) == Algo.ourAlgo)
	    Enable(Algo);
      }
   }

   explicit PrivateHashes(HashStringList const &Hashes) : PrivateHashes()
   {
      for (auto & Algo : Algorithms)
      {
	 if (not Hashes.usable() || Hashes.find(Algo.name) != NULL)
	    Enable(Algo);
      }
   }
};
									/*}}}*/
// Hashes::Add* - Add the contents of data or FD			/*{{{*/
bool Hashes::Add(const unsigned char * const Data, unsigned long long const Size)
{
   if (Size != 0)
   {
      if (not d->Write(Data, Size))
	 return false;
      d->FileSize += Size;
   }
   return true;
}
bool Hashes::AddFD(int const Fd,unsigned long long Size)
{
   std::array<unsigned char, APT_BUFFER_SIZE> Buf;
   bool const ToEOF = (Size == UntilEOF);
   while (Size != 0 || ToEOF)
   {
      decltype(Size) n = Buf.size();
      if (!ToEOF) n = std::min(Size, n);
      ssize_t const Res = read(Fd,Buf.data(),n);
      if (Res < 0 || (!ToEOF && Res != (ssize_t) n)) // error, or short read
	 return false;
      if (ToEOF && Res == 0) // EOF
	 break;
      Size -= Res;
      if (Add(Buf.data(), Res) == false)
	 return false;
   }
   return true;
}
bool Hashes::AddFD(FileFd &Fd,unsigned long long Size)
{
   std::array<unsigned char, APT_BUFFER_SIZE> Buf;
   bool const ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      decltype(Size) n = Buf.size();
      if (!ToEOF) n = std::min(Size, n);
      decltype(Size) a = 0;
      if (Fd.Read(Buf.data(), n, &a) == false) // error
	 return false;
      if (ToEOF == false)
      {
	 if (a != n) // short read
	    return false;
      }
      else if (a == 0) // EOF
	 break;
      Size -= a;
      if (Add(Buf.data(), a) == false)
	 return false;
   }
   return true;
}
									/*}}}*/

HashStringList Hashes::GetHashStringList()
{
   HashStringList hashes;
   for (auto &Algo : d->Algorithms)
      if (d->IsEnabled(Algo))
	 hashes.push_back(HashString(Algo.name, d->HexDigest(Algo)));
   hashes.FileSize(d->FileSize);

   return hashes;
}

HashString Hashes::GetHashString(SupportedHashes hash)
{
   for (auto &Algo : d->Algorithms)
      if (hash == Algo.ourAlgo)
	 return HashString(Algo.name, d->HexDigest(Algo));

   abort();
}
Hashes::Hashes() : d(new PrivateHashes(~0)) { }
Hashes::Hashes(unsigned int const Hashes) : d(new PrivateHashes(Hashes)) {}
Hashes::Hashes(HashStringList const &Hashes) : d(new PrivateHashes(Hashes)) {}
Hashes::~Hashes() { delete d; }
