#ifndef PKGLIB_VENDOR_H
#define PKGLIB_VENDOR_H
#include <string>
#include <vector>
#include <map>

#ifdef __GNUG__
#pragma interface "apt-pkg/vendor.h"
#endif

using std::string;

// A class representing a particular software provider. 
class Vendor
{
   public:
   struct Fingerprint
   {
      string Print;
      string Description;
   };

   protected:
   string VendorID;
   string Origin;
   std::map<string, string> Fingerprints;

   public:
   Vendor(string VendorID, string Origin,
          std::vector<struct Fingerprint *> *FingerprintList);
   virtual const string& GetVendorID() const { return VendorID; };
   virtual const string LookupFingerprint(string Print) const;
   virtual bool CheckDist(string Dist);
   virtual ~Vendor(){};
};

#endif
