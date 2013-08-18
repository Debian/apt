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
#include <string>
#include <iostream>
									/*}}}*/

const char * HashString::_SupportedHashes[] =
{
   "SHA512", "SHA256", "SHA1", "MD5Sum", NULL
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

HashString const * HashStringList::find(char const * const type) const /*{{{*/
{
   if (type == NULL || type[0] == '\0')
   {
      std::string forcedType = _config->Find("Acquire::ForceHash", "");
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
   if (list.empty() == true)
      return false;
   HashString const * const hs = find(NULL);
   if (hs == NULL || hs->VerifyFile(filename) == false)
      return false;
   return true;
}
									/*}}}*/
bool HashStringList::operator==(HashStringList const &other) const	/*{{{*/
{
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

// Hashes::AddFD - Add the contents of the FD				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Hashes::AddFD(int const Fd,unsigned long long Size, bool const addMD5,
		   bool const addSHA1, bool const addSHA256, bool const addSHA512)
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
      if (addMD5 == true)
	 MD5.Add(Buf,Res);
      if (addSHA1 == true)
	 SHA1.Add(Buf,Res);
      if (addSHA256 == true)
	 SHA256.Add(Buf,Res);
      if (addSHA512 == true)
	 SHA512.Add(Buf,Res);
   }
   return true;
}
bool Hashes::AddFD(FileFd &Fd,unsigned long long Size, bool const addMD5,
		   bool const addSHA1, bool const addSHA256, bool const addSHA512)
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
      if (addMD5 == true)
	 MD5.Add(Buf, a);
      if (addSHA1 == true)
	 SHA1.Add(Buf, a);
      if (addSHA256 == true)
	 SHA256.Add(Buf, a);
      if (addSHA512 == true)
	 SHA512.Add(Buf, a);
   }
   return true;
}
									/*}}}*/
