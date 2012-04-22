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
#include<config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/pkgsystem.h>

#include <locale.h>
#include <iostream>
#include <string>
#include <vector>

#include <apti18n.h>
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
   bool const empty = _config->FindB("APT::Config::Dump::EmptyValue", true);
   std::string const format = _config->Find("APT::Config::Dump::Format", "%f \"%v\";\n");
   if (CmdL.FileSize() == 1)
      _config->Dump(cout, NULL, format.c_str(), empty);
   else
      for (const char **I = CmdL.FileList + 1; *I != 0; ++I)
	 _config->Dump(cout, *I, format.c_str(), empty);
   return true;
}
									/*}}}*/
// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
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
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,"empty","APT::Config::Dump::EmptyValue",CommandLine::Boolean},
      {0,"format","APT::Config::Dump::Format",CommandLine::HasArg},
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

   std::vector<std::string> const langs = APT::Configuration::getLanguages(true);
   _config->Clear("Acquire::Languages");
   for (std::vector<std::string>::const_iterator l = langs.begin(); l != langs.end(); ++l)
      _config->Set("Acquire::Languages::", *l);

   std::vector<std::string> const archs = APT::Configuration::getArchitectures();
   _config->Clear("APT::Architectures");
   for (std::vector<std::string>::const_iterator a = archs.begin(); a != archs.end(); ++a)
      _config->Set("APT::Architectures::", *a);

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   _config->Clear("APT::Compressor");
   string conf = "APT::Compressor::";
   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressors.begin(); c != compressors.end(); ++c)
   {
      string comp = conf + c->Name + "::";
      _config->Set(comp + "Name", c->Name);
      _config->Set(comp + "Extension", c->Extension);
      _config->Set(comp + "Binary", c->Binary);
      _config->Set(std::string(comp + "Cost").c_str(), c->Cost);
      for (std::vector<std::string>::const_iterator a = c->CompressArgs.begin(); a != c->CompressArgs.end(); ++a)
	 _config->Set(comp + "CompressArg::", *a);
      for (std::vector<std::string>::const_iterator a = c->UncompressArgs.begin(); a != c->UncompressArgs.end(); ++a)
	 _config->Set(comp + "UncompressArg::", *a);
   }

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
									/*}}}*/
