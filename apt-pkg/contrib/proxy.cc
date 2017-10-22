// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Proxy - Proxy related functions
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

#include "proxy.h"
									/*}}}*/

// AutoDetectProxy - auto detect proxy					/*{{{*/
// ---------------------------------------------------------------------
/* */
static std::vector<std::string> CompatibleProxies(URI const &URL)
{
   if (URL.Access == "http" || URL.Access == "https")
      return {"http", "https", "socks5h"};
   return {URL.Access};
}

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

   if (faccessat(AT_FDCWD, AutoDetectProxyCmd.c_str(), R_OK | X_OK, AT_EACCESS) != 0)
      return _error->Errno("access", "ProxyAutoDetect command '%s' can not be executed!", AutoDetectProxyCmd.c_str());

   std::string const urlstring = URL;
   std::vector<const char *> Args;
   Args.push_back(AutoDetectProxyCmd.c_str());
   Args.push_back(urlstring.c_str());
   Args.push_back(nullptr);
   FileFd PipeFd;
   pid_t Child;
   if (Popen(&Args[0], PipeFd, Child, FileFd::ReadOnly, false, true) == false)
      return _error->Error("ProxyAutoDetect command '%s' failed!", AutoDetectProxyCmd.c_str());
   char buf[512];
   bool const goodread = PipeFd.ReadLine(buf, sizeof(buf)) != nullptr;
   PipeFd.Close();
   if (ExecWait(Child, "ProxyAutoDetect") == false)
      return false;
   // no output means the detector has no idea which proxy to use
   // and apt will use the generic proxy settings
   if (goodread == false)
      return true;
   auto const cleanedbuf = _strstrip(buf);
   // We warn about this as the implementor probably meant to use DIRECT instead
   if (cleanedbuf[0] == '\0')
   {
      _error->Warning("ProxyAutoDetect command returned an empty line");
      return true;
   }

   if (Debug)
      std::clog << "auto detect command returned: '" << cleanedbuf << "'" << std::endl;

   auto compatibleTypes = CompatibleProxies(URL);
   bool compatible = strcmp(cleanedbuf, "DIRECT") == 0 ||
		     compatibleTypes.end() != std::find_if(compatibleTypes.begin(),
							   compatibleTypes.end(), [cleanedbuf](std::string &compat) {
							      return strstr(cleanedbuf, compat.c_str()) == cleanedbuf;
							   });

   if (compatible)
      _config->Set("Acquire::"+URL.Access+"::proxy::"+URL.Host, cleanedbuf);

   return true;
}
									/*}}}*/
