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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

using namespace std;

#include "mirror.h"
#include "http.h"
#include "apti18n.h"
									/*}}}*/

/* 
 * TODO: 
 * - send expected checksum to the mirror method so that 
     some checking/falling back can be done here already
 * - keep the mirror file around in /var/lib/apt/mirrors
     * can't be put into lists/ because of the listclearer
     * cleanup by time (mtime relative to the other mtimes)
 * - use a TTL time the mirror file is fetched again (6h?)
 * - deal with runing as non-root because we can't write to the lists 
     dir then -> use the cached mirror file
 * - better method to download than having a pkgAcquire interface here
 * - magicmarker is (a bit) evil
 * - testing :)
 */

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

// clean the mirrors dir based on ttl information
bool MirrorMethod::Clean(string Dir)
{
   // FIXME: it would better to have a global idea of the mirrors
   //        in the sources.list and use this instead of this time
   //        based approach. currently apt does not support this :/

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
      
      // Del everything not touched for MaxAge days
      time_t t,now,max;
      struct stat buf;
      if(stat(Dir->d_name, &buf) != 0) 
      {
	 cerr << "Can't stat '" << Dir->d_name << "'" << endl;
	 continue;
      }
      t = std::max(buf.st_mtime, buf.st_ctime);
      now = time(NULL);
      max = 24*60*60*_config->FindI("Acquire::Mirror::MaxAge",90);
      if(t + max < now)
      {
	 if(Debug)
	    clog << "Mirror file is older than MaxAge days, deleting" << endl;
	 unlink(Dir->d_name);
      }
   };
   
   chdir(StartDir.c_str());
   closedir(D);
   return true;   
}


bool MirrorMethod::GetMirrorFile(string uri)
{
   string Marker = _config->Find("Acquire::Mirror::MagicMarker","///");
   BaseUri = uri.substr(0,uri.find(Marker));

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


