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
      string SwapMaint(string Orig,bool &Failed);
      ~Item() {};
   };
   
   map<string,Item> Mapping;
   
   inline Item *GetItem(string Package) 
   {
      return GetItem(Package, "");
   }
   Item *GetItem(string Package, string Architecture);
   
   bool ReadOverride(string File,bool Source = false);
   bool ReadExtraOverride(string File,bool Source = false);
};

#endif
    
