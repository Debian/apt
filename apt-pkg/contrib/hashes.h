// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Hashes - Simple wrapper around the hash functions
   
   This is just used to make building the methods simpler, this is the
   only interface required..
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_HASHES_H
#define APTPKG_HASHES_H

#include <apt-pkg/macros.h>

#ifdef APT_COMPILING_APT
#include <apt-pkg/string_view.h>
#include <apt-pkg/tagfile-keys.h>
#endif

#include <cstring>
#include <string>
#include <vector>



class FileFd;

// helper class that contains hash function name
// and hash
class APT_PUBLIC HashString
{
 protected:
   std::string Type;
   std::string Hash;
   static const char * _SupportedHashes[10];

   // internal helper
   std::string GetHashForFile(std::string filename) const;

 public:
   HashString(std::string Type, std::string Hash);
   explicit HashString(std::string StringedHashString);  // init from str as "type:hash"
   HashString();

   // get hash type used
   std::string HashType() const { return Type; };
   std::string HashValue() const { return Hash; };

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
   static APT_PURE const char** SupportedHashes();
#ifdef APT_COMPILING_APT
   struct APT_HIDDEN HashSupportInfo {
      APT::StringView name;
      pkgTagSection::Key namekey;
      APT::StringView chksumsname;
      pkgTagSection::Key chksumskey;
   };
   APT_HIDDEN static std::vector<HashSupportInfo> SupportedHashesInfo();
#endif
};

class APT_PUBLIC HashStringList
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

   /** verify file against all hashes in the list
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
   explicit HashStringList(std::string const &hash) {
      if (hash.empty() == false)
	 list.push_back(HashString(hash));
   }
   explicit HashStringList(char const * const hash) {
      if (hash != NULL && hash[0] != '\0')
	 list.push_back(HashString(hash));
   }

   private:
   std::vector<HashString> list;
};

class PrivateHashes;
class APT_PUBLIC Hashes
{
   PrivateHashes * const d;
   public:
   static const int UntilEOF = 0;

   bool Add(const unsigned char * const Data, unsigned long long const Size) APT_NONNULL(2);
   inline bool Add(const char * const Data) APT_NONNULL(2)
   {return Add(reinterpret_cast<unsigned char const *>(Data),strlen(Data));};
   inline bool Add(const char *const Data, unsigned long long const Size) APT_NONNULL(2)
   {
      return Add(reinterpret_cast<unsigned char const *>(Data), Size);
   };
   inline bool Add(const unsigned char * const Beg,const unsigned char * const End) APT_NONNULL(2,3)
   {return Add(Beg,End-Beg);};

   enum SupportedHashes { MD5SUM = (1 << 0), SHA1SUM = (1 << 1), SHA256SUM = (1 << 2),
      SHA512SUM = (1 << 3) };
   bool AddFD(int const Fd,unsigned long long Size = 0);
   bool AddFD(FileFd &Fd,unsigned long long Size = 0);

   HashStringList GetHashStringList();

   /** Get a specific hash. It is an error to use a hash that was not hashes */
   HashString GetHashString(SupportedHashes hash);

   /** create a Hashes object to calculate all supported hashes
    *
    * If ALL is too much, you can limit which Hashes are calculated
    * with the following other constructors which mention explicitly
    * which hashes to generate. */
   Hashes();
   /** @param Hashes bitflag composed of #SupportedHashes */
   explicit Hashes(unsigned int const Hashes);
   /** @param Hashes is a list of hashes */
   explicit Hashes(HashStringList const &Hashes);
   virtual ~Hashes();
};

#endif
