#ifndef PKGLIB_VENDOR_H
#define PKGLIB_VENDOR_H
#include <string>
#include <vector>
#include <map>

#include <apt-pkg/macros.h>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
#endif

// A class representing a particular software provider.
class __deprecated Vendor
{
   public:
   struct Fingerprint
   {
      std::string Print;
      std::string Description;
   };

   protected:
   std::string VendorID;
   std::string Origin;
   std::map<std::string, std::string> Fingerprints;

   public:
   Vendor(std::string VendorID, std::string Origin,
          std::vector<struct Fingerprint *> *FingerprintList);
   virtual const std::string& GetVendorID() const { return VendorID; };
   virtual const std::string LookupFingerprint(std::string Print) const;
   virtual bool CheckDist(std::string Dist);
   virtual ~Vendor(){};
};

#endif
