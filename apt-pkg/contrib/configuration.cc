// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.cc,v 1.3 1998/07/12 23:58:44 jgg Exp $
/* ######################################################################

   Configuration Class
   
   This class provides a configuration file and command line parser
   for a tree-oriented configuration environment. All runtime configuration
   is stored in here.
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/configuration.h"
#endif
#include <apt-pkg/configuration.h>
#include <strutl.h>

#include <stdio.h>
									/*}}}*/

Configuration *_config = new Configuration;

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
      if ((Res = stringcasecmp(I->Tag.begin(),I->Tag.end(),S,S + Len)) == 0)
	 break;
   
   if (Res == 0)
      return I;
   if (Create == false)
      return 0;
   
   I = new Item;
   I->Tag = string(S,Len);
   I->Next = *Last;
   I->Parent = Head;
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
   {
      if (Default == 0)
	 return string();
      else
	 return Default;
   }
   
   return Itm->Value;
}
									/*}}}*/
// Configuration::FindDir - Find a directory				/*{{{*/
// ---------------------------------------------------------------------
/* Directories are stored as the base dir in the Parent node and the
 */
string Configuration::FindDir(const char *Name,const char *Default = 0)
{
   Item *Itm = Lookup(Name,false);
   if (Itm == 0 || Itm->Value.empty() == true)
   {
      if (Default == 0)
	 return string();
      else
	 return Default;
   }
   
   if (Itm->Value[0] == '/' || Itm->Parent == 0)
      return Itm->Value;
   if (Itm->Parent->Value.end()[-1] == '/')
      return Itm->Parent->Value + Itm->Value;
   else
      return Itm->Parent->Value + '/' + Itm->Value;
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
