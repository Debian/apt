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
   std::string HashType() const { return Type; };
   std::string HashValue() const { return Hash; };
   APT_DEPRECATED std::string HashType() { return Type; };
   APT_DEPRECATED std::string HashValue() { return Hash; };

   // verify the given filename against the currently loaded hash
   bool VerifyFile(std::string filename) const;

   // generate a hash string from the given filename
   bool FromFile(std::string filename);


   // helper
   std::string toStr() const;                    // convert to str as "type:hash"
   bool empty() const;
   bool usable() const;
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

   /** finds the filesize hash and returns it as number
    *
    * @return beware: if the size isn't known we return \b 0 here,
    * just like we would do for an empty file. If that is a problem
    * for you have to get the size manually out of the list.
    */
   unsigned long long FileSize() const;

   /** sets the filesize hash
    *
    * @param Size of the file
    * @return @see #push_back
    */
   bool FileSize(unsigned long long const Size);

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

   /** has the list at least one good entry
    *
    * similar to #empty, but handles forced hashes.
    *
    * @return if no hash is forced, same result as #empty,
    * if one is forced \b true if this has is available, \b false otherwise
    */
   bool usable() const;

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
    * and the hashsum matches. All hashes are checked by default,
    * if one doesn't match false is returned regardless of how many
    * matched before. If a hash is forced, only this hash is compared,
    * all others are ignored.
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

class PrivateHashes;
class Hashes
{
   PrivateHashes * const d;

   public:
   /* those will disappear in the future as it is hard to add new ones this way.
    * Use Add* to build the results and get them via GetHashStringList() instead */
   APT_DEPRECATED MD5Summation MD5;
   APT_DEPRECATED SHA1Summation SHA1;
   APT_DEPRECATED SHA256Summation SHA256;
   APT_DEPRECATED SHA512Summation SHA512;

   static const int UntilEOF = 0;

   bool Add(const unsigned char * const Data, unsigned long long const Size);
   APT_DEPRECATED bool Add(const unsigned char * const Data, unsigned long long const Size, unsigned int const Hashes);
   inline bool Add(const char * const Data)
   {return Add((unsigned char const * const)Data,strlen(Data));};
   inline bool Add(const unsigned char * const Beg,const unsigned char * const End)
   {return Add(Beg,End-Beg);};

   enum SupportedHashes { MD5SUM = (1 << 0), SHA1SUM = (1 << 1), SHA256SUM = (1 << 2),
      SHA512SUM = (1 << 3) };
   bool AddFD(int const Fd,unsigned long long Size = 0);
   APT_DEPRECATED bool AddFD(int const Fd,unsigned long long Size, unsigned int const Hashes);
   bool AddFD(FileFd &Fd,unsigned long long Size = 0);
   APT_DEPRECATED bool AddFD(FileFd &Fd,unsigned long long Size, unsigned int const Hashes);

   HashStringList GetHashStringList();

APT_IGNORE_DEPRECATED_PUSH
   /** create a Hashes object to calculate all supported hashes
    *
    * If ALL is too much, you can limit which Hashes are calculated
    * with the following other constructors which mention explicitly
    * which hashes to generate. */
   Hashes();
   /** @param Hashes bitflag composed of #SupportedHashes */
   Hashes(unsigned int const Hashes);
   /** @param Hashes is a list of hashes */
   Hashes(HashStringList const &Hashes);
   virtual ~Hashes();
APT_IGNORE_DEPRECATED_POP

   private:
   APT_HIDDEN APT_CONST inline unsigned int boolsToFlag(bool const addMD5, bool const addSHA1, bool const addSHA256, bool const addSHA512)
   {
      unsigned int Hashes = ~0;
      if (addMD5 == false) Hashes &= ~MD5SUM;
      if (addSHA1 == false) Hashes &= ~SHA1SUM;
      if (addSHA256 == false) Hashes &= ~SHA256SUM;
      if (addSHA512 == false) Hashes &= ~SHA512SUM;
      return Hashes;
   }

   public:
APT_IGNORE_DEPRECATED_PUSH
   APT_DEPRECATED bool AddFD(int const Fd, unsigned long long Size, bool const addMD5,
	 bool const addSHA1, bool const addSHA256, bool const addSHA512) {
      return AddFD(Fd, Size, boolsToFlag(addMD5, addSHA1, addSHA256, addSHA512));
   };
   APT_DEPRECATED bool AddFD(FileFd &Fd, unsigned long long Size, bool const addMD5,
	 bool const addSHA1, bool const addSHA256, bool const addSHA512) {
      return AddFD(Fd, Size, boolsToFlag(addMD5, addSHA1, addSHA256, addSHA512));
   };
APT_IGNORE_DEPRECATED_POP
};

#endif
