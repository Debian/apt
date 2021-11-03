// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Configuration Class
   
   This class provides a configuration file and command line parser
   for a tree-oriented configuration environment. All runtime configuration
   is stored in here.

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@debian.org>.
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/string_view.h>

#include <ctype.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <apti18n.h>

using namespace std;
									/*}}}*/

Configuration *_config = new Configuration;

/* TODO: This config verification shouldn't be using a static variable
   but a Cnf-member – but that would need ABI breaks and stuff and for now
   that really is an apt-dev-only tool, so it isn't that bad that it is
   unusable and allaround a bit strange */
enum class APT_HIDDEN ConfigType { UNDEFINED, INT, BOOL, STRING, STRING_OR_BOOL, STRING_OR_LIST, FILE, DIR, LIST, PROGRAM_PATH = FILE };
APT_HIDDEN std::unordered_map<std::string, ConfigType> apt_known_config {};
static std::string getConfigTypeString(ConfigType const type)		/*{{{*/
{
   switch (type)
   {
      case ConfigType::UNDEFINED: return "UNDEFINED";
      case ConfigType::INT: return "INT";
      case ConfigType::BOOL: return "BOOL";
      case ConfigType::STRING: return "STRING";
      case ConfigType::STRING_OR_BOOL: return "STRING_OR_BOOL";
      case ConfigType::FILE: return "FILE";
      case ConfigType::DIR: return "DIR";
      case ConfigType::LIST: return "LIST";
      case ConfigType::STRING_OR_LIST: return "STRING_OR_LIST";
   }
   return "UNKNOWN";
}
									/*}}}*/
static ConfigType getConfigType(std::string const &type)		/*{{{*/
{
   if (type == "<INT>")
      return ConfigType::INT;
   else if (type == "<BOOL>")
      return ConfigType::BOOL;
   else if (type == "<STRING>")
      return ConfigType::STRING;
   else if (type == "<STRING_OR_BOOL>")
      return ConfigType::STRING_OR_BOOL;
   else if (type == "<FILE>")
      return ConfigType::FILE;
   else if (type == "<DIR>")
      return ConfigType::DIR;
   else if (type == "<LIST>")
      return ConfigType::LIST;
   else if (type == "<STRING_OR_LIST>")
      return ConfigType::STRING_OR_LIST;
   else if (type == "<PROGRAM_PATH>")
      return ConfigType::PROGRAM_PATH;
   return ConfigType::UNDEFINED;
}
									/*}}}*/
// checkFindConfigOptionType - workhorse of option checking		/*{{{*/
static void checkFindConfigOptionTypeInternal(std::string name, ConfigType const type)
{
   std::transform(name.begin(), name.end(), name.begin(), ::tolower);
   auto known = apt_known_config.find(name);
   if (known == apt_known_config.cend())
   {
      auto const rcolon = name.rfind(':');
      if (rcolon != std::string::npos)
      {
	 known = apt_known_config.find(name.substr(0, rcolon) + ":*");
	 if (known == apt_known_config.cend())
	 {
	    auto const parts = StringSplit(name, "::");
	    size_t psize = parts.size();
	    if (psize > 1)
	    {
	       for (size_t max = psize; max != 1; --max)
	       {
		  std::ostringstream os;
		  std::copy(parts.begin(), parts.begin() + max, std::ostream_iterator<std::string>(os, "::"));
		  os << "**";
		  known = apt_known_config.find(os.str());
		  if (known != apt_known_config.cend() && known->second == ConfigType::UNDEFINED)
		     return;
	       }
	       for (size_t max = psize - 1; max != 1; --max)
	       {
		  std::ostringstream os;
		  std::copy(parts.begin(), parts.begin() + max - 1, std::ostream_iterator<std::string>(os, "::"));
		  os << "*::";
		  std::copy(parts.begin() + max + 1, parts.end() - 1, std::ostream_iterator<std::string>(os, "::"));
		  os << *(parts.end() - 1);
		  known = apt_known_config.find(os.str());
		  if (known != apt_known_config.cend())
		     break;
	       }
	    }
	 }
      }
   }
   if (known == apt_known_config.cend())
      _error->Warning("Using unknown config option »%s« of type %s",
	    name.c_str(), getConfigTypeString(type).c_str());
   else if (known->second != type)
   {
      if (known->second == ConfigType::DIR && type == ConfigType::FILE)
	 ; // implementation detail
      else if (type == ConfigType::STRING && (known->second == ConfigType::FILE || known->second == ConfigType::DIR))
	 ; // TODO: that might be an error or not, we will figure this out later
      else if (known->second == ConfigType::STRING_OR_BOOL && (type == ConfigType::BOOL || type == ConfigType::STRING))
	 ;
      else if (known->second == ConfigType::STRING_OR_LIST && (type == ConfigType::LIST || type == ConfigType::STRING))
	 ;
      else
	 _error->Warning("Using config option »%s« of type %s as a type %s",
	       name.c_str(), getConfigTypeString(known->second).c_str(), getConfigTypeString(type).c_str());
   }
}
static void checkFindConfigOptionType(char const * const name, ConfigType const type)
{
   if (apt_known_config.empty())
      return;
   checkFindConfigOptionTypeInternal(name, type);
}
									/*}}}*/
static bool LoadConfigurationIndex(std::string const &filename)		/*{{{*/
{
   apt_known_config.clear();
   if (filename.empty())
      return true;
   Configuration Idx;
   if (ReadConfigFile(Idx, filename) == false)
      return false;

   Configuration::Item const * Top = Idx.Tree(nullptr);
   if (unlikely(Top == nullptr))
      return false;

   do {
      if (Top->Value.empty() == false)
      {
	 std::string fulltag = Top->FullTag();
	 std::transform(fulltag.begin(), fulltag.end(), fulltag.begin(), ::tolower);
	 apt_known_config.emplace(std::move(fulltag), getConfigType(Top->Value));
      }

      if (Top->Child != nullptr)
      {
	 Top = Top->Child;
	 continue;
      }

      while (Top != nullptr && Top->Next == nullptr)
	 Top = Top->Parent;
      if (Top != nullptr)
	 Top = Top->Next;
   } while (Top != nullptr);

   return true;
}
									/*}}}*/

// Configuration::Configuration - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
Configuration::Configuration() : ToFree(true)
{
   Root = new Item;
}
Configuration::Configuration(const Item *Root) : Root((Item *)Root), ToFree(false)
{
}
									/*}}}*/
// Configuration::~Configuration - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
Configuration::~Configuration()
{
   if (ToFree == false)
      return;
   
   Item *Top = Root;
   for (; Top != 0;)
   {
      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }
            
      while (Top != 0 && Top->Next == 0)
      {
	 Item *Parent = Top->Parent;
	 delete Top;
	 Top = Parent;
      }      
      if (Top != 0)
      {
	 Item *Next = Top->Next;
	 delete Top;
	 Top = Next;
      }
   }
}
									/*}}}*/
// Configuration::Lookup - Lookup a single item				/*{{{*/
// ---------------------------------------------------------------------
/* This will lookup a single item by name below another item. It is a 
   helper function for the main lookup function */
Configuration::Item *Configuration::Lookup(Item *Head,const char *S,
					   unsigned long const &Len,bool const &Create)
{
   int Res = 1;
   Item *I = Head->Child;
   Item **Last = &Head->Child;
   
   // Empty strings match nothing. They are used for lists.
   if (Len != 0)
   {
      for (; I != 0; Last = &I->Next, I = I->Next)
	 if (Len == I->Tag.length() && (Res = stringcasecmp(I->Tag,S,S + Len)) == 0)
	    break;
   }
   else
      for (; I != 0; Last = &I->Next, I = I->Next);
      
   if (Res == 0)
      return I;
   if (Create == false)
      return 0;
   
   I = new Item;
   I->Tag.assign(S,Len);
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
Configuration::Item *Configuration::Lookup(const char *Name,bool const &Create)
{
   if (Name == 0)
      return Root->Child;
   
   const char *Start = Name;
   const char *End = Start + strlen(Name);
   const char *TagEnd = Name;
   Item *Itm = Root;
   for (; End - TagEnd >= 2; TagEnd++)
   {
      if (TagEnd[0] == ':' && TagEnd[1] == ':')
      {
	 Itm = Lookup(Itm,Start,TagEnd - Start,Create);
	 if (Itm == 0)
	    return 0;
	 TagEnd = Start = TagEnd + 2;	 
      }
   }   

   // This must be a trailing ::, we create unique items in a list
   if (End - Start == 0)
   {
      if (Create == false)
	 return 0;
   }
   
   Itm = Lookup(Itm,Start,End - Start,Create);
   return Itm;
}
									/*}}}*/
// Configuration::Find - Find a value					/*{{{*/
// ---------------------------------------------------------------------
/* */
string Configuration::Find(const char *Name,const char *Default) const
{
   checkFindConfigOptionType(Name, ConfigType::STRING);
   const Item *Itm = Lookup(Name);
   if (Itm == 0 || Itm->Value.empty() == true)
   {
      if (Default == 0)
	 return "";
      else
	 return Default;
   }
   
   return Itm->Value;
}
									/*}}}*/
// Configuration::FindFile - Find a Filename				/*{{{*/
// ---------------------------------------------------------------------
/* Directories are stored as the base dir in the Parent node and the
   sub directory in sub nodes with the final node being the end filename
 */
string Configuration::FindFile(const char *Name,const char *Default) const
{
   checkFindConfigOptionType(Name, ConfigType::FILE);
   const Item *RootItem = Lookup("RootDir");
   std::string result =  (RootItem == 0) ? "" : RootItem->Value;
   if(result.empty() == false && result[result.size() - 1] != '/')
     result.push_back('/');

   const Item *Itm = Lookup(Name);
   if (Itm == 0 || Itm->Value.empty() == true)
   {
      if (Default != 0)
	 result.append(Default);
   }
   else
   {
      string val = Itm->Value;
      while (Itm->Parent != 0)
      {
	 if (Itm->Parent->Value.empty() == true)
	 {
	    Itm = Itm->Parent;
	    continue;
	 }

	 // Absolute
	 if (val.length() >= 1 && val[0] == '/')
	 {
	    if (val.compare(0, 9, "/dev/null") == 0)
	       val.erase(9);
	    break;
	 }

	 // ~/foo or ./foo
	 if (val.length() >= 2 && (val[0] == '~' || val[0] == '.') && val[1] == '/')
	    break;

	 // ../foo
	 if (val.length() >= 3 && val[0] == '.' && val[1] == '.' && val[2] == '/')
	    break;

	 if (Itm->Parent->Value.end()[-1] != '/')
	    val.insert(0, "/");

	 val.insert(0, Itm->Parent->Value);
	 Itm = Itm->Parent;
      }
      result.append(val);
   }
   return flNormalize(result);
}
									/*}}}*/
// Configuration::FindDir - Find a directory name			/*{{{*/
// ---------------------------------------------------------------------
/* This is like findfile execept the result is terminated in a / */
string Configuration::FindDir(const char *Name,const char *Default) const
{
   checkFindConfigOptionType(Name, ConfigType::DIR);
   string Res = FindFile(Name,Default);
   if (Res.end()[-1] != '/')
   {
      size_t const found = Res.rfind("/dev/null");
      if (found != string::npos && found == Res.size() - 9)
	 return Res; // /dev/null returning
      return Res + '/';
   }
   return Res;
}
									/*}}}*/
// Configuration::FindVector - Find a vector of values			/*{{{*/
// ---------------------------------------------------------------------
/* Returns a vector of config values under the given item */
vector<string> Configuration::FindVector(const char *Name, std::string const &Default, bool const Keys) const
{
   checkFindConfigOptionType(Name, ConfigType::LIST);
   vector<string> Vec;
   const Item *Top = Lookup(Name);
   if (Top == NULL)
      return VectorizeString(Default, ',');

   if (Top->Value.empty() == false)
      return VectorizeString(Top->Value, ',');

   Item *I = Top->Child;
   while(I != NULL)
   {
      Vec.push_back(Keys ? I->Tag : I->Value);
      I = I->Next;
   }
   if (Vec.empty() == true)
      return VectorizeString(Default, ',');

   return Vec;
}
									/*}}}*/
// Configuration::FindI - Find an integer value				/*{{{*/
// ---------------------------------------------------------------------
/* */
int Configuration::FindI(const char *Name,int const &Default) const
{
   checkFindConfigOptionType(Name, ConfigType::INT);
   const Item *Itm = Lookup(Name);
   if (Itm == 0 || Itm->Value.empty() == true)
      return Default;
   
   char *End;
   int Res = strtol(Itm->Value.c_str(),&End,0);
   if (End == Itm->Value.c_str())
      return Default;
   
   return Res;
}
									/*}}}*/
// Configuration::FindB - Find a boolean type				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Configuration::FindB(const char *Name,bool const &Default) const
{
   checkFindConfigOptionType(Name, ConfigType::BOOL);
   const Item *Itm = Lookup(Name);
   if (Itm == 0 || Itm->Value.empty() == true)
      return Default;
   
   return StringToBool(Itm->Value,Default);
}
									/*}}}*/
// Configuration::FindAny - Find an arbitrary type			/*{{{*/
// ---------------------------------------------------------------------
/* a key suffix of /f, /d, /b or /i calls Find{File,Dir,B,I} */
string Configuration::FindAny(const char *Name,const char *Default) const
{
   string key = Name;
   char type = 0;

   if (key.size() > 2 && key.end()[-2] == '/')
   {
      type = key.end()[-1];
      key.resize(key.size() - 2);
   }

   switch (type)
   {
      // file
      case 'f': 
      return FindFile(key.c_str(), Default);
      
      // directory
      case 'd': 
      return FindDir(key.c_str(), Default);
      
      // bool
      case 'b': 
      return FindB(key, Default) ? "true" : "false";
      
      // int
      case 'i': 
      {
	 char buf[16];
	 snprintf(buf, sizeof(buf)-1, "%d", FindI(key, Default ? atoi(Default) : 0 ));
	 return buf;
      }
   }

   // fallback
   return Find(Name, Default);
}
									/*}}}*/
// Configuration::CndSet - Conditional Set a value			/*{{{*/
// ---------------------------------------------------------------------
/* This will not overwrite */
void Configuration::CndSet(const char *Name,const string &Value)
{
   Item *Itm = Lookup(Name,true);
   if (Itm == 0)
      return;
   if (Itm->Value.empty() == true)
      Itm->Value = Value;
}
									/*}}}*/
// Configuration::Set - Set an integer value				/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::CndSet(const char *Name,int const Value)
{
   Item *Itm = Lookup(Name,true);
   if (Itm == 0 || Itm->Value.empty() == false)
      return;
   char S[300];
   snprintf(S,sizeof(S),"%i",Value);
   Itm->Value = S;
}
									/*}}}*/
// Configuration::Set - Set a value					/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Set(const char *Name,const string &Value)
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
void Configuration::Set(const char *Name,int const &Value)
{
   Item *Itm = Lookup(Name,true);
   if (Itm == 0)
      return;
   char S[300];
   snprintf(S,sizeof(S),"%i",Value);
   Itm->Value = S;
}
									/*}}}*/
// Configuration::Clear - Clear an single value from a list	        /*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(string const &Name, int const &Value)
{
   char S[300];
   snprintf(S,sizeof(S),"%i",Value);
   Clear(Name, S);
}
									/*}}}*/
// Configuration::Clear - Clear an single value from a list	        /*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(string const &Name, string const &Value)
{
   Item *Top = Lookup(Name.c_str(),false);
   if (Top == 0 || Top->Child == 0)
      return;

   Item *Tmp, *Prev, *I;
   Prev = I = Top->Child;

   while(I != NULL)
   {
      if(I->Value == Value)
      {
	 Tmp = I;
	 // was first element, point parent to new first element
	 if(Top->Child == Tmp)
	    Top->Child = I->Next;
	 I = I->Next;
	 Prev->Next = I;
	 delete Tmp;
      } else {
	 Prev = I;
	 I = I->Next;
      }
   }
     
}
									/*}}}*/
// Configuration::Clear - Clear everything				/*{{{*/
// ---------------------------------------------------------------------
void Configuration::Clear()
{
   const Configuration::Item *Top = Tree(0);
   while( Top != 0 )
   {
      Clear(Top->FullTag());
      Top = Top->Next;
   }
}
									/*}}}*/
// Configuration::Clear - Clear an entire tree				/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(string const &Name)
{
   Item *Top = Lookup(Name.c_str(),false);
   if (Top == 0) 
      return;

   Top->Value.clear();
   Item *Stop = Top;
   Top = Top->Child;
   Stop->Child = 0;
   for (; Top != 0;)
   {
      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }

      while (Top != 0 && Top->Next == 0)
      {
	 Item *Tmp = Top;
	 Top = Top->Parent;
	 delete Tmp;
	 
	 if (Top == Stop)
	    return;
      }
      
      Item *Tmp = Top;
      if (Top != 0)
	 Top = Top->Next;
      delete Tmp;
   }
}
									/*}}}*/
void Configuration::MoveSubTree(char const * const OldRootName, char const * const NewRootName)/*{{{*/
{
   // prevent NewRoot being a subtree of OldRoot
   if (OldRootName == nullptr)
      return;
   if (NewRootName != nullptr)
   {
      if (strcmp(OldRootName, NewRootName) == 0)
	 return;
      std::string const oldroot = std::string(OldRootName) + "::";
      if (strcasestr(NewRootName, oldroot.c_str()) != NULL)
	 return;
   }

   Item * Top;
   Item const * const OldRoot = Top = Lookup(OldRootName, false);
   if (Top == nullptr)
      return;
   std::string NewRoot;
   if (NewRootName != nullptr)
      NewRoot.append(NewRootName).append("::");

   Top->Value.clear();
   Item * const Stop = Top;
   Top = Top->Child;
   Stop->Child = 0;
   for (; Top != 0;)
   {
      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }

      while (Top != 0 && Top->Next == 0)
      {
	 Set(NewRoot + Top->FullTag(OldRoot), Top->Value);
	 Item const * const Tmp = Top;
	 Top = Top->Parent;
	 delete Tmp;

	 if (Top == Stop)
	    return;
      }

      Set(NewRoot + Top->FullTag(OldRoot), Top->Value);
      Item const * const Tmp = Top;
      if (Top != 0)
	 Top = Top->Next;
      delete Tmp;
   }
}
									/*}}}*/
// Configuration::Exists - Returns true if the Name exists		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Configuration::Exists(const char *Name) const
{
   const Item *Itm = Lookup(Name);
   if (Itm == 0)
      return false;
   return true;
}
									/*}}}*/
// Configuration::ExistsAny - Returns true if the Name, possibly	/*{{{*/
// ---------------------------------------------------------------------
/* qualified by /[fdbi] exists */
bool Configuration::ExistsAny(const char *Name) const
{
   string key = Name;

   if (key.size() > 2 && key.end()[-2] == '/')
   {
      if (key.find_first_of("fdbi",key.size()-1) < key.size())
      {
         key.resize(key.size() - 2);
         if (Exists(key.c_str()))
            return true;
      }
      else
      {
         _error->Warning(_("Unrecognized type abbreviation: '%c'"), key.end()[-3]);
      }
   }
   return Exists(Name);
}
									/*}}}*/
// Configuration::Dump - Dump the config				/*{{{*/
// ---------------------------------------------------------------------
/* Dump the entire configuration space */
void Configuration::Dump(ostream& str)
{
   Dump(str, NULL, "%F \"%v\";\n", true);
}
void Configuration::Dump(ostream& str, char const * const root,
			 char const * const formatstr, bool const emptyValue)
{
   const Configuration::Item* Top = Tree(root);
   if (Top == 0)
      return;
   const Configuration::Item* const Root = (root == NULL) ? NULL : Top;
   std::vector<std::string> const format = VectorizeString(formatstr, '%');

   /* Write out all of the configuration directives by walking the
      configuration tree */
   do {
      if (emptyValue == true || Top->Value.empty() == emptyValue)
      {
	 std::vector<std::string>::const_iterator f = format.begin();
	 str << *f;
	 for (++f; f != format.end(); ++f)
	 {
	    if (f->empty() == true)
	    {
	       ++f;
	       str << '%' << *f;
	       continue;
	    }
	    char const type = (*f)[0];
	    if (type == 'f')
	       str << Top->FullTag();
	    else if (type == 't')
	       str << Top->Tag;
	    else if (type == 'v')
	       str << Top->Value;
	    else if (type == 'F')
	       str << QuoteString(Top->FullTag(), "=\"\n");
	    else if (type == 'T')
	       str << QuoteString(Top->Tag, "=\"\n");
	    else if (type == 'V')
	       str << QuoteString(Top->Value, "=\"\n");
	    else if (type == 'n')
	       str << "\n";
	    else if (type == 'N')
	       str << "\t";
	    else
	       str << '%' << type;
	    str << f->c_str() + 1;
	 }
      }

      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }

      while (Top != 0 && Top->Next == 0)
	 Top = Top->Parent;
      if (Top != 0)
	 Top = Top->Next;

      if (Root != NULL)
      {
	 const Configuration::Item* I = Top;
	 while(I != 0)
	 {
	    if (I == Root)
	       break;
	    else
	       I = I->Parent;
	 }
	 if (I == 0)
	    break;
      }
   } while (Top != 0);
}
									/*}}}*/

// Configuration::Item::FullTag - Return the fully scoped tag		/*{{{*/
// ---------------------------------------------------------------------
/* Stop sets an optional max recursion depth if this item is being viewed as
   part of a sub tree. */
string Configuration::Item::FullTag(const Item *Stop) const
{
   if (Parent == 0 || Parent->Parent == 0 || Parent == Stop)
      return Tag;
   return Parent->FullTag(Stop) + "::" + Tag;
}
									/*}}}*/

// ReadConfigFile - Read a configuration file				/*{{{*/
// ---------------------------------------------------------------------
/* The configuration format is very much like the named.conf format
   used in bind8, in fact this routine can parse most named.conf files. 
   Sectional config files are like bind's named.conf where there are 
   sections like 'zone "foo.org" { .. };' This causes each section to be
   added in with a tag like "zone::foo.org" instead of being split 
   tag/value. AsSectional enables Sectional parsing.*/
static void leaveCurrentScope(std::stack<std::string> &Stack, std::string &ParentTag)
{
   if (Stack.empty())
      ParentTag.clear();
   else
   {
      ParentTag = Stack.top();
      Stack.pop();
   }
}
bool ReadConfigFile(Configuration &Conf,const string &FName,bool const &AsSectional,
		    unsigned const &Depth)
{
   // Open the stream for reading
   FileFd F;
   if (OpenConfigurationFileFd(FName, F) == false)
      return false;

   string LineBuffer;
   std::stack<std::string> Stack;

   // Parser state
   string ParentTag;

   int CurLine = 0;
   bool InComment = false;
   while (F.Eof() == false)
   {
      // The raw input line.
      std::string Input;
      if (F.ReadLine(Input) == false)
	 Input.clear();
      // The input line with comments stripped.
      std::string Fragment;

      // Expand tabs in the input line and remove leading and trailing whitespace.
      Input = APT::String::Strip(SubstVar(Input, "\t", "        "));
      CurLine++;

      // Now strip comments; if the whole line is contained in a
      // comment, skip this line.
      APT::StringView Line{Input.data(), Input.size()};

      // continued Multi line comment
      if (InComment)
      {
	 size_t end = Line.find("*/");
	 if (end != APT::StringView::npos)
	 {
	    Line.remove_prefix(end + 2);
	    InComment = false;
	 }
	 else
	    continue;
      }

      // Discard single line comments
      {
	 size_t start = 0;
	 while ((start = Line.find("//", start)) != APT::StringView::npos)
	 {
	    if (std::count(Line.begin(), Line.begin() + start, '"') % 2 != 0)
	    {
	       ++start;
	       continue;
	    }
	    Line.remove_suffix(Line.length() - start);
	    break;
	 }
	 using APT::operator""_sv;
	 constexpr std::array<APT::StringView, 3> magicComments { "clear"_sv, "include"_sv, "x-apt-configure-index"_sv };
	 start = 0;
	 while ((start = Line.find('#', start)) != APT::StringView::npos)
	 {
	    if (std::count(Line.begin(), Line.begin() + start, '"') % 2 != 0 ||
		std::any_of(magicComments.begin(), magicComments.end(), [&](auto const m) { return Line.compare(start+1, m.length(), m) == 0; }))
	    {
	       ++start;
	       continue;
	    }
	    Line.remove_suffix(Line.length() - start);
	    break;
	 }
      }

      // Look for multi line comments and build up the
      // fragment.
      Fragment.reserve(Line.length());
      {
	 size_t start = 0;
	 while ((start = Line.find("/*", start)) != APT::StringView::npos)
	 {
	    if (std::count(Line.begin(), Line.begin() + start, '"') % 2 != 0)
	    {
	       start += 2;
	       continue;
	    }
	    Fragment.append(Line.data(), start);
	    auto const end = Line.find("*/", start + 2);
	    if (end == APT::StringView::npos)
	    {
	       Line.clear();
	       InComment = true;
	       break;
	    }
	    else
	       Line.remove_prefix(end + 2);
	    start = 0;
	 }
	 if (not Line.empty())
	    Fragment.append(Line.data(), Line.length());
      }

      // Skip blank lines.
      if (Fragment.empty())
	 continue;

      // The line has actual content; interpret what it means.
      bool InQuote = false;
      auto Start = Fragment.cbegin();
      auto End = Fragment.cend();
      for (std::string::const_iterator I = Start;
	   I != End; ++I)
      {
	 if (*I == '"')
	    InQuote = !InQuote;

	 if (InQuote == false && (*I == '{' || *I == ';' || *I == '}'))
	 {
	    // Put the last fragment into the buffer
	    std::string::const_iterator NonWhitespaceStart = Start;
	    std::string::const_iterator NonWhitespaceStop = I;
	    for (; NonWhitespaceStart != I && isspace(*NonWhitespaceStart) != 0; ++NonWhitespaceStart)
	      ;
	    for (; NonWhitespaceStop != NonWhitespaceStart && isspace(NonWhitespaceStop[-1]) != 0; --NonWhitespaceStop)
	      ;
	    if (LineBuffer.empty() == false && NonWhitespaceStop - NonWhitespaceStart != 0)
	       LineBuffer += ' ';
	    LineBuffer += string(NonWhitespaceStart, NonWhitespaceStop);

	    // Drop this from the input string, saving the character
	    // that terminated the construct we just closed. (i.e., a
	    // brace or a semicolon)
	    char TermChar = *I;
	    Start = I + 1;

	    // Syntax Error
	    if (TermChar == '{' && LineBuffer.empty() == true)
	       return _error->Error(_("Syntax error %s:%u: Block starts with no name."),FName.c_str(),CurLine);

	    // No string on this line
	    if (LineBuffer.empty() == true)
	    {
	       if (TermChar == '}')
		  leaveCurrentScope(Stack, ParentTag);
	       continue;
	    }

	    // Parse off the tag
	    string Tag;
	    const char *Pos = LineBuffer.c_str();
	    if (ParseQuoteWord(Pos,Tag) == false)
	       return _error->Error(_("Syntax error %s:%u: Malformed tag"),FName.c_str(),CurLine);

	    // Parse off the word
	    string Word;
	    bool NoWord = false;
	    if (ParseCWord(Pos,Word) == false &&
		ParseQuoteWord(Pos,Word) == false)
	    {
	       if (TermChar != '{')
	       {
		  Word = Tag;
		  Tag = "";
	       }
	       else
		  NoWord = true;
	    }
	    if (strlen(Pos) != 0)
	       return _error->Error(_("Syntax error %s:%u: Extra junk after value"),FName.c_str(),CurLine);

	    // Go down a level
	    if (TermChar == '{')
	    {
	       Stack.push(ParentTag);

	       /* Make sectional tags incorporate the section into the
	          tag string */
	       if (AsSectional == true && Word.empty() == false)
	       {
		  Tag.append("::").append(Word);
		  Word.clear();
	       }

	       if (ParentTag.empty() == true)
		  ParentTag = Tag;
	       else
		  ParentTag.append("::").append(Tag);
	       Tag.clear();
	    }

	    // Generate the item name
	    string Item;
	    if (ParentTag.empty() == true)
	       Item = Tag;
	    else
	    {
	       if (TermChar != '{' || Tag.empty() == false)
		  Item = ParentTag + "::" + Tag;
	       else
		  Item = ParentTag;
	    }

	    // Specials
	    if (Tag.length() >= 1 && Tag[0] == '#')
	    {
	       if (ParentTag.empty() == false)
		  return _error->Error(_("Syntax error %s:%u: Directives can only be done at the top level"),FName.c_str(),CurLine);
	       Tag.erase(Tag.begin());
	       if (Tag == "clear")
		  Conf.Clear(Word);
	       else if (Tag == "include")
	       {
		  if (Depth > 10)
		     return _error->Error(_("Syntax error %s:%u: Too many nested includes"),FName.c_str(),CurLine);
		  if (Word.length() > 2 && Word.end()[-1] == '/')
		  {
		     if (ReadConfigDir(Conf,Word,AsSectional,Depth+1) == false)
			return _error->Error(_("Syntax error %s:%u: Included from here"),FName.c_str(),CurLine);
		  }
		  else
		  {
		     if (ReadConfigFile(Conf,Word,AsSectional,Depth+1) == false)
			return _error->Error(_("Syntax error %s:%u: Included from here"),FName.c_str(),CurLine);
		  }
	       }
	       else if (Tag == "x-apt-configure-index")
	       {
		  if (LoadConfigurationIndex(Word) == false)
		     return _error->Warning("Loading the configure index %s in file %s:%u failed!", Word.c_str(), FName.c_str(), CurLine);
	       }
	       else
		  return _error->Error(_("Syntax error %s:%u: Unsupported directive '%s'"),FName.c_str(),CurLine,Tag.c_str());
	    }
	    else if (Tag.empty() == true && NoWord == false && Word == "#clear")
	       return _error->Error(_("Syntax error %s:%u: clear directive requires an option tree as argument"),FName.c_str(),CurLine);
	    else
	    {
	       // Set the item in the configuration class
	       if (NoWord == false)
		  Conf.Set(Item,Word);
	    }

	    // Empty the buffer
	    LineBuffer.clear();

	    // Move up a tag, but only if there is no bit to parse
	    if (TermChar == '}')
	       leaveCurrentScope(Stack, ParentTag);
	 }
      }

      // Store the remaining text, if any, in the current line buffer.

      // NB: could change this to use string-based operations; I'm
      // using strstrip now to ensure backwards compatibility.
      //   -- dburrows 2008-04-01
      {
	char *Buffer = new char[End - Start + 1];
	try
	  {
	    std::copy(Start, End, Buffer);
	    Buffer[End - Start] = '\0';

	    const char *Stripd = _strstrip(Buffer);
	    if (*Stripd != 0 && LineBuffer.empty() == false)
	      LineBuffer += " ";
	    LineBuffer += Stripd;
	  }
	catch(...)
	  {
	    delete[] Buffer;
	    throw;
	  }
	delete[] Buffer;
      }
   }

   if (LineBuffer.empty() == false)
      return _error->Error(_("Syntax error %s:%u: Extra junk at end of file"),FName.c_str(),CurLine);
   return true;
}
									/*}}}*/
// ReadConfigDir - Read a directory of config files			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ReadConfigDir(Configuration &Conf,const string &Dir,
		   bool const &AsSectional, unsigned const &Depth)
{
   _error->PushToStack();
   auto const files = GetListOfFilesInDir(Dir, "conf", true, true);
   auto const successfulList = not _error->PendingError();
   _error->MergeWithStack();
   return std::accumulate(files.cbegin(), files.cend(), true, [&](bool good, auto const &file) {
      return ReadConfigFile(Conf, file, AsSectional, Depth) && good;
   }) && successfulList;
}
									/*}}}*/
// MatchAgainstConfig Constructor					/*{{{*/
Configuration::MatchAgainstConfig::MatchAgainstConfig(char const * Config)
{
   std::vector<std::string> const strings = _config->FindVector(Config);
   for (std::vector<std::string>::const_iterator s = strings.begin();
	s != strings.end(); ++s)
   {
      regex_t *p = new regex_t;
      if (regcomp(p, s->c_str(), REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0)
	 patterns.push_back(p);
      else
      {
	 regfree(p);
	 delete p;
	 _error->Warning("Invalid regular expression '%s' in configuration "
                         "option '%s' will be ignored.",
                         s->c_str(), Config);
	 continue;
      }
   }
   if (strings.empty() == true)
      patterns.push_back(NULL);
}
									/*}}}*/
// MatchAgainstConfig Destructor					/*{{{*/
Configuration::MatchAgainstConfig::~MatchAgainstConfig()
{
   clearPatterns();
}
void Configuration::MatchAgainstConfig::clearPatterns()
{
   for(std::vector<regex_t *>::const_iterator p = patterns.begin();
	p != patterns.end(); ++p)
   {
      if (*p == NULL) continue;
      regfree(*p);
      delete *p;
   }
   patterns.clear();
}
									/*}}}*/
// MatchAgainstConfig::Match - returns true if a pattern matches	/*{{{*/
bool Configuration::MatchAgainstConfig::Match(char const * str) const
{
   for(std::vector<regex_t *>::const_iterator p = patterns.begin();
	p != patterns.end(); ++p)
      if (*p != NULL && regexec(*p, str, 0, 0, 0) == 0)
	 return true;

   return false;
}
									/*}}}*/
