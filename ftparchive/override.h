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

#ifdef __GNUG__
#pragma interface "override.h"
#endif

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
   };
   
   map<string,Item> Mapping;
   
   inline Item *GetItem(string Package) 
   {
      map<string,Item>::iterator I = Mapping.find(Package);
      if (I == Mapping.end())
	 return 0;
      return &I->second;
   };
   
   bool ReadOverride(string File,bool Source = false);
   bool ReadExtraOverride(string File,bool Source = false);
};

#endif
    
