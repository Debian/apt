// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.cc,v 1.1 1998/07/07 04:17:10 jgg Exp $
/* ######################################################################

   Configuration Class
   
   This class provides a configuration file and command line parser
   for a tree-oriented configuration environment. All runtime configuration
   is stored in here.
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "pkglib/configuration.h"
#endif
#include <pkglib/configuration.h>
#include <strutl.h>

#include <stdio.h>
									/*}}}*/
Configuration *_config;

// Configuration::Configuration - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
Configuration::Configuration()
{
   Root = new Item;
}
									/*}}}*/
// Configuration::Lookup - Lookup a single item									/*{{{*/
// ---------------------------------------------------------------------
/* This will lookup a single item by name below another item. It is a 
   helper function for the main lookup function */
Configuration::Item *Configuration::Lookup(Item *Head,const char *S,
					   unsigned long Len,bool Create)
{
   int Res = 1;
   Item *I = Head->Child;
   Item **Last = &Head->Child;
   for (; I != 0; Last = &I->Next, I = I->Next)
      if ((Res = stringcasecmp(I->Value.begin(),I->Value.end(),S,S + Len)) == 0)
	 break;

   if (Res == 0)
      return I;
   if (Create == false)
      return 0;
   
   I = new Item;
   I->Value = string(S,Len);
   I->Next = *Last;
   *Last = I;
   return I;
}
									/*}}}*/
// Configuration::Lookup - Lookup a fully scoped item			/*{{{*/
// ---------------------------------------------------------------------
/* This performs a fully scoped lookup of a given name, possibly creating
   new items */
Configuration::Item *Configuration::Lookup(const char *Name,bool Create)
{
   const char *Start = Name;
   const char *End = Start + strlen(Name);
   const char *TagEnd = Name;
   Item *Itm = Root;
   for (; End - TagEnd > 2; TagEnd++)
   {
      if (TagEnd[0] == ':' && TagEnd[1] == ':')
      {
	 Itm = Lookup(Itm,Start,TagEnd - Start,Create);
	 if (Itm == 0)
	    return 0;
	 TagEnd = Start = TagEnd + 2;	 
      }
   }   

   Itm = Lookup(Itm,Start,End - Start,Create);
   if (Itm == 0)
      return 0;
   return Itm;
}
									/*}}}*/
// Configuration::Find - Find a value					/*{{{*/
// ---------------------------------------------------------------------
/* */
string Configuration::Find(const char *Name,const char *Default)
{
   Item *Itm = Lookup(Name,false);
   if (Itm == 0 || Itm->Value.empty() == true)
      return Default;
   return Itm->Value;
}
									/*}}}*/
// Configuration::FindI - Find an integer value				/*{{{*/
// ---------------------------------------------------------------------
/* */
int Configuration::FindI(const char *Name,int Default)
{
   Item *Itm = Lookup(Name,false);
   if (Itm == 0 || Itm->Value.empty() == true)
      return Default;
   
   char *End;
   int Res = strtol(Itm->Value.c_str(),&End,0);
   if (End == Itm->Value.c_str())
      return Default;
   
   return Res;
}
									/*}}}*/
// Configuration::Set - Set a value					/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Set(const char *Name,string Value)
{
   Item *Itm = Lookup(Name,true);
   if (Itm == 0)
      return;
   Itm->Value = Value;
}
									/*}}}*/
// Configuration::Set - Set an integer value				/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Set(const char *Name,int Value)
{
   Item *Itm = Lookup(Name,true);
   if (Itm == 0)
      return;
   char S[300];
   snprintf(S,sizeof(S),"%i",Value);
   Itm->Value = S;
}
									/*}}}*/
