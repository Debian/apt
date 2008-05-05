// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: configuration.cc,v 1.28 2004/04/30 04:00:15 mdz Exp $
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
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apti18n.h>

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
    
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
									/*}}}*/

Configuration *_config = new Configuration;

// Configuration::Configuration - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
Configuration::Configuration() : ToFree(true)
{
   Root = new Item;
}
Configuration::Configuration(const Item *Root) : Root((Item *)Root), ToFree(false)
{
};

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
					   unsigned long Len,bool Create)
{
   int Res = 1;
   Item *I = Head->Child;
   Item **Last = &Head->Child;
   
   // Empty strings match nothing. They are used for lists.
   if (Len != 0)
   {
      for (; I != 0; Last = &I->Next, I = I->Next)
	 if ((Res = stringcasecmp(I->Tag,S,S + Len)) == 0)
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
Configuration::Item *Configuration::Lookup(const char *Name,bool Create)
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
   const Item *RootItem = Lookup("RootDir");
   std::string rootDir =  (RootItem == 0) ? "" : RootItem->Value;
   if(rootDir.size() > 0 && rootDir[rootDir.size() - 1] != '/')
     rootDir.push_back('/');

   const Item *Itm = Lookup(Name);
   if (Itm == 0 || Itm->Value.empty() == true)
   {
      if (Default == 0)
	 return "";
      else
	 return Default;
   }
   
   string val = Itm->Value;
   while (Itm->Parent != 0 && Itm->Parent->Value.empty() == false)
   {	 
      // Absolute
      if (val.length() >= 1 && val[0] == '/')
         break;

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

   return rootDir + val;
}
									/*}}}*/
// Configuration::FindDir - Find a directory name			/*{{{*/
// ---------------------------------------------------------------------
/* This is like findfile execept the result is terminated in a / */
string Configuration::FindDir(const char *Name,const char *Default) const
{
   string Res = FindFile(Name,Default);
   if (Res.end()[-1] != '/')
      return Res + '/';
   return Res;
}
									/*}}}*/
// Configuration::FindI - Find an integer value				/*{{{*/
// ---------------------------------------------------------------------
/* */
int Configuration::FindI(const char *Name,int Default) const
{
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
bool Configuration::FindB(const char *Name,bool Default) const
{
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
// Configuration::CndSet - Conditinal Set a value			/*{{{*/
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
// Configuration::Clear - Clear an single value from a list	        /*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(const string Name, int Value)
{
   char S[300];
   snprintf(S,sizeof(S),"%i",Value);
   Clear(Name, S);
}
									/*}}}*/
// Configuration::Clear - Clear an single value from a list	        /*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(const string Name, string Value)
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
// Configuration::Clear - Clear an entire tree				/*{{{*/
// ---------------------------------------------------------------------
/* */
void Configuration::Clear(string Name)
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
   /* Write out all of the configuration directives by walking the 
      configuration tree */
   const Configuration::Item *Top = Tree(0);
   for (; Top != 0;)
   {
      str << Top->FullTag() << " \"" << Top->Value << "\";" << endl;
      
      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }
      
      while (Top != 0 && Top->Next == 0)
	 Top = Top->Parent;
      if (Top != 0)
	 Top = Top->Next;
   }
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
bool ReadConfigFile(Configuration &Conf,const string &FName,bool AsSectional,
		    unsigned Depth)
{   
   // Open the stream for reading
   ifstream F(FName.c_str(),ios::in); 
   if (!F != 0)
      return _error->Errno("ifstream::ifstream",_("Opening configuration file %s"),FName.c_str());

   string LineBuffer;
   string Stack[100];
   unsigned int StackPos = 0;
   
   // Parser state
   string ParentTag;
   
   int CurLine = 0;
   bool InComment = false;
   while (F.eof() == false)
   {
      // The raw input line.
      std::string Input;
      // The input line with comments stripped.
      std::string Fragment;

      // Grab the next line of F and place it in Input.
      do
	{
	  char *Buffer = new char[1024];

	  F.clear();
	  F.getline(Buffer,sizeof(Buffer) / 2);

	  Input += Buffer;
	}
      while (F.fail() && !F.eof());

      // Expand tabs in the input line and remove leading and trailing
      // whitespace.
      {
	const int BufferSize = Input.size() * 8 + 1;
	char *Buffer = new char[BufferSize];
	try
	  {
	    memcpy(Buffer, Input.c_str(), Input.size() + 1);

	    _strtabexpand(Buffer, BufferSize);
	    _strstrip(Buffer);
	    Input = Buffer;
	  }
	catch(...)
	  {
	    delete[] Buffer;
	    throw;
	  }
	delete[] Buffer;
      }
      CurLine++;

      // Now strip comments; if the whole line is contained in a
      // comment, skip this line.

      // The first meaningful character in the current fragment; will
      // be adjusted below as we remove bytes from the front.
      std::string::const_iterator Start = Input.begin();
      // The last meaningful character in the current fragment.
      std::string::const_iterator End = Input.end();

      // Multi line comment
      if (InComment == true)
      {
	for (std::string::const_iterator I = Start;
	     I != End; ++I)
	 {
	    if (*I == '*' && I + 1 != End && I[1] == '/')
	    {
	       Start = I + 2;
	       InComment = false;
	       break;
	    }	    
	 }
	 if (InComment == true)
	    continue;
      }
      
      // Discard single line comments
      bool InQuote = false;
      for (std::string::const_iterator I = Start;
	   I != End; ++I)
      {
	 if (*I == '"')
	    InQuote = !InQuote;
	 if (InQuote == true)
	    continue;
	 
	 if (*I == '/' && I + 1 != End && I[1] == '/')
         {
	    End = I;
	    break;
	 }
      }

      // Look for multi line comments and build up the
      // fragment.
      Fragment.reserve(End - Start);
      InQuote = false;
      for (std::string::const_iterator I = Start;
	   I != End; ++I)
      {
	 if (*I == '"')
	    InQuote = !InQuote;
	 if (InQuote == true)
	   Fragment.push_back(*I);
	 else if (*I == '/' && I + 1 != End && I[1] == '*')
         {
	    InComment = true;
	    for (std::string::const_iterator J = I;
		 J != End; ++J)
	    {
	       if (*J == '*' && J + 1 != End && J[1] == '/')
	       {
		  // Pretend we just finished walking over the
		  // comment, and don't add anything to the output
		  // fragment.
		  I = J + 1;
		  InComment = false;
		  break;
	       }	       
	    }
	    
	    if (InComment == true)
	      break;
	 }
	 else
	   Fragment.push_back(*I);
      }

      // Skip blank lines.
      if (Fragment.empty())
	 continue;
      
      // The line has actual content; interpret what it means.
      InQuote = false;
      Start = Fragment.begin();
      End = Fragment.end();
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
	    for (; NonWhitespaceStart != I && isspace(*NonWhitespaceStart) != 0; NonWhitespaceStart++)
	      ;
	    for (; NonWhitespaceStop != NonWhitespaceStart && isspace(NonWhitespaceStop[-1]) != 0; NonWhitespaceStop--)
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
	       {
		  if (StackPos == 0)
		     ParentTag = string();
		  else
		     ParentTag = Stack[--StackPos];
	       }
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
	       if (StackPos <= 100)
		  Stack[StackPos++] = ParentTag;
	       
	       /* Make sectional tags incorperate the section into the
	          tag string */
	       if (AsSectional == true && Word.empty() == false)
	       {
		  Tag += "::" ;
		  Tag += Word;
		  Word = "";
	       }
	       
	       if (ParentTag.empty() == true)
		  ParentTag = Tag;
	       else
		  ParentTag += string("::") + Tag;
	       Tag = string();
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
	       else
		  return _error->Error(_("Syntax error %s:%u: Unsupported directive '%s'"),FName.c_str(),CurLine,Tag.c_str());
	    }
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
	    {
	       if (StackPos == 0)
		  ParentTag.clear();
	       else
		  ParentTag = Stack[--StackPos];
	    }
	    
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
bool ReadConfigDir(Configuration &Conf,const string &Dir,bool AsSectional,
		   unsigned Depth)
{   
   DIR *D = opendir(Dir.c_str());
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());

   vector<string> List;
   
   for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D))
   {
      if (Ent->d_name[0] == '.')
	 continue;
      
      // Skip bad file names ala run-parts
      const char *C = Ent->d_name;
      for (; *C != 0; C++)
	 if (isalpha(*C) == 0 && isdigit(*C) == 0 && *C != '_' && *C != '-')
	    break;
      if (*C != 0)
	 continue;
      
      // Make sure it is a file and not something else
      string File = flCombine(Dir,Ent->d_name);
      struct stat St;
      if (stat(File.c_str(),&St) != 0 || S_ISREG(St.st_mode) == 0)
	 continue;
      
      List.push_back(File);      
   }   
   closedir(D);
   
   sort(List.begin(),List.end());

   // Read the files
   for (vector<string>::const_iterator I = List.begin(); I != List.end(); I++)
      if (ReadConfigFile(Conf,*I,AsSectional,Depth) == false)
	 return false;
   return true;
}
									/*}}}*/
