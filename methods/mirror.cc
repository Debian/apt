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
   : HttpMethod(), HasMirrorFile(false)
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
	 if(uri.substr(0,strlen("mirror://")) != string("mirror://"))
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


bool MirrorMethod::GetMirrorFile(string mirror_uri_str)
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
   if(Debug)
      std::cerr << "GetMirrorFile: " << mirror_uri_str << std::endl;

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
   string fetch = BaseUri;
   fetch.replace(0,strlen("mirror://"),"http://");

   // get new file
   MirrorFile = _config->FindDir("Dir::State::mirrors") + URItoFileName(BaseUri);

   if(Debug) 
   {
      cerr << "base-uri: " << BaseUri << endl;
      cerr << "mirror-file: " << MirrorFile << endl;
   }

   // check the file, if it is not older than RefreshInterval just use it
   // otherwise try to get a new one
   if(FileExists(MirrorFile)) 
   {
      struct stat buf;
      time_t t,now,refresh;
      if(stat(MirrorFile.c_str(), &buf) != 0)
	 return false;
      t = std::max(buf.st_mtime, buf.st_ctime);
      now = time(NULL);
      refresh = 60*_config->FindI("Acquire::Mirror::RefreshInterval",360);
      if(t + refresh > now)
      {
	 if(Debug)
	    clog << "Mirror file is in RefreshInterval" << endl;
	 HasMirrorFile = true;
	 return true;
      }
      if(Debug)
	 clog << "Mirror file " << MirrorFile << " older than " << refresh << "min, re-download it" << endl;
   }

   // not that great to use pkgAcquire here, but we do not have 
   // any other way right now
   pkgAcquire Fetcher;
   new pkgAcqFile(&Fetcher, fetch, "", 0, "", "", "", MirrorFile);
   bool res = (Fetcher.Run() == pkgAcquire::Continue);
   if(res)
      HasMirrorFile = true;
   Fetcher.Shutdown();
   return res;
}

bool MirrorMethod::SelectMirror()
{
   // FIXME: make the mirror selection more clever, do not 
   //        just use the first one!
   ifstream in(MirrorFile.c_str());
   getline(in, Mirror);
   if(Debug)
      cerr << "Using mirror: " << Mirror << endl;

   UsedMirror = Mirror;
   return true;
}

// MirrorMethod::Fetch - Fetch an item					/*{{{*/
// ---------------------------------------------------------------------
/* This adds an item to the pipeline. We keep the pipeline at a fixed
   depth. */
bool MirrorMethod::Fetch(FetchItem *Itm)
{
   // select mirror only once per session
   if(!HasMirrorFile)
   {
      Clean(_config->FindDir("Dir::State::mirrors"));
      GetMirrorFile(Itm->Uri);
      SelectMirror();
   }

   for (FetchItem *I = Queue; I != 0; I = I->Next)
   {
      if(I->Uri.find("mirror://") != string::npos)
	 I->Uri.replace(0,BaseUri.size(),Mirror);
   }

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


