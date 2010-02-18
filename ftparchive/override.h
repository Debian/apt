// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: override.h,v 1.4 2001/06/26 02:50:27 jgg Exp $
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
      string SwapMaint(string const &Orig,bool &Failed);
      ~Item() {};
   };
   
   map<string,Item> Mapping;
   
   inline Item *GetItem(string const &Package) 
   {
      return GetItem(Package, "");
   }
   Item *GetItem(string const &Package, string const &Architecture);
   
   bool ReadOverride(string const &File,bool const &Source = false);
   bool ReadExtraOverride(string const &File,bool const &Source = false);
};

#endif
    
