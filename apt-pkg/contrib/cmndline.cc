// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cmndline.cc,v 1.15 2003/02/10 01:40:58 doogie Exp $
/* ######################################################################

   Command Line Class - Sophisticated command line parser
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe <jgg@debian.org>.
   
   ##################################################################### */
									/*}}}*/
// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <apti18n.h>
									/*}}}*/
using namespace std;

// CommandLine::CommandLine - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CommandLine::CommandLine(Args *AList,Configuration *Conf) : ArgList(AList), 
                                 Conf(Conf), FileList(0)
{
}
CommandLine::CommandLine() : ArgList(NULL), Conf(NULL), FileList(0)
{
}
									/*}}}*/
// CommandLine::~CommandLine - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CommandLine::~CommandLine()
{
   delete [] FileList;
}
									/*}}}*/
// CommandLine::GetCommand - return the first non-option word		/*{{{*/
char const * CommandLine::GetCommand(Dispatch const * const Map,
      unsigned int const argc, char const * const * const argv)
{
   // if there is a -- on the line there must be the word we search for either
   // before it (as -- marks the end of the options) or right after it (as we can't
   // decide if the command is actually an option, given that in theory, you could
   // have parameters named like commands)
   for (size_t i = 1; i < argc; ++i)
   {
      if (strcmp(argv[i], "--") != 0)
	 continue;
      // check if command is before --
      for (size_t k = 1; k < i; ++k)
	 for (size_t j = 0; Map[j].Match != NULL; ++j)
	    if (strcmp(argv[k], Map[j].Match) == 0)
	       return Map[j].Match;
      // see if the next token after -- is the command
      ++i;
      if (i < argc)
	 for (size_t j = 0; Map[j].Match != NULL; ++j)
	    if (strcmp(argv[i], Map[j].Match) == 0)
	       return Map[j].Match;
      // we found a --, but not a command
      return NULL;
   }
   // no --, so search for the first word matching a command
   // FIXME: How like is it that an option parameter will be also a valid Match ?
   for (size_t i = 1; i < argc; ++i)
   {
      if (*(argv[i]) == '-')
	 continue;
      for (size_t j = 0; Map[j].Match != NULL; ++j)
	 if (strcmp(argv[i], Map[j].Match) == 0)
	    return Map[j].Match;
   }
   return NULL;
}
									/*}}}*/
// CommandLine::Parse - Main action member				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CommandLine::Parse(int argc,const char **argv)
{
   delete [] FileList;
   FileList = new const char *[argc];
   const char **Files = FileList;
   int I;
   for (I = 1; I != argc; I++)
   {
      const char *Opt = argv[I];
      
      // It is not an option
      if (*Opt != '-')
      {
	 *Files++ = Opt;
	 continue;
      }
      
      Opt++;
      
      // Double dash signifies the end of option processing
      if (*Opt == '-' && Opt[1] == 0)
      {
	 I++;
	 break;
      }
      
      // Single dash is a short option
      if (*Opt != '-')
      {
	 // Iterate over each letter
	 while (*Opt != 0)
	 {	    
	    // Search for the option
	    Args *A;
	    for (A = ArgList; A->end() == false && A->ShortOpt != *Opt; A++);
	    if (A->end() == true)
	       return _error->Error(_("Command line option '%c' [from %s] is not understood in combination with the other options."),*Opt,argv[I]);

	    if (HandleOpt(I,argc,argv,Opt,A) == false)
	       return false;
	    if (*Opt != 0)
	       Opt++;	       
	 }
	 continue;
      }
      
      Opt++;

      // Match up to a = against the list
      Args *A;
      const char *OptEnd = strchrnul(Opt, '=');
      for (A = ArgList; A->end() == false &&
	   (A->LongOpt == 0 || stringcasecmp(Opt,OptEnd,A->LongOpt) != 0);
	   ++A);
      
      // Failed, look for a word after the first - (no-foo)
      bool PreceedMatch = false;
      if (A->end() == true)
      {
         Opt = (const char*) memchr(Opt, '-', OptEnd - Opt);
	 if (Opt == NULL)
	    return _error->Error(_("Command line option %s is not understood in combination with the other options"),argv[I]);
	 Opt++;
	 
	 for (A = ArgList; A->end() == false &&
	      (A->LongOpt == 0 || stringcasecmp(Opt,OptEnd,A->LongOpt) != 0);
	      ++A);

	 // Failed again..
	 if (A->end() == true && OptEnd - Opt != 1)
	    return _error->Error(_("Command line option %s is not understood in combination with the other options"),argv[I]);

	 // The option could be a single letter option prefixed by a no-..
	 if (A->end() == true)
	 {
	    for (A = ArgList; A->end() == false && A->ShortOpt != *Opt; A++);
	    
	    if (A->end() == true)
	       return _error->Error(_("Command line option %s is not understood in combination with the other options"),argv[I]);
	 }
	 
	 // The option is not boolean
	 if (A->IsBoolean() == false)
	    return _error->Error(_("Command line option %s is not boolean"),argv[I]);
	 PreceedMatch = true;
      }
      
      // Deal with it.
      OptEnd--;
      if (HandleOpt(I,argc,argv,OptEnd,A,PreceedMatch) == false)
	 return false;
   }
   
   // Copy any remaining file names over
   for (; I != argc; I++)
      *Files++ = argv[I];
   *Files = 0;

   SaveInConfig(argc, argv);

   return true;
}
									/*}}}*/
// CommandLine::HandleOpt - Handle a single option including all flags	/*{{{*/
// ---------------------------------------------------------------------
/* This is a helper function for parser, it looks at a given argument
   and looks for specific patterns in the string, it gets tokanized
   -ruffly- like -*[yes|true|enable]-(o|longopt)[=][ ][argument] */
bool CommandLine::HandleOpt(int &I,int argc,const char *argv[],
			    const char *&Opt,Args *A,bool PreceedMatch)
{
   const char *Argument = 0;
   bool CertainArg = false;
   int IncI = 0;

   /* Determine the possible location of an option or 0 if their is
      no option */
   if (Opt[1] == 0)
   {
      if (I + 1 < argc && argv[I+1][0] != '-')
	 Argument = argv[I+1];

      IncI = 1;
   }
   else
   {
      if (Opt[1] == '=')
      {
	 CertainArg = true;
	 Argument = Opt + 2;
      }      
      else
	 Argument = Opt + 1;
   }
   
   // Option is an argument set
   if ((A->Flags & HasArg) == HasArg)
   {
      if (Argument == 0)
	 return _error->Error(_("Option %s requires an argument."),argv[I]);
      Opt += strlen(Opt);
      I += IncI;
      
      // Parse a configuration file
      if ((A->Flags & ConfigFile) == ConfigFile)
	 return ReadConfigFile(*Conf,Argument);

      // Arbitrary item specification
      if ((A->Flags & ArbItem) == ArbItem)
      {
	 const char * const J = strchr(Argument, '=');
	 if (J == nullptr)
	    return _error->Error(_("Option %s: Configuration item specification must have an =<val>."),argv[I]);

	 Conf->Set(string(Argument,J-Argument), J+1);
	 return true;
      }
      
      const char *I = strchrnul(A->ConfName, ' ');
      if (*I == ' ')
	 Conf->Set(string(A->ConfName,0,I-A->ConfName),string(I+1) + Argument);
      else
	 Conf->Set(A->ConfName,string(I) + Argument);
	 
      return true;
   }
   
   // Option is an integer level
   if ((A->Flags & IntLevel) == IntLevel)
   {
      // There might be an argument
      if (Argument != 0)
      {
	 char *EndPtr;
	 unsigned long Value = strtol(Argument,&EndPtr,10);
	 
	 // Conversion failed and the argument was specified with an =s
	 if (EndPtr == Argument && CertainArg == true)
	    return _error->Error(_("Option %s requires an integer argument, not '%s'"),argv[I],Argument);

	 // Conversion was ok, set the value and return
	 if (EndPtr != 0 && EndPtr != Argument && *EndPtr == 0)
	 {
	    Conf->Set(A->ConfName,Value);
	    Opt += strlen(Opt);
	    I += IncI;
	    return true;
	 }	 
      }      
      
      // Increase the level
      Conf->Set(A->ConfName,Conf->FindI(A->ConfName)+1);
      return true;
   }
  
   // Option is a boolean
   int Sense = -1;  // -1 is unspecified, 0 is yes 1 is no

   // Look for an argument.
   while (1)
   {
      // Look at preceding text
      char Buffer[300];
      if (Argument == 0)
      {
	 if (PreceedMatch == false)
	    break;
	 
	 if (strlen(argv[I]) >= sizeof(Buffer))
	    return _error->Error(_("Option '%s' is too long"),argv[I]);

	 // Skip the leading dash
	 const char *J = argv[I];
	 for (; *J != 0 && *J == '-'; J++);

	 const char *JEnd = strchr(J, '-');
	 if (JEnd != NULL)
	 {
	    strncpy(Buffer,J,JEnd - J);
	    Buffer[JEnd - J] = 0;
	    Argument = Buffer;
	    CertainArg = true;
	 }	 
	 else
	    break;
      }

      // Check for boolean
      Sense = StringToBool(Argument);
      if (Sense >= 0)
      {
	 // Eat the argument	 
	 if (Argument != Buffer)
	 {
	    Opt += strlen(Opt);
	    I += IncI;
	 }	 
	 break;
      }

      if (CertainArg == true)
	 return _error->Error(_("Sense %s is not understood, try true or false."),Argument);
      
      Argument = 0;
   }
      
   // Indeterminate sense depends on the flag
   if (Sense == -1)
   {
      if ((A->Flags & InvBoolean) == InvBoolean)
	 Sense = 0;
      else
	 Sense = 1;
   }
   
   Conf->Set(A->ConfName,Sense);
   return true;
}
									/*}}}*/
// CommandLine::FileSize - Count the number of filenames		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned int CommandLine::FileSize() const
{
   unsigned int Count = 0;
   for (const char **I = FileList; I != 0 && *I != 0; I++)
      Count++;
   return Count;
}
									/*}}}*/
// CommandLine::DispatchArg - Do something with the first arg		/*{{{*/
bool CommandLine::DispatchArg(Dispatch const * const Map,bool NoMatch)
{
   int I;
   for (I = 0; Map[I].Match != 0; I++)
   {
      if (strcmp(FileList[0],Map[I].Match) == 0)
      {
	 bool Res = Map[I].Handler(*this);
	 if (Res == false && _error->PendingError() == false)
	    _error->Error("Handler silently failed");
	 return Res;
      }
   }
   
   // No matching name
   if (Map[I].Match == 0)
   {
      if (NoMatch == true)
	 _error->Error(_("Invalid operation %s"),FileList[0]);
   }
   
   return false;
}
bool CommandLine::DispatchArg(Dispatch *Map,bool NoMatch)
{
   Dispatch const * const Map2 = Map;
   return DispatchArg(Map2, NoMatch);
}
									/*}}}*/
// CommandLine::SaveInConfig - for output later in a logfile or so	/*{{{*/
// ---------------------------------------------------------------------
/* We save the commandline here to have it around later for e.g. logging.
   It feels a bit like a hack here and isn't bulletproof, but it is better
   than nothing after all. */
void CommandLine::SaveInConfig(unsigned int const &argc, char const * const * const argv)
{
   char cmdline[100 + argc * 50];
   memset(cmdline, 0, sizeof(cmdline));
   unsigned int length = 0;
   bool lastWasOption = false;
   bool closeQuote = false;
   for (unsigned int i = 0; i < argc && length < sizeof(cmdline); ++i, ++length)
   {
      for (unsigned int j = 0; argv[i][j] != '\0' && length < sizeof(cmdline)-2; ++j)
      {
	 // we can't really sensibly deal with quoting so skip it
	 if (strchr("\"\'\r\n", argv[i][j]) != nullptr)
	    continue;
	 cmdline[length++] = argv[i][j];
	 if (lastWasOption == true && argv[i][j] == '=')
	 {
	    // That is possibly an option: Quote it if it includes spaces,
	    // the benefit is that this will eliminate also most false positives
	    const char* c = strchr(&argv[i][j+1], ' ');
	    if (c == NULL) continue;
	    cmdline[length++] = '\'';
	    closeQuote = true;
	 }
      }
      if (closeQuote == true)
      {
	 cmdline[length++] = '\'';
	 closeQuote = false;
      }
      // Problem: detects also --hello
      if (cmdline[length-1] == 'o')
	 lastWasOption = true;
      cmdline[length] = ' ';
   }
   cmdline[--length] = '\0';
   _config->Set("CommandLine::AsString", cmdline);
}
									/*}}}*/
CommandLine::Args CommandLine::MakeArgs(char ShortOpt, char const *LongOpt, char const *ConfName, unsigned long Flags)/*{{{*/
{
   /* In theory, this should be a constructor for CommandLine::Args instead,
      but this breaks compatibility as gcc thinks this is a c++11 initializer_list */
   CommandLine::Args arg;
   arg.ShortOpt = ShortOpt;
   arg.LongOpt = LongOpt;
   arg.ConfName = ConfName;
   arg.Flags = Flags;
   return arg;
}
									/*}}}*/
