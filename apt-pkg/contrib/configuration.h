// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.h,v 1.5 1998/10/20 02:39:27 jgg Exp $
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
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
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
      Item() : Child(0), Next(0) {};
   };
   Item *Root;
   
   Item *Lookup(Item *Head,const char *S,unsigned long Len,bool Create);
   Item *Lookup(const char *Name,bool Create);
      
   public:

   string Find(const char *Name,const char *Default = 0);
   string FindFile(const char *Name,const char *Default = 0);
   string FindDir(const char *Name,const char *Default = 0);
   int FindI(const char *Name,int Default = 0);
   bool FindB(const char *Name,bool Default = false);
	      
   inline void Set(string Name,string Value) {Set(Name.c_str(),Value);};
   void Set(const char *Name,string Value);
   void Set(const char *Name,int Value);   
   
   inline bool Exists(string Name) {return Exists(Name.c_str());};
   bool Exists(const char *Name);
      
   Configuration();
};

extern Configuration *_config;

bool ReadConfigFile(Configuration &Conf,string File);

#endif
