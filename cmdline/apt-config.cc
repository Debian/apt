// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-config.cc,v 1.11 2003/01/11 07:18:44 jgg Exp $
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
#include <apt-pkg/strutl.h>

#include <config.h>
#include <apti18n.h>

#include <locale.h>
#include <iostream>
#include <string>
									/*}}}*/
using namespace std;

// DoShell - Handle the shell command					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoShell(CommandLine &CmdL)
{
   for (const char **I = CmdL.FileList + 1; *I != 0; I += 2)
   {
      if (I[1] == 0 || strlen(I[1]) == 0)
	 return _error->Error(_("Arguments not in pairs"));

      string key = I[1];
      if (key.end()[-1] == '/') // old directory format
	 key.append("d");

      if (_config->ExistsAny(key.c_str()))
	 cout << *I << "='" << 
	         SubstVar(_config->FindAny(key.c_str()),"'","'\\''") << '\'' << endl;
      
   }
   
   return true;
}
									/*}}}*/
// DoDump - Dump the configuration space				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoDump(CommandLine &CmdL)
{
   _config->Dump(cout);
   return true;
}
									/*}}}*/
// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return 0;
   
   cout <<
    _("Usage: apt-config [options] command\n"
      "\n"
      "apt-config is a simple tool to read the APT config file\n"
      "\n"
      "Commands:\n"
      "   shell - Shell mode\n"
      "   dump - Show the configuration\n"
      "\n"
      "Options:\n"
      "  -h   This help text.\n" 
      "  -c=? Read this configuration file\n" 
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n");
   return 0;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {{"shell",&DoShell},
                                   {"dump",&DoDump},
                                   {0,0}};

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Match the operation
   CmdL.DispatchArg(Cmds);
   
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;
}
