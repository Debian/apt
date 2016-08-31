// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: hashes.cc,v 1.1 2001/03/06 07:15:29 jgg Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/hashes.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>

#include <stddef.h>
#include <algorithm>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
									/*}}}*/

const char * HashString::_SupportedHashes[] =
{
   "SHA512", "SHA256", "SHA1", "MD5Sum", "Checksum-FileSize", NULL
};

HashString::HashString()
{
}

HashString::HashString(std::string Type, std::string Hash) : Type(Type), Hash(Hash)
{
}

HashString::HashString(std::string StringedHash)			/*{{{*/
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
   std::string::size_type pos = StringedHash.find(":");
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
      MD5Summation MD5;
      MD5.AddFD(Fd);
      fileHash = (std::string)MD5.Result();
   }
   else if (strcasecmp(Type.c_str(), "SHA1") == 0)
   {
      SHA1Summation SHA1;
      SHA1.AddFD(Fd);
      fileHash = (std::string)SHA1.Result();
   }
   else if (strcasecmp(Type.c_str(), "SHA256") == 0)
   {
      SHA256Summation SHA256;
      SHA256.AddFD(Fd);
      fileHash = (std::string)SHA256.Result();
   }
   else if (strcasecmp(Type.c_str(), "SHA512") == 0)
   {
      SHA512Summation SHA512;
      SHA512.AddFD(Fd);
      fileHash = (std::string)SHA512.Result();
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
      for (auto const &hs: list)
         if (hs.usable())
            return true;
      return false;
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
   std::string size;
   strprintf(size, "%llu", Size);
   return push_back(HashString("Checksum-FileSize", size));
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

// PrivateHashes							/*{{{*/
class PrivateHashes {
public:
   unsigned long long FileSize;
   unsigned int CalcHashes;

   explicit PrivateHashes(unsigned int const CalcHashes) : FileSize(0), CalcHashes(CalcHashes) {}
   explicit PrivateHashes(HashStringList const &Hashes) : FileSize(0) {
      unsigned int calcHashes = Hashes.usable() ? 0 : ~0;
      if (Hashes.find("MD5Sum") != NULL)
	 calcHashes |= Hashes::MD5SUM;
      if (Hashes.find("SHA1") != NULL)
	 calcHashes |= Hashes::SHA1SUM;
      if (Hashes.find("SHA256") != NULL)
	 calcHashes |= Hashes::SHA256SUM;
      if (Hashes.find("SHA512") != NULL)
	 calcHashes |= Hashes::SHA512SUM;
      CalcHashes = calcHashes;
   }
};
									/*}}}*/
// Hashes::Add* - Add the contents of data or FD			/*{{{*/
bool Hashes::Add(const unsigned char * const Data, unsigned long long const Size)
{
   if (Size == 0)
      return true;
   bool Res = true;
APT_IGNORE_DEPRECATED_PUSH
   if ((d->CalcHashes & MD5SUM) == MD5SUM)
      Res &= MD5.Add(Data, Size);
   if ((d->CalcHashes & SHA1SUM) == SHA1SUM)
      Res &= SHA1.Add(Data, Size);
   if ((d->CalcHashes & SHA256SUM) == SHA256SUM)
      Res &= SHA256.Add(Data, Size);
   if ((d->CalcHashes & SHA512SUM) == SHA512SUM)
      Res &= SHA512.Add(Data, Size);
APT_IGNORE_DEPRECATED_POP
   d->FileSize += Size;
   return Res;
}
bool Hashes::Add(const unsigned char * const Data, unsigned long long const Size, unsigned int const Hashes)
{
   d->CalcHashes = Hashes;
   return Add(Data, Size);
}
bool Hashes::AddFD(int const Fd,unsigned long long Size)
{
   unsigned char Buf[64*64];
   bool const ToEOF = (Size == UntilEOF);
   while (Size != 0 || ToEOF)
   {
      unsigned long long n = sizeof(Buf);
      if (!ToEOF) n = std::min(Size, n);
      ssize_t const Res = read(Fd,Buf,n);
      if (Res < 0 || (!ToEOF && Res != (ssize_t) n)) // error, or short read
	 return false;
      if (ToEOF && Res == 0) // EOF
	 break;
      Size -= Res;
      if (Add(Buf, Res) == false)
	 return false;
   }
   return true;
}
bool Hashes::AddFD(int const Fd,unsigned long long Size, unsigned int const Hashes)
{
   d->CalcHashes = Hashes;
   return AddFD(Fd, Size);
}
bool Hashes::AddFD(FileFd &Fd,unsigned long long Size)
{
   unsigned char Buf[64*64];
   bool const ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned long long n = sizeof(Buf);
      if (!ToEOF) n = std::min(Size, n);
      unsigned long long a = 0;
      if (Fd.Read(Buf, n, &a) == false) // error
	 return false;
      if (ToEOF == false)
      {
	 if (a != n) // short read
	    return false;
      }
      else if (a == 0) // EOF
	 break;
      Size -= a;
      if (Add(Buf, a) == false)
	 return false;
   }
   return true;
}
bool Hashes::AddFD(FileFd &Fd,unsigned long long Size, unsigned int const Hashes)
{
   d->CalcHashes = Hashes;
   return AddFD(Fd, Size);
}
									/*}}}*/
HashStringList Hashes::GetHashStringList()
{
   HashStringList hashes;
APT_IGNORE_DEPRECATED_PUSH
   if ((d->CalcHashes & MD5SUM) == MD5SUM)
      hashes.push_back(HashString("MD5Sum", MD5.Result().Value()));
   if ((d->CalcHashes & SHA1SUM) == SHA1SUM)
      hashes.push_back(HashString("SHA1", SHA1.Result().Value()));
   if ((d->CalcHashes & SHA256SUM) == SHA256SUM)
      hashes.push_back(HashString("SHA256", SHA256.Result().Value()));
   if ((d->CalcHashes & SHA512SUM) == SHA512SUM)
      hashes.push_back(HashString("SHA512", SHA512.Result().Value()));
APT_IGNORE_DEPRECATED_POP
   hashes.FileSize(d->FileSize);
   return hashes;
}
APT_IGNORE_DEPRECATED_PUSH
Hashes::Hashes() : d(new PrivateHashes(~0)) { }
Hashes::Hashes(unsigned int const Hashes) : d(new PrivateHashes(Hashes)) {}
Hashes::Hashes(HashStringList const &Hashes) : d(new PrivateHashes(Hashes)) {}
Hashes::~Hashes() { delete d; }
APT_IGNORE_DEPRECATED_POP
