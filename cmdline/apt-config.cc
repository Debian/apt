// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-config.cc,v 1.1 1998/11/22 23:37:07 jgg Exp $
/* ######################################################################
   
   APT Config - Program to manipulate APT configuration files
   
   This program will parse a config file and then do something with it.
   
   Commands:
     shell - Shell mode. After this a series of word pairs should occure.
             The first is the environment var to set and the second is
             the key to set it from. Use like: 
 eval `apt-config shell QMode apt::QMode`
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include "config.h"

#include <iostream>
									/*}}}*/

// DoShell - Handle the shell command					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoShell(CommandLine &CmdL)
{
   for (const char **I = CmdL.FileList + 1; *I != 0; I += 2)
   {
      if (I[1] == 0)
	 return _error->Error("Arguments not in pairs");
      if (_config->Exists(I[1]) == true)
	 cout << *I << "=\"" << _config->Find(I[1]) << '"' << endl;
   }
   
   return true;
}
									/*}}}*/
// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   cout << PACKAGE << ' ' << VERSION << " for " << ARCHITECTURE <<
       " compiled on " << __DATE__ << "  " << __TIME__ << endl;
   
   cout << "Usage: apt-config [options] command" << endl;
   cout << endl;
   cout << "apt-config is a simple tool to read the APT config file" << endl;   
   cout << endl;
   cout << "Commands:" << endl;
   cout << "   shell - Shell mode" << endl;
   cout << endl;
   cout << "Options:" << endl;
   cout << "  -h   This help text." << endl;
   cout << "  -c=? Read this configuration file" << endl;
   cout << "  -o=? Set an arbitary configuration option, ie -o dir::cache=/tmp" << endl;
   return 100;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   
   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitialize(*_config) == false ||
       CmdL.Parse(argc,argv) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();
   
   // Match the operation
   struct 
   {
      const char *Match;
      bool (*Handler)(CommandLine &);
   } Map[] = {{"shell",&DoShell},
              {0,0}};
   int I;
   for (I = 0; Map[I].Match != 0; I++)
   {
      if (strcmp(CmdL.FileList[0],Map[I].Match) == 0)
      {
	 if (Map[I].Handler(CmdL) == false && _error->PendingError() == false)
	    _error->Error("Handler silently failed");
	 break;
      }
   }
      
   // No matching name
   if (Map[I].Match == 0)
      _error->Error("Invalid operation %s", CmdL.FileList[0]);

   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;
}
