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
#include <apt-pkg/hashes.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
    
#include <unistd.h>    
#include <system.h>    
#include <string>
#include <iostream>
									/*}}}*/

const char* HashString::_SupportedHashes[] = 
{
   "SHA256", "SHA1", "MD5Sum", NULL
};

HashString::HashString()
{
}

HashString::HashString(string Type, string Hash) : Type(Type), Hash(Hash)
{
}

HashString::HashString(string StringedHash)
{
   // legacy: md5sum without "MD5Sum:" prefix
   if (StringedHash.find(":") == string::npos && StringedHash.size() == 32)
   {
      Type = "MD5Sum";
      Hash = StringedHash;
      return;
   }
   string::size_type pos = StringedHash.find(":");
   Type = StringedHash.substr(0,pos);
   Hash = StringedHash.substr(pos+1, StringedHash.size() - pos);

   if(_config->FindB("Debug::Hashes",false) == true)
      std::clog << "HashString(string): " << Type << " : " << Hash << std::endl;
}


bool HashString::VerifyFile(string filename) const
{
   FileFd fd;
   MD5Summation MD5;
   SHA1Summation SHA1;
   SHA256Summation SHA256;
   string fileHash;

   FileFd Fd(filename, FileFd::ReadOnly);
   if(Type == "MD5Sum") 
   {
      MD5.AddFD(Fd.Fd(), Fd.Size());
      fileHash = (string)MD5.Result();
   } 
   else if (Type == "SHA1")
   {
      SHA1.AddFD(Fd.Fd(), Fd.Size());
      fileHash = (string)SHA1.Result();
   } 
   else if (Type == "SHA256") 
   {
      SHA256.AddFD(Fd.Fd(), Fd.Size());
      fileHash = (string)SHA256.Result();
   }
   Fd.Close();

   if(_config->FindB("Debug::Hashes",false) == true)
      std::clog << "HashString::VerifyFile: got: " << fileHash << " expected: " << toStr() << std::endl;

   return (fileHash == Hash);
}

const char** HashString::SupportedHashes()
{
   return _SupportedHashes;
}

bool HashString::empty() const
{
   return (Type.empty() || Hash.empty());
}


string HashString::toStr() const
{
   return Type+string(":")+Hash;
}


// Hashes::AddFD - Add the contents of the FD				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Hashes::AddFD(int Fd,unsigned long Size)
{
   unsigned char Buf[64*64];
   int Res = 0;
   while (Size != 0)
   {
      Res = read(Fd,Buf,min(Size,(unsigned long)sizeof(Buf)));
      if (Res < 0 || (unsigned)Res != min(Size,(unsigned long)sizeof(Buf)))
	 return false;
      Size -= Res;
      MD5.Add(Buf,Res);
      SHA1.Add(Buf,Res);
      SHA256.Add(Buf,Res);
   }
   return true;
}
									/*}}}*/

