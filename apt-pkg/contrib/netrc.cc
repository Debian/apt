// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: netrc.c,v 1.38 2007-11-07 09:21:35 bagder Exp $
/* ######################################################################

   netrc file parser - returns the login and password of a give host in
                       a specified netrc-type file

   Originally written by Daniel Stenberg, <daniel@haxx.se>, et al. and
   placed into the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <pwd.h>

#include "netrc.h"

using std::string;

/* Get user and password from .netrc when given a machine name */

enum {
  NOTHING,
  HOSTFOUND,    /* the 'machine' keyword was found */
  HOSTCOMPLETE, /* the machine name following the keyword was found too */
  HOSTVALID,    /* this is "our" machine! */
  HOSTEND /* LAST enum */
};

/* make sure we have room for at least this size: */
#define LOGINSIZE 256
#define PASSWORDSIZE 256
#define NETRC DOT_CHAR "netrc"

/* returns -1 on failure, 0 if the host is found, 1 is the host isn't found */
static int parsenetrc_string (char *host, std::string &login, std::string &password, char *netrcfile = NULL)
{
  FILE *file;
  int retcode = 1;
  int specific_login = (login.empty() == false);
  bool netrc_alloc = false;

  if (!netrcfile) {
    char const * home = getenv ("HOME"); /* portable environment reader */

    if (!home) {
      struct passwd *pw;
      pw = getpwuid (geteuid ());
      if(pw)
        home = pw->pw_dir;
    }

    if (!home)
      return -1;

    if (asprintf (&netrcfile, "%s%s%s", home, DIR_CHAR, NETRC) == -1 || netrcfile == NULL)
      return -1;
    else
      netrc_alloc = true;
  }

  file = fopen (netrcfile, "r");
  if(file) {
    char *tok;
    char *tok_buf;
    bool done = false;
    char *netrcbuffer = NULL;
    size_t netrcbuffer_size = 0;

    int state = NOTHING;
    char state_login = 0;        /* Found a login keyword */
    char state_password = 0;     /* Found a password keyword */
    int state_our_login = false;  /* With specific_login,
				     found *our* login name */

    while (!done && getline(&netrcbuffer, &netrcbuffer_size, file) != -1) {
      tok = strtok_r (netrcbuffer, " \t\n", &tok_buf);
      while (!done && tok) {
        if(login.empty() == false && password.empty() == false) {
          done = true;
          break;
        }

        switch(state) {
        case NOTHING:
          if (!strcasecmp ("machine", tok)) {
            /* the next tok is the machine name, this is in itself the
               delimiter that starts the stuff entered for this machine,
               after this we need to search for 'login' and
               'password'. */
            state = HOSTFOUND;
          }
          break;
        case HOSTFOUND:
	   /* extended definition of a "machine" if we have a "/"
	      we match the start of the string (host.startswith(token) */
	  if ((strchr(host, '/') && strstr(host, tok) == host) ||
	      (!strcasecmp (host, tok))) {
            /* and yes, this is our host! */
            state = HOSTVALID;
            retcode = 0; /* we did find our host */
          }
          else
            /* not our host */
            state = NOTHING;
          break;
        case HOSTVALID:
          /* we are now parsing sub-keywords regarding "our" host */
          if (state_login) {
            if (specific_login)
              state_our_login = !strcasecmp (login.c_str(), tok);
            else
              login = tok;
            state_login = 0;
          } else if (state_password) {
            if (state_our_login || !specific_login)
              password = tok;
            state_password = 0;
          } else if (!strcasecmp ("login", tok))
            state_login = 1;
          else if (!strcasecmp ("password", tok))
            state_password = 1;
          else if(!strcasecmp ("machine", tok)) {
            /* ok, there's machine here go => */
            state = HOSTFOUND;
            state_our_login = false;
          }
          break;
        } /* switch (state) */

        tok = strtok_r (NULL, " \t\n", &tok_buf);
      } /* while(tok) */
    } /* while getline() */

    free(netrcbuffer);
    fclose(file);
  }

  if (netrc_alloc)
    free(netrcfile);

  return retcode;
}

void maybe_add_auth (URI &Uri, string NetRCFile)
{
  if (_config->FindB("Debug::Acquire::netrc", false) == true)
     std::clog << "maybe_add_auth: " << (string)Uri 
	       << " " << NetRCFile << std::endl;
  if (Uri.Password.empty () == true || Uri.User.empty () == true)
  {
    if (NetRCFile.empty () == false)
    {
       std::string login, password;
      char *netrcfile = strdup(NetRCFile.c_str());

      // first check for a generic host based netrc entry
      char *host = strdup(Uri.Host.c_str());
      if (host && parsenetrc_string(host, login, password, netrcfile) == 0)
      {
	 if (_config->FindB("Debug::Acquire::netrc", false) == true)
	    std::clog << "host: " << host 
		      << " user: " << login
		      << " pass-size: " << password.size()
		      << std::endl;
        Uri.User = login;
        Uri.Password = password;
	free(netrcfile);
	free(host);
	return;
      }
      free(host);

      // if host did not work, try Host+Path next, this will trigger
      // a lookup uri.startswith(host) in the netrc file parser (because
      // of the "/"
      char *hostpath = strdup((Uri.Host + Uri.Path).c_str());
      if (hostpath && parsenetrc_string(hostpath, login, password, netrcfile) == 0)
      {
	 if (_config->FindB("Debug::Acquire::netrc", false) == true)
	    std::clog << "hostpath: " << hostpath
		      << " user: " << login
		      << " pass-size: " << password.size()
		      << std::endl;
	 Uri.User = login;
	 Uri.Password = password;
      }
      free(netrcfile);
      free(hostpath);
    }
  }
}

#ifdef DEBUG
int main(int argc, char* argv[])
{
  char login[64] = "";
  char password[64] = "";

  if(argc < 2)
    return -1;

  if(0 == parsenetrc (argv[1], login, password, argv[2])) {
    printf("HOST: %s LOGIN: %s PASSWORD: %s\n", argv[1], login, password);
  }
}
#endif
