// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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

#include <iostream>
#include <string>
#include <vector>

#include <apt-pkg/macros.h>


class APT_PUBLIC Configuration
{
   public:
   
   struct Item
   {
      std::string Value;
      std::string Tag;
      Item *Parent;
      Item *Child;
      Item *Next;
      
      std::string FullTag(const Item *Stop = 0) const;
      
      Item() : Parent(0), Child(0), Next(0) {};
   };
   
   private:
   
   Item *Root;
   bool ToFree;

   Item *Lookup(Item *Head,const char *S,unsigned long const &Len,bool const &Create);
   Item *Lookup(const char *Name,const bool &Create);
   inline const Item *Lookup(const char *Name) const
   {
      return const_cast<Configuration *>(this)->Lookup(Name,false);
   }  
   
   public:

   std::string Find(const char *Name,const char *Default = 0) const;
   std::string Find(std::string const &Name,const char *Default = 0) const {return Find(Name.c_str(),Default);};
   std::string Find(std::string const &Name, std::string const &Default) const {return Find(Name.c_str(),Default.c_str());};
   std::string FindFile(const char *Name,const char *Default = 0) const;
   std::string FindDir(const char *Name,const char *Default = 0) const;
   /** return a list of child options
    *
    * Options like Acquire::Languages are handled as lists which
    * can be overridden and have a default. For the later two a comma
    * separated list of values is supported.
    *
    * \param Name of the parent node
    * \param Default list of values separated by commas */
   std::vector<std::string> FindVector(const char *Name, std::string const &Default = "", bool const Keys = false) const;
   std::vector<std::string> FindVector(std::string const &Name, std::string const &Default = "", bool const Keys = false) const { return FindVector(Name.c_str(), Default, Keys); };

   int FindI(const char *Name,int const &Default = 0) const;
   int FindI(std::string const &Name,int const &Default = 0) const {return FindI(Name.c_str(),Default);};
   bool FindB(const char *Name,bool const &Default = false) const;
   bool FindB(std::string const &Name,bool const &Default = false) const {return FindB(Name.c_str(),Default);};
   std::string FindAny(const char *Name,const char *Default = 0) const;
	      
   inline void Set(const std::string &Name,const std::string &Value) {Set(Name.c_str(),Value);};
   void CndSet(const char *Name,const std::string &Value);
   void CndSet(const char *Name,const int Value);
   void Set(const char *Name,const std::string &Value);
   void Set(const char *Name,const int &Value);
   
   inline bool Exists(const std::string &Name) const {return Exists(Name.c_str());};
   bool Exists(const char *Name) const;
   bool ExistsAny(const char *Name) const;

   void MoveSubTree(char const * const OldRoot, char const * const NewRoot);

   // clear a whole tree
   void Clear(const std::string &Name);
   void Clear();

   // remove a certain value from a list (e.g. the list of "APT::Keep-Fds")
   void Clear(std::string const &List, std::string const &Value);
   void Clear(std::string const &List, int const &Value);

   inline const Item *Tree(const char *Name) const {return Lookup(Name);};

   inline void Dump() { Dump(std::clog); };
   void Dump(std::ostream& str);
   void Dump(std::ostream& str, char const * const root,
	     char const * const format, bool const emptyValue);

   explicit Configuration(const Item *Root);
   Configuration();
   ~Configuration();

   /** \brief match a string against a configurable list of patterns */
   class MatchAgainstConfig
   {
     std::vector<regex_t *> patterns;
     APT_HIDDEN void clearPatterns();

   public:
     explicit MatchAgainstConfig(char const * Config);
     virtual ~MatchAgainstConfig();

     /** \brief Returns \b true for a string matching one of the patterns */
     bool Match(char const * str) const;
     bool Match(std::string const &str) const { return Match(str.c_str()); };

     /** \brief returns if the matcher setup was successful */
     bool wasConstructedSuccessfully() const { return patterns.empty() == false; }
   };
};

APT_PUBLIC extern Configuration *_config;

APT_PUBLIC bool ReadConfigFile(Configuration &Conf,const std::string &FName,
		    bool const &AsSectional = false,
		    unsigned const &Depth = 0);

APT_PUBLIC bool ReadConfigDir(Configuration &Conf,const std::string &Dir,
		   bool const &AsSectional = false,
		   unsigned const &Depth = 0);

#endif
