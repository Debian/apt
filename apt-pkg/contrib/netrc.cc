// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   netrc file parser - returns the login and password of a give host in
                       a specified netrc-type file

   Originally written by Daniel Stenberg, <daniel@haxx.se>, et al. and
   placed into the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/
#include <config.h>
#include <apti18n.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <iostream>

#include "netrc.h"

/* Get user and password from .netrc when given a machine name */
bool MaybeAddAuth(FileFd &NetRCFile, URI &Uri)
{
   if (Uri.User.empty() == false || Uri.Password.empty() == false)
      return true;
   if (NetRCFile.IsOpen() == false || NetRCFile.Failed())
      return false;
   auto const Debug = _config->FindB("Debug::Acquire::netrc", false);

   std::string lookfor;
   if (Uri.Port != 0)
      strprintf(lookfor, "%s:%i%s", Uri.Host.c_str(), Uri.Port, Uri.Path.c_str());
   else
      lookfor.append(Uri.Host).append(Uri.Path);

   enum
   {
      NO,
      MACHINE,
      GOOD_MACHINE,
      LOGIN,
      PASSWORD
   } active_token = NO;
   std::string line;
   while (NetRCFile.Eof() == false || line.empty() == false)
   {
      bool protocolSpecified = false;

      if (line.empty())
      {
	 if (NetRCFile.ReadLine(line) == false)
	    break;
	 else if (line.empty())
	    continue;
      }
      auto tokenend = line.find_first_of("\t ");
      std::string token;
      if (tokenend != std::string::npos)
      {
	 token = line.substr(0, tokenend);
	 line.erase(0, tokenend + 1);
      }
      else
	 std::swap(line, token);
      if (token.empty())
	 continue;
      switch (active_token)
      {
      case NO:
	 if (token == "machine")
	    active_token = MACHINE;
	 break;
      case MACHINE:
	 // If token contains a protocol: Check it first, and strip it away if
	 // it matches. If it does not match, ignore this stanza.
	 // If there is no protocol, only allow https protocols.
	 protocolSpecified = token.find("://") != std::string::npos;
	 if (protocolSpecified)
	 {
	    if (not APT::String::Startswith(token, Uri.Access + "://"))
	    {
	       active_token = NO;
	       break;
	    }
	    token.erase(0, Uri.Access.length() + 3);
	 }

	 if (token.find('/') == std::string::npos)
	 {
	    if (Uri.Port != 0 && Uri.Host == token)
	       active_token = GOOD_MACHINE;
	    else if (lookfor.compare(0, lookfor.length() - Uri.Path.length(), token) == 0)
	       active_token = GOOD_MACHINE;
	    else
	       active_token = NO;
	 }
	 else
	 {
	    if (APT::String::Startswith(lookfor, token))
	       active_token = GOOD_MACHINE;
	    else
	       active_token = NO;
	 }

	 if (active_token == GOOD_MACHINE && not protocolSpecified)
	 {
	    if (Uri.Access != "https" && Uri.Access != "tor+https")
	    {
	       _error->Warning(_("%s: Credentials for %s match, but the protocol is not encrypted. Annotate with %s:// to use."), NetRCFile.Name().c_str(), token.c_str(), Uri.Access.c_str());
	       active_token = NO;
	    }
	 }
	 break;
      case GOOD_MACHINE:
	 if (token == "login")
	    active_token = LOGIN;
	 else if (token == "password")
	    active_token = PASSWORD;
	 else if (token == "machine")
	 {
	    if (Debug)
	       std::clog << "MaybeAddAuth: Found matching host adding '" << Uri.User << "' and '" << Uri.Password << "' for "
			 << (std::string)Uri << " from " << NetRCFile.Name() << std::endl;
	    return true;
	 }
	 break;
      case LOGIN:
	 std::swap(Uri.User, token);
	 active_token = GOOD_MACHINE;
	 break;
      case PASSWORD:
	 std::swap(Uri.Password, token);
	 active_token = GOOD_MACHINE;
	 break;
      }
   }
   if (active_token == GOOD_MACHINE)
   {
      if (Debug)
	 std::clog << "MaybeAddAuth: Found matching host adding '" << Uri.User << "' and '" << Uri.Password << "' for "
		   << (std::string)Uri << " from " << NetRCFile.Name() << std::endl;
      return true;
   }
   else if (active_token == NO)
   {
      if (Debug)
	 std::clog << "MaybeAddAuth: Found no matching host for "
		   << (std::string)Uri << " from " << NetRCFile.Name() << std::endl;
      return true;
   }
   else if (Debug)
   {
      std::clog << "MaybeAddAuth: Found no matching host (syntax error: token:";
      switch (active_token)
      {
	 case NO: std::clog << "NO"; break;
	 case MACHINE: std::clog << "MACHINE"; break;
	 case GOOD_MACHINE: std::clog << "GOOD_MACHINE"; break;
	 case LOGIN: std::clog << "LOGIN"; break;
	 case PASSWORD: std::clog << "PASSWORD"; break;
      }
      std::clog << ") for " << (std::string)Uri << " from " << NetRCFile.Name() << std::endl;
   }
   return false;
}
