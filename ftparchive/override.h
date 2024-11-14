// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Override

   Store the override file.

   ##################################################################### */
									/*}}}*/
#ifndef OVERRIDE_H
#define OVERRIDE_H

#include <map>
#include <string>

using std::string;
using std::map;

class Override
{
   public:

   struct Item
   {
      string Priority;
      string OldMaint;
      string NewMaint;

      map<string,string> FieldOverride;
      string SwapMaint(std::string_view const &Orig,bool &Failed);
      ~Item() {};
   };

   map<string,Item,std::less<>> Mapping;

   inline Item *GetItem(std::string_view const &Package)
   {
      return GetItem(Package, "");
   }
   Item *GetItem(std::string_view const &Package, std::string_view const &Architecture);

   bool ReadOverride(string const &File,bool const &Source = false);
   bool ReadExtraOverride(string const &File,bool const &Source = false);
};

#endif

