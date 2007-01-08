// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
// $Id: http.h,v 1.12 2002/04/18 05:09:38 jgg Exp $
/* ######################################################################

   MIRROR Aquire Method - This is the MIRROR aquire method for APT.

   ##################################################################### */
									/*}}}*/

#ifndef APT_MIRROR_H
#define APT_MIRROR_H


#include <iostream>

using std::cout;
using std::cerr;
using std::endl;

#include "http.h"

class MirrorMethod : public HttpMethod
{
   FetchResult Res;
   // we simply transform between BaseUri and Mirror
   string BaseUri;    // the original mirror://... url
   string Mirror;     // the selected mirror uri (http://...)
   string MirrorFile; // 
   bool HasMirrorFile;

   bool Debug;

 protected:
   bool GetMirrorFile(string uri);
   bool SelectMirror();
   bool Clean(string dir);
   
   // we need to overwrite those to transform the url back
   virtual void Fail(string Why, bool Transient = false);
   virtual void URIStart(FetchResult &Res);
   virtual void URIDone(FetchResult &Res,FetchResult *Alt = 0);
   virtual bool Configuration(string Message);

 public:
   MirrorMethod();
   virtual bool Fetch(FetchItem *Itm);
};


#endif
