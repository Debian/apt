// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: mirror.cc,v 1.59 2004/05/08 19:42:35 mdz Exp $
/* ######################################################################

   Mirror Aquire Method - This is the Mirror aquire method for APT.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>

#include <fstream>
#include <iostream>
#include <stdarg.h>

using namespace std;

#include "mirror.h"
#include "http.h"

									/*}}}*/

MirrorMethod::MirrorMethod()
   : HttpMethod(), HasMirrorFile(false)
{
#if 0
   HasMirrorFile=true;
   BaseUri="mirror://people.ubuntu.com/~mvo/mirror/mirrors";
   MirrorFile="/var/lib/apt/lists/people.ubuntu.com_%7emvo_apt_mirror_mirrors";
   Mirror="http://de.archive.ubuntu.com/ubuntu/";
#endif
};

// HttpMethod::Configuration - Handle a configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* We stash the desired pipeline depth */
bool MirrorMethod::Configuration(string Message)
{
   if (pkgAcqMethod::Configuration(Message) == false)
      return false;
   Debug = _config->FindB("Debug::Acquire::mirror",false);
   
   return true;
}
									/*}}}*/


bool MirrorMethod::GetMirrorFile(string uri)
{
   string Marker = _config->Find("Acquire::Mirror::MagicMarker","///");
   BaseUri = uri.substr(0,uri.find(Marker));

   string fetch = BaseUri;
   fetch.replace(0,strlen("mirror://"),"http://");

   MirrorFile = _config->FindDir("Dir::State::lists") + URItoFileName(BaseUri);

   if(Debug) 
   {
      cerr << "base-uri: " << BaseUri << endl;
      cerr << "mirror-file: " << MirrorFile << endl;
   }

   // FIXME: fetch it with curl
   pkgAcquire Fetcher;
   new pkgAcqFile(&Fetcher, fetch, "", 0, "", "", "", MirrorFile);
   bool res = (Fetcher.Run() == pkgAcquire::Continue);
   
   if(res) 
      HasMirrorFile = true;
   Fetcher.Shutdown();
   return true;
}

bool MirrorMethod::SelectMirror()
{
   ifstream in(MirrorFile.c_str());
   getline(in, Mirror);
   if(Debug)
      cerr << "Using mirror: " << Mirror << endl;
   return true;
}

// MirrorMethod::Fetch - Fetch an item					/*{{{*/
// ---------------------------------------------------------------------
/* This adds an item to the pipeline. We keep the pipeline at a fixed
   depth. */
bool MirrorMethod::Fetch(FetchItem *Itm)
{
   // get mirror information
   if(!HasMirrorFile)
   {
      GetMirrorFile(Itm->Uri);
      SelectMirror();
   }

   if(Queue->Uri.find("mirror://") != string::npos)
      Queue->Uri.replace(0,BaseUri.size(),Mirror);

   // now run the real fetcher
   return HttpMethod::Fetch(Itm);
};

void MirrorMethod::Fail(string Err,bool Transient)
{
   if(Queue->Uri.find("http://") != string::npos)
      Queue->Uri.replace(0,Mirror.size(), BaseUri);
   pkgAcqMethod::Fail(Err, Transient);
}

void MirrorMethod::URIStart(FetchResult &Res)
{
   if(Queue->Uri.find("http://") != string::npos)
      Queue->Uri.replace(0,Mirror.size(), BaseUri);
   pkgAcqMethod::URIStart(Res);
}

void MirrorMethod::URIDone(FetchResult &Res,FetchResult *Alt)
{
   if(Queue->Uri.find("http://") != string::npos)
      Queue->Uri.replace(0,Mirror.size(), BaseUri);
   pkgAcqMethod::URIDone(Res, Alt);
}


int main()
{
   setlocale(LC_ALL, "");

   MirrorMethod Mth;

   return Mth.Loop();
}


