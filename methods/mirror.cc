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
using namespace std;

#include "mirror.h"
#include "http.h"

									/*}}}*/

MirrorMethod::MirrorMethod()
   : pkgAcqMethod("1.2",Pipeline | SendConfig), HasMirrorFile(false)
{
#if 0
   HasMirrorFile=true;
   BaseUri="http://people.ubuntu.com/~mvo/mirror/mirrors///";
   Mirror="http://de.archive.ubuntu.com/ubuntu/";
#endif
};

bool MirrorMethod::GetMirrorFile(string uri)
{
   string Marker = _config->Find("Acquire::Mirror::MagicMarker","///");
   BaseUri = uri.substr(0,uri.find(Marker));
   BaseUri.replace(0,strlen("mirror://"),"http://");

   MirrorFile = _config->FindDir("Dir::State::lists") + URItoFileName(BaseUri);

   cerr << "base-uri: " << BaseUri << endl;
   cerr << "mirror-file: " << MirrorFile << endl;

   // FIXME: fetch it with curl
   pkgAcquire Fetcher;
   new pkgAcqFile(&Fetcher, BaseUri, "", 0, "", "", "", MirrorFile);
   bool res = (Fetcher.Run() == pkgAcquire::Continue);
   cerr << "fetch-result: " << res << endl;
   
   if(res) 
      HasMirrorFile = true;
   Fetcher.Shutdown();
   return true;
}

bool MirrorMethod::SelectMirror()
{
   ifstream in(MirrorFile.c_str());
   getline(in, Mirror);
   cerr << "Mirror: " << Mirror << endl;
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

   // change the items in the queue
   Itm->Uri.replace(0,BaseUri.size()+_config->Find("Acquire::Mirror::MagicMarker","///").size()+2/*len("mirror")-len("http")*/,Mirror);
   cerr << "new Fetch-uri: " << Itm->Uri << endl;

   // FIXME: fetch it with!
   
};

int main()
{
   setlocale(LC_ALL, "");

   MirrorMethod Mth;

   return Mth.Run();
}


