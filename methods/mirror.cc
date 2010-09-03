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
#include <apt-pkg/sourcelist.h>

#include <fstream>
#include <iostream>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

using namespace std;

#include<sstream>

#include "mirror.h"
#include "http.h"
#include "apti18n.h"
									/*}}}*/

/* Done:
 * - works with http (only!)
 * - always picks the first mirror from the list
 * - call out to problem reporting script
 * - supports "deb mirror://host/path/to/mirror-list/// dist component"
 * - uses pkgAcqMethod::FailReason() to have a string representation
 *   of the failure that is also send to LP
 * 
 * TODO: 
 * - deal with runing as non-root because we can't write to the lists 
     dir then -> use the cached mirror file
 * - better method to download than having a pkgAcquire interface here
 *   and better error handling there!
 * - support more than http
 * - testing :)
 */

MirrorMethod::MirrorMethod()
   : HttpMethod(), DownloadedMirrorFile(false)
{
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

// clean the mirrors dir based on ttl information
bool MirrorMethod::Clean(string Dir)
{
   vector<metaIndex *>::const_iterator I;

   if(Debug)
      clog << "MirrorMethod::Clean(): " << Dir << endl;

   if(Dir == "/")
      return _error->Error("will not clean: '/'");

   // read sources.list
   pkgSourceList list;
   list.ReadMainList();

   DIR *D = opendir(Dir.c_str());   
   if (D == 0)
      return _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());
   
   string StartDir = SafeGetCWD();
   if (chdir(Dir.c_str()) != 0)
   {
      closedir(D);
      return _error->Errno("chdir",_("Unable to change to %s"),Dir.c_str());
   }
   
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,"lock") == 0 ||
	  strcmp(Dir->d_name,"partial") == 0 ||
	  strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;

      // see if we have that uri
      for(I=list.begin(); I != list.end(); I++)
      {
	 string uri = (*I)->GetURI();
	 if(uri.find("mirror://") != 0)
	    continue;
	 string BaseUri = uri.substr(0,uri.size()-1);
	 if (URItoFileName(BaseUri) == Dir->d_name)
	    break;
      }
      // nothing found, nuke it
      if (I == list.end())
	 unlink(Dir->d_name);
   };
   
   chdir(StartDir.c_str());
   closedir(D);
   return true;   
}


bool MirrorMethod::DownloadMirrorFile(string mirror_uri_str)
{
   if(Debug)
      clog << "MirrorMethod::DownloadMirrorFile(): " << endl;

   // not that great to use pkgAcquire here, but we do not have 
   // any other way right now
   string fetch = BaseUri;
   fetch.replace(0,strlen("mirror://"),"http://");

   pkgAcquire Fetcher;
   new pkgAcqFile(&Fetcher, fetch, "", 0, "", "", "", MirrorFile);
   bool res = (Fetcher.Run() == pkgAcquire::Continue);
   if(res)
      DownloadedMirrorFile = true;
   Fetcher.Shutdown();
   return res;
}

/* convert a the Queue->Uri back to the mirror base uri and look
 * at all mirrors we have for this, this is needed as queue->uri
 * may point to different mirrors (if TryNextMirror() was run)
 */
void MirrorMethod::CurrentQueueUriToMirror()
{
   // already in mirror:// style so nothing to do
   if(Queue->Uri.find("mirror://") == 0)
      return;

   // find current mirror and select next one
   for (vector<string>::const_iterator mirror = AllMirrors.begin();
	mirror != AllMirrors.end(); ++mirror)
   {
      if (Queue->Uri.find(*mirror) == 0)
      {
	 Queue->Uri.replace(0, mirror->length(), BaseUri);
	 return;
      }
   }
   _error->Error("Internal error: Failed to convert %s back to %s",
		 Queue->Uri.c_str(), BaseUri.c_str());
}

bool MirrorMethod::TryNextMirror()
{
   // find current mirror and select next one
   for (vector<string>::const_iterator mirror = AllMirrors.begin();
	mirror != AllMirrors.end(); ++mirror)
   {
      if (Queue->Uri.find(*mirror) != 0)
	 continue;

      vector<string>::const_iterator nextmirror = mirror + 1;
      if (nextmirror != AllMirrors.end())
	 break;
      Queue->Uri.replace(0, mirror->length(), *nextmirror);
      if (Debug)
	 clog << "TryNextMirror: " << Queue->Uri << endl;
      return true;
   }

   if (Debug)
      clog << "TryNextMirror could not find another mirror to try" << endl;

   return false;
}

bool MirrorMethod::InitMirrors()
{
   // if we do not have a MirrorFile, fallback
   if(!FileExists(MirrorFile))
   {
      // FIXME: fallback to a default mirror here instead 
      //        and provide a config option to define that default
      return _error->Error(_("No mirror file '%s' found "), MirrorFile.c_str());
   }

   // FIXME: make the mirror selection more clever, do not 
   //        just use the first one!
   // BUT: we can not make this random, the mirror has to be
   //      stable accross session, because otherwise we can
   //      get into sync issues (got indexfiles from mirror A,
   //      but packages from mirror B - one might be out of date etc)
   ifstream in(MirrorFile.c_str());
   string s;
   while (!in.eof()) 
   {
      getline(in, s);
      if (s.size() > 0)
	 AllMirrors.push_back(s);
   }
   Mirror = AllMirrors[0];
   UsedMirror = Mirror;
   return true;
}

string MirrorMethod::GetMirrorFileName(string mirror_uri_str)
{
   /* 
    - a mirror_uri_str looks like this:
    mirror://people.ubuntu.com/~mvo/apt/mirror/mirrors/dists/feisty/Release.gpg
   
    - the matching source.list entry
    deb mirror://people.ubuntu.com/~mvo/apt/mirror/mirrors feisty main
   
    - we actually want to go after:
    http://people.ubuntu.com/~mvo/apt/mirror/mirrors

    And we need to save the BaseUri for later:
    - mirror://people.ubuntu.com/~mvo/apt/mirror/mirrors

   FIXME: what if we have two similar prefixes?
     mirror://people.ubuntu.com/~mvo/mirror
     mirror://people.ubuntu.com/~mvo/mirror2
   then mirror_uri_str looks like:
     mirror://people.ubuntu.com/~mvo/apt/mirror/dists/feisty/Release.gpg
     mirror://people.ubuntu.com/~mvo/apt/mirror2/dists/feisty/Release.gpg
   we search sources.list and find:
     mirror://people.ubuntu.com/~mvo/apt/mirror
   in both cases! So we need to apply some domain knowledge here :( and
   check for /dists/ or /Release.gpg as suffixes
   */
   string name;
   if(Debug)
      std::cerr << "GetMirrorFileName: " << mirror_uri_str << std::endl;

   // read sources.list and find match
   vector<metaIndex *>::const_iterator I;
   pkgSourceList list;
   list.ReadMainList();
   for(I=list.begin(); I != list.end(); I++)
   {
      string uristr = (*I)->GetURI();
      if(Debug)
	 std::cerr << "Checking: " << uristr << std::endl;
      if(uristr.substr(0,strlen("mirror://")) != string("mirror://"))
	 continue;
      // find matching uri in sources.list
      if(mirror_uri_str.substr(0,uristr.size()) == uristr)
      {
	 if(Debug)
	    std::cerr << "found BaseURI: " << uristr << std::endl;
	 BaseUri = uristr.substr(0,uristr.size()-1);
      }
   }
   // get new file
   name = _config->FindDir("Dir::State::mirrors") + URItoFileName(BaseUri);

   if(Debug) 
   {
      cerr << "base-uri: " << BaseUri << endl;
      cerr << "mirror-file: " << name << endl;
   }
   return name;
}

// MirrorMethod::Fetch - Fetch an item					/*{{{*/
// ---------------------------------------------------------------------
/* This adds an item to the pipeline. We keep the pipeline at a fixed
   depth. */
bool MirrorMethod::Fetch(FetchItem *Itm)
{
   if(Debug)
      clog << "MirrorMethod::Fetch()" << endl;

   // the http method uses Fetch(0) as a way to update the pipeline,
   // just let it do its work in this case - Fetch() with a valid
   // Itm will always run before the first Fetch(0)
   if(Itm == NULL) 
      return HttpMethod::Fetch(Itm);

   // if we don't have the name of the mirror file on disk yet,
   // calculate it now (can be derived from the uri)
   if(MirrorFile.empty())
      MirrorFile = GetMirrorFileName(Itm->Uri);

  // download mirror file once (if we are after index files)
   if(Itm->IndexFile && !DownloadedMirrorFile)
   {
      Clean(_config->FindDir("Dir::State::mirrors"));
      DownloadMirrorFile(Itm->Uri);
   }

   if(AllMirrors.empty()) {
      if(!InitMirrors()) {
	 // no valid mirror selected, something went wrong downloading
	 // from the master mirror site most likely and there is
	 // no old mirror file availalbe
	 return false;
      }
   }

   if(Itm->Uri.find("mirror://") != string::npos)
      Itm->Uri.replace(0,BaseUri.size(), Mirror);

   if(Debug)
      clog << "Fetch: " << Itm->Uri << endl << endl;
   
   // now run the real fetcher
   return HttpMethod::Fetch(Itm);
};

void MirrorMethod::Fail(string Err,bool Transient)
{
   // FIXME: TryNextMirror is not ideal for indexfile as we may
   //        run into auth issues

   if (Debug)
      clog << "Failure to get " << Queue->Uri << endl;

   // try the next mirror on fail (if its not a expected failure,
   // e.g. translations are ok to ignore)
   if (!Queue->FailIgnore && TryNextMirror()) 
      return;

   // all mirrors failed, so bail out
   string s;
   strprintf(s, _("[Mirror: %s]"), Mirror.c_str());
   SetIP(s);

   CurrentQueueUriToMirror();
   pkgAcqMethod::Fail(Err, Transient);
}

void MirrorMethod::URIStart(FetchResult &Res)
{
   CurrentQueueUriToMirror();
   pkgAcqMethod::URIStart(Res);
}

void MirrorMethod::URIDone(FetchResult &Res,FetchResult *Alt)
{
   CurrentQueueUriToMirror();
   pkgAcqMethod::URIDone(Res, Alt);
}


int main()
{
   setlocale(LC_ALL, "");

   MirrorMethod Mth;

   return Mth.Loop();
}


