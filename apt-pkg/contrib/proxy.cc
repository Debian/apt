// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Proxy - Proxy releated functions
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<apt-pkg/configuration.h>
#include<apt-pkg/error.h>
#include<apt-pkg/fileutl.h>
#include<apt-pkg/strutl.h>

#include<iostream>
#include <unistd.h>

#include "proxy.h"


// AutoDetectProxy - auto detect proxy          			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool AutoDetectProxy(URI &URL)
{
   // we support both http/https debug options
   bool Debug = _config->FindB("Debug::Acquire::"+URL.Access,false);

   // the user already explicitly set a proxy for this host
   if(_config->Find("Acquire::"+URL.Access+"::proxy::"+URL.Host, "") != "")
      return true;

   // option is "Acquire::http::Proxy-Auto-Detect" but we allow the old
   // name without the dash ("-")
   std::string AutoDetectProxyCmd = _config->Find("Acquire::"+URL.Access+"::Proxy-Auto-Detect",
                                      _config->Find("Acquire::"+URL.Access+"::ProxyAutoDetect"));

   if (AutoDetectProxyCmd.empty())
      return true;

   if (Debug)
      std::clog << "Using auto proxy detect command: " << AutoDetectProxyCmd << std::endl;

   int Pipes[2] = {-1,-1};
   if (pipe(Pipes) != 0)
      return _error->Errno("pipe", "Failed to create Pipe");

   pid_t Process = ExecFork();
   if (Process == 0)
   {
      close(Pipes[0]);
      dup2(Pipes[1],STDOUT_FILENO);
      SetCloseExec(STDOUT_FILENO,false);

      std::string foo = URL;
      const char *Args[4];
      Args[0] = AutoDetectProxyCmd.c_str();
      Args[1] = foo.c_str();
      Args[2] = 0;
      execv(Args[0],(char **)Args);
      std::cerr << "Failed to exec method " << Args[0] << std::endl;
      _exit(100);
   }
   char buf[512];
   int InFd = Pipes[0];
   close(Pipes[1]);
   int res = read(InFd, buf, sizeof(buf)-1);
   ExecWait(Process, "ProxyAutoDetect", true);

   if (res < 0)
      return _error->Errno("read", "Failed to read");
   if (res == 0)
      return _error->Warning("ProxyAutoDetect returned no data");

   // add trailing \0
   buf[res] = 0;

   if (Debug)
      std::clog << "auto detect command returned: '" << buf << "'" << std::endl;

   if (strstr(buf, URL.Access.c_str()) == buf)
      _config->Set("Acquire::"+URL.Access+"::proxy::"+URL.Host, _strstrip(buf));

   return true;
}
									/*}}}*/
