// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.h,v 1.10 1999/03/15 08:10:39 jgg Exp $
/* ######################################################################

   Configuration Class
   
   This class provides a configuration file and command line parser
   for a tree-oriented configuration environment. All runtime configuration
   is stored in here.
   
   Each configuration name is given as a fully scoped string such as
     Foo::Bar
   And has associated with it a text string. The Configuration class only
   provides storage and lookup for this tree, other classes provide
   configuration file formats (and parsers/emitters if needed).

   Most things can get by quite happily with,
     cout << _config->Find("Foo::Bar") << endl;

   A special extension, support for ordered lists is provided by using the
   special syntax, "block::list::" the trailing :: designates the 
   item as a list. To access the list you must use the tree function on
   "block::list".
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CONFIGURATION_H
#define PKGLIB_CONFIGURATION_H

#ifdef __GNUG__
#pragma interface "apt-pkg/configuration.h"
#endif 

#include <string>

class Configuration
{
   struct Item
   {
      string Value;
      string Tag;
      Item *Parent;
      Item *Child;
      Item *Next;
      
      string FullTag() const;
      
      Item() : Child(0), Next(0) {};
   };
   Item *Root;
   
   Item *Lookup(Item *Head,const char *S,unsigned long Len,bool Create);
   Item *Lookup(const char *Name,bool Create);
      
   public:

   string Find(const char *Name,const char *Default = 0);
   string Find(string Name,const char *Default = 0) {return Find(Name.c_str(),Default);};
   string FindFile(const char *Name,const char *Default = 0);
   string FindDir(const char *Name,const char *Default = 0);
   int FindI(const char *Name,int Default = 0);
   int FindI(string Name,bool Default = 0) {return FindI(Name.c_str(),Default);};
   bool FindB(const char *Name,bool Default = false);
   bool FindB(string Name,bool Default = false) {return FindB(Name.c_str(),Default);};
	      
   inline void Set(string Name,string Value) {Set(Name.c_str(),Value);};
   void Set(const char *Name,string Value);
   void Set(const char *Name,int Value);   
   
   inline bool Exists(string Name) {return Exists(Name.c_str());};
   bool Exists(const char *Name);
      
   inline const Item *Tree(const char *Name) {return Lookup(Name,false);};

   void Dump();
   
   Configuration();
};

extern Configuration *_config;

bool ReadConfigFile(Configuration &Conf,string File);

#endif
