// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Proxy - Proxy related functions
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

#include "proxy.h"
									/*}}}*/

bool CanURIBeAccessedViaProxy(URI const &URL)				/*{{{*/
{
   // for some methods a proxy doesn't make sense, so we don't have to fork
   if (URL.Host.empty() ||
       APT::String::Startswith(URL.Access, "mirror+") || URL.Access.find("+mirror+") != std::string::npos || APT::String::Endswith(URL.Access, "+mirror"))
      return false;
   std::array const noproxy{"file", "copy", "store", "gpgv", "rred", "cdrom", "mirror"};
   return std::find(noproxy.begin(), noproxy.end(), URL.Access) == noproxy.end();
}
									/*}}}*/
// AutoDetectProxy - auto detect proxy					/*{{{*/
// ---------------------------------------------------------------------
/* */
static std::vector<std::string> CompatibleProxies(URI const &URL)
{
   if (URL.Access == "http" || URL.Access == "https")
      return {"http", "https", "socks5h"};
   if (URL.Access == "tor" || URL.Access == "tor+http" || URL.Access == "tor+https")
      return {"socks5h"};
   if (URL.Access == "ftp")
      return {"ftp"};
   return {};
}
bool AutoDetectProxy(URI &URL)
{
   if (not CanURIBeAccessedViaProxy(URL))
      return true;

   // the user already explicitly set a proxy for this host
   if (not _config->Find("Acquire::" + URL.Access + "::proxy::" + URL.Host, "").empty())
      return true;

   // option is "Acquire::http::Proxy-Auto-Detect" but we allow the old
   // name without the dash ("-")
   std::string AutoDetectProxyCmd = _config->Find("Acquire::"+URL.Access+"::Proxy-Auto-Detect",
                                      _config->Find("Acquire::"+URL.Access+"::ProxyAutoDetect"));

   if (AutoDetectProxyCmd.empty())
      return true;

   bool const Debug = _config->FindB("Debug::Acquire::" + URL.Access, false);
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
   APT::StringView const cleanedbuf = _strstrip(buf);
   // We warn about this as the implementor probably meant to use DIRECT instead
   if (cleanedbuf.empty())
   {
      _error->Warning("ProxyAutoDetect command returned an empty line");
      return true;
   }

   if (Debug)
      std::clog << "auto detect command returned: '" << cleanedbuf.data() << "'" << std::endl;

   bool compatible = true;
   if (cleanedbuf != "DIRECT")
   {
      if (auto const compatibleTypes = CompatibleProxies(URL); not compatibleTypes.empty())
	 compatible = std::any_of(compatibleTypes.begin(), compatibleTypes.end(),
				  [cleanedbuf](std::string const &compat)
				  {
				     return cleanedbuf.substr(0, compat.size()) == compat;
				  });
   }
   else if (URL.Access == "tor" || URL.Access == "tor+http" || URL.Access == "tor+https")
      compatible = false; // Accepting DIRECT would silently disable tor

   if (compatible)
      _config->Set("Acquire::"+URL.Access+"::proxy::"+URL.Host, cleanedbuf.data());
   else
      _error->Warning("ProxyAutoDetect command returned incompatible proxy '%s' for access type %s", cleanedbuf.data(), URL.Access.c_str());

   return true;
}
									/*}}}*/
