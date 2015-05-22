// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: hashes.h,v 1.2 2001/03/11 05:30:20 jgg Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_HASHES_H
#define APTPKG_HASHES_H


#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>
#include <apt-pkg/macros.h>

#include <cstring>
#include <string>

#ifndef APT_8_CLEANER_HEADERS
using std::min;
using std::vector;
#endif
#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
#include <algorithm>
#include <vector>
#endif


class FileFd;

// helper class that contains hash function name
// and hash
class HashString
{
 protected:
   std::string Type;
   std::string Hash;
   static const char * _SupportedHashes[10];

   // internal helper
   std::string GetHashForFile(std::string filename) const;

 public:
   HashString(std::string Type, std::string Hash);
   HashString(std::string StringedHashString);  // init from str as "type:hash"
   HashString();

   // get hash type used
   std::string HashType() { return Type; };
   std::string HashType() const { return Type; };
   std::string HashValue() const { return Hash; };

   // verify the given filename against the currently loaded hash
   bool VerifyFile(std::string filename) const;

   // generate a hash string from the given filename
   bool FromFile(std::string filename);


   // helper
   std::string toStr() const;                    // convert to str as "type:hash"
   bool empty() const;
   bool operator==(HashString const &other) const;
   bool operator!=(HashString const &other) const;

   // return the list of hashes we support
   static APT_CONST const char** SupportedHashes();
};

class HashStringList
{
   public:
   /** find best hash if no specific one is requested
    *
    * @param type of the checksum to return, can be \b NULL
    * @return If type is \b NULL (or the empty string) it will
    *  return the 'best' hash; otherwise the hash which was
    *  specifically requested. If no hash is found \b NULL will be returned.
    */
   HashString const * find(char const * const type) const;
   HashString const * find(std::string const &type) const { return find(type.c_str()); }
   /** check if the given hash type is supported
    *
    * @param type to check
    * @return true if supported, otherwise false
    */
   static APT_PURE bool supported(char const * const type);
   /** add the given #HashString to the list
    *
    * @param hashString to add
    * @return true if the hash is added because it is supported and
    *  not already a different hash of the same type included, otherwise false
    */
   bool push_back(const HashString &hashString);
   /** @return size of the list of HashStrings */
   size_t size() const { return list.size(); }

   /** take the 'best' hash and verify file with it
    *
    * @param filename to verify
    * @return true if the file matches the hashsum, otherwise false
    */
   bool VerifyFile(std::string filename) const;

   /** is the list empty ?
    *
    * @return \b true if the list is empty, otherwise \b false
    */
   bool empty() const { return list.empty(); }

   typedef std::vector<HashString>::const_iterator const_iterator;

   /** iterator to the first element */
   const_iterator begin() const { return list.begin(); }

   /** iterator to the end element */
   const_iterator end() const { return list.end(); }

   /** start fresh with a clear list */
   void clear() { list.clear(); }

   /** compare two HashStringList for similarity.
    *
    * Two lists are similar if at least one hashtype is in both lists
    * and the hashsum matches. All hashes are checked, if one doesn't
    * match false is returned regardless of how many matched before.
    */
   bool operator==(HashStringList const &other) const;
   bool operator!=(HashStringList const &other) const;

   HashStringList() {}

   // simplifying API-compatibility constructors
   HashStringList(std::string const &hash) {
      if (hash.empty() == false)
	 list.push_back(HashString(hash));
   }
   HashStringList(char const * const hash) {
      if (hash != NULL && hash[0] != '\0')
	 list.push_back(HashString(hash));
   }

   private:
   std::vector<HashString> list;
};

class Hashes
{
   public:

   MD5Summation MD5;
   SHA1Summation SHA1;
   SHA256Summation SHA256;
   SHA512Summation SHA512;
   
   static const int UntilEOF = 0;

   inline bool Add(const unsigned char *Data,unsigned long long Size)
   {
      return MD5.Add(Data,Size) && SHA1.Add(Data,Size) && SHA256.Add(Data,Size) && SHA512.Add(Data,Size);
   };
   inline bool Add(const char *Data) {return Add((unsigned char const *)Data,strlen(Data));};
   inline bool AddFD(int const Fd,unsigned long long Size = 0)
   { return AddFD(Fd, Size, true, true, true, true); };
   bool AddFD(int const Fd, unsigned long long Size, bool const addMD5,
	      bool const addSHA1, bool const addSHA256, bool const addSHA512);
   inline bool AddFD(FileFd &Fd,unsigned long long Size = 0)
   { return AddFD(Fd, Size, true, true, true, true); };
   bool AddFD(FileFd &Fd, unsigned long long Size, bool const addMD5,
	      bool const addSHA1, bool const addSHA256, bool const addSHA512);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
};

#endif
