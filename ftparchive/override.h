// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: override.h,v 1.2 2001/02/20 07:03:18 jgg Exp $
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
    
class Override
{
   public:
   
   struct Item
   {
      string Priority;
      string Section;
      string OldMaint;
      string NewMaint;
      
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
};
    
#endif
    
