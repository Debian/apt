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
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

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
									/*}}}*/
// CommandLine::~CommandLine - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
CommandLine::~CommandLine()
{
   delete [] FileList;
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
	       return _error->Error(_("Command line option '%c' [from %s] is not known."),*Opt,argv[I]);

	    if (HandleOpt(I,argc,argv,Opt,A) == false)
	       return false;
	    if (*Opt != 0)
	       Opt++;	       
	 }
	 continue;
      }
      
      Opt++;

      // Match up to a = against the list
      const char *OptEnd = Opt;
      Args *A;
      for (; *OptEnd != 0 && *OptEnd != '='; OptEnd++);
      for (A = ArgList; A->end() == false && 
	   stringcasecmp(Opt,OptEnd,A->LongOpt) != 0; A++);
      
      // Failed, look for a word after the first - (no-foo)
      bool PreceedMatch = false;
      if (A->end() == true)
      {
	 for (; Opt != OptEnd && *Opt != '-'; Opt++);

	 if (Opt == OptEnd)
	    return _error->Error(_("Command line option %s is not understood"),argv[I]);
	 Opt++;
	 
	 for (A = ArgList; A->end() == false &&
	      stringcasecmp(Opt,OptEnd,A->LongOpt) != 0; A++);

	 // Failed again..
	 if (A->end() == true && OptEnd - Opt != 1)
	    return _error->Error(_("Command line option %s is not understood"),argv[I]);

	 // The option could be a single letter option prefixed by a no-..
	 if (A->end() == true)
	 {
	    for (A = ArgList; A->end() == false && A->ShortOpt != *Opt; A++);
	    
	    if (A->end() == true)
	       return _error->Error(_("Command line option %s is not understood"),argv[I]);
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
   if (Opt[1] == 0 || (Opt[1] == '=' && Opt[2] == 0))
   {
      if (I + 1 < argc && argv[I+1][0] != '-')
	 Argument = argv[I+1];
      
      // Equals was specified but we fell off the end!
      if (Opt[1] == '=' && Argument == 0)
	 return _error->Error(_("Option %s requires an argument."),argv[I]);
      if (Opt[1] == '=')
	 CertainArg = true;
	 
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
	 const char *J;
	 for (J = Argument; *J != 0 && *J != '='; J++);
	 if (*J == 0)
	    return _error->Error(_("Option %s: Configuration item specification must have an =<val>."),argv[I]);

	 // = is trailing
	 if (J[1] == 0)
	 {
	    if (I+1 >= argc)
	       return _error->Error(_("Option %s: Configuration item specification must have an =<val>."),argv[I]);
	    Conf->Set(string(Argument,J-Argument),string(argv[I++ +1]));
	 }
	 else
	    Conf->Set(string(Argument,J-Argument),string(J+1));
	 
	 return true;
      }
      
      const char *I = A->ConfName;
      for (; *I != 0 && *I != ' '; I++);
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
      // Look at preceeding text
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
	 
	 const char *JEnd = J;
	 for (; *JEnd != 0 && *JEnd != '-'; JEnd++);
	 if (*JEnd != 0)
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
// ---------------------------------------------------------------------
/* */
bool CommandLine::DispatchArg(Dispatch *Map,bool NoMatch)
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
									/*}}}*/
