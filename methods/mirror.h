// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   MIRROR Acquire Method - This is the MIRROR acquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_MIRROR_H
#define APT_MIRROR_H

#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::cerr;
using std::endl;

#include "http.h"

class MirrorMethod : public HttpMethod
{
   FetchResult Res;
   // we simply transform between BaseUri and Mirror
   std::string BaseUri;    // the original mirror://... url
   std::string Mirror;     // the selected mirror uri (http://...)
   std::vector<std::string> AllMirrors; // all available mirrors
   std::string MirrorFile; // the file that contains the list of mirrors
   bool DownloadedMirrorFile; // already downloaded this session
   std::string Dist;       // the target distrubtion (e.g. sid, oneiric)

   bool Debug;

 protected:
   bool DownloadMirrorFile(std::string uri);
   bool RandomizeMirrorFile(std::string file);
   std::string GetMirrorFileName(std::string uri);
   bool InitMirrors();
   bool TryNextMirror();
   void CurrentQueueUriToMirror();
   bool Clean(std::string dir);
   
   // we need to overwrite those to transform the url back
   virtual void Fail(std::string Why, bool Transient = false) APT_OVERRIDE;
   virtual void URIStart(FetchResult &Res) APT_OVERRIDE;
   virtual void URIDone(FetchResult &Res,FetchResult *Alt = 0) APT_OVERRIDE;
   virtual bool Configuration(std::string Message) APT_OVERRIDE;

 public:
   MirrorMethod();
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;
};


#endif
