// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.h,v 1.16 2002/11/11 06:55:50 doogie Exp $
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

#include <regex.h>

#include <string>
#include <vector>
#include <iostream>

using std::string;

class Configuration
{
   public:
   
   struct Item
   {
      string Value;
      string Tag;
      Item *Parent;
      Item *Child;
      Item *Next;
      
      string FullTag(const Item *Stop = 0) const;
      
      Item() : Parent(0), Child(0), Next(0) {};
   };
   
   private:
   
   Item *Root;
   bool ToFree;
   
   Item *Lookup(Item *Head,const char *S,unsigned long const &Len,bool const &Create);
   Item *Lookup(const char *Name,const bool &Create);
   inline const Item *Lookup(const char *Name) const
   {
      return ((Configuration *)this)->Lookup(Name,false);
   }  
   
   public:

   string Find(const char *Name,const char *Default = 0) const;
   string Find(string const &Name,const char *Default = 0) const {return Find(Name.c_str(),Default);};
   string Find(string const &Name, string const &Default) const {return Find(Name.c_str(),Default.c_str());};
   string FindFile(const char *Name,const char *Default = 0) const;
   string FindDir(const char *Name,const char *Default = 0) const;
   std::vector<string> FindVector(const char *Name) const;
   std::vector<string> FindVector(string const &Name) const { return FindVector(Name.c_str()); };
   int FindI(const char *Name,int const &Default = 0) const;
   int FindI(string const &Name,int const &Default = 0) const {return FindI(Name.c_str(),Default);};
   bool FindB(const char *Name,bool const &Default = false) const;
   bool FindB(string const &Name,bool const &Default = false) const {return FindB(Name.c_str(),Default);};
   string FindAny(const char *Name,const char *Default = 0) const;
	      
   inline void Set(const string &Name,const string &Value) {Set(Name.c_str(),Value);};
   void CndSet(const char *Name,const string &Value);
   void Set(const char *Name,const string &Value);
   void Set(const char *Name,const int &Value);
   
   inline bool Exists(const string &Name) const {return Exists(Name.c_str());};
   bool Exists(const char *Name) const;
   bool ExistsAny(const char *Name) const;

   // clear a whole tree
   void Clear(const string &Name);

   // remove a certain value from a list (e.g. the list of "APT::Keep-Fds")
   void Clear(string const &List, string const &Value);
   void Clear(string const &List, int const &Value);

   inline const Item *Tree(const char *Name) const {return Lookup(Name);};

   inline void Dump() { Dump(std::clog); };
   void Dump(std::ostream& str);

   Configuration(const Item *Root);
   Configuration();
   ~Configuration();

   /** \brief match a string against a configurable list of patterns */
   class MatchAgainstConfig
   {
     std::vector<regex_t *> patterns;
     void clearPatterns();

   public:
     MatchAgainstConfig(char const * Config);
     virtual ~MatchAgainstConfig();

     /** \brief Returns \b true for a string matching one of the patterns */
     bool Match(char const * str) const;
     bool Match(std::string const &str) const { return Match(str.c_str()); };

     /** \brief returns if the matcher setup was successful */
     bool wasConstructedSuccessfully() const { return patterns.empty() == false; }
   };
};

extern Configuration *_config;

bool ReadConfigFile(Configuration &Conf,const string &FName,
		    bool const &AsSectional = false,
		    unsigned const &Depth = 0);

bool ReadConfigDir(Configuration &Conf,const string &Dir,
		   bool const &AsSectional = false,
		   unsigned const &Depth = 0);

#endif
