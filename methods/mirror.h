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

class MirrorMethod : public pkgAcqMethod
{
   FetchResult Res;
   string Mirror;
   string BaseUri;
   string MirrorFile;
   bool HasMirrorFile;

 protected:
   bool GetMirrorFile(string uri);
   bool SelectMirror();
 public:
   MirrorMethod();
   virtual bool Fetch(FetchItem *Itm);
};


#endif
