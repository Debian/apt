// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cmndline.h,v 1.7 1999/10/31 06:32:28 jgg Exp $
/* ######################################################################

   Command Line Class - Sophisticated command line parser
   
   This class provides a unified command line parser/option handliner/
   configuration mechanism. It allows the caller to specify the option
   set and map the option set into the configuration class or other
   special functioning.
   
   Filenames are stripped from the option stream and put into their
   own array.
   
   The argument descriptor array can be initialized as:
   
 CommandLine::Args Args[] = 
 {{'q',"quiet","apt::get::quiet",CommandLine::IntLevel},
  {0,0,0,0}};
   
   The flags mean,
     HasArg - Means the argument has a value
     IntLevel - Means the argument is an integer level indication, the
                following -qqqq (+3) -q5 (=5) -q=5 (=5) are valid
     Boolean  - Means it is true/false or yes/no. 
                -d (true) --no-d (false) --yes-d (true)
                --long (true) --no-long (false) --yes-long (true)
                -d=yes (true) -d=no (false) Words like enable, disable,
                true false, yes no and on off are recognized in logical 
                places.
     InvBoolean - Same as boolean but the case with no specified sense
                  (first case) is set to false.
     ConfigFile - Means this flag should be interprited as the name of 
                  a config file to read in at this point in option processing.
                  Implies HasArg.
     ArbItem    - Means the item is an arbitrary configuration string of
                  the form item=value, where item is passed directly
                  to the configuration class.
   The default, if the flags are 0 is to use Boolean
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CMNDLINE_H
#define PKGLIB_CMNDLINE_H



#include <apt-pkg/configuration.h>

class CommandLine
{
   public:
   struct Args;
   struct Dispatch;
   
   protected:
   
   Args *ArgList;
   Configuration *Conf;
   bool HandleOpt(int &I,int argc,const char *argv[],
		  const char *&Opt,Args *A,bool PreceedeMatch = false);

   public:
   
   enum AFlags 
   {
      HasArg = (1 << 0),
      IntLevel = (1 << 1),
      Boolean = (1 << 2),
      InvBoolean = (1 << 3),
      ConfigFile = (1 << 4) | HasArg,
      ArbItem = (1 << 5) | HasArg
   };

   const char **FileList;
   
   bool Parse(int argc,const char **argv);
   void ShowHelp();
   unsigned int FileSize() const;
   bool DispatchArg(Dispatch *List,bool NoMatch = true);
      
   CommandLine(Args *AList,Configuration *Conf);
   ~CommandLine();
};

struct CommandLine::Args
{
   char ShortOpt;
   const char *LongOpt;
   const char *ConfName;
   unsigned long Flags;
   
   inline bool end() {return ShortOpt == 0 && LongOpt == 0;};
   inline bool IsBoolean() {return Flags == 0 || (Flags & (Boolean|InvBoolean)) != 0;};
};

struct CommandLine::Dispatch
{
   const char *Match;
   bool (*Handler)(CommandLine &);
};

#endif
