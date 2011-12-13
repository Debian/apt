// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: vendorlist.h,v 1.1.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   VendorList - Manage a list of vendors
   
   The Vendor List class provides access to a list of vendors and
   attributes associated with them, read from a configuration file.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_VENDORLIST_H
#define PKGLIB_VENDORLIST_H

#include <string>
#include <vector>
#include <apt-pkg/macros.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/vendor.h>
#include <apt-pkg/configuration.h>
using std::string;
using std::vector;
#endif

class Vendor;
class Configuration;

class __deprecated pkgVendorList
{
   protected:
   std::vector<Vendor const *> VendorList;

   bool CreateList(Configuration& Cnf);
   const Vendor* LookupFingerprint(std::string Fingerprint);

   public:
   typedef std::vector<Vendor const *>::const_iterator const_iterator;
   bool ReadMainList();
   bool Read(std::string File);

   // List accessors
   inline const_iterator begin() const {return VendorList.begin();};
   inline const_iterator end() const {return VendorList.end();};
   inline unsigned int size() const {return VendorList.size();};
   inline bool empty() const {return VendorList.empty();};

   const Vendor* FindVendor(const std::vector<std::string> GPGVOutput);

   ~pkgVendorList();
};

#endif
