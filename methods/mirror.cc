// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Mirror URI – This method helps avoiding hardcoding of mirrors in the
   sources.lists by looking up a list of mirrors first to which the
   following requests are redirected.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include "aptmethod.h"
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>

#include <functional>
#include <random>
#include <string>
#include <unordered_map>

#include <sys/utsname.h>

#include <apti18n.h>
									/*}}}*/

static void sortByLength(std::vector<std::string> &vec)			/*{{{*/
{
   // this ensures having mirror://foo/ and mirror://foo/bar/ works as expected
   // by checking for the longest matches first
   std::sort(vec.begin(), vec.end(), [](std::string const &a, std::string const &b) {
      return a.length() > b.length();
   });
}
									/*}}}*/
class MirrorMethod : public aptMethod /*{{{*/
{
   std::vector<std::string> sourceslist;
   enum MirrorFileState
   {
      REQUESTED,
      FAILED,
      AVAILABLE
   };
   struct MirrorInfo
   {
      MirrorFileState state;
      std::string baseuri;
      std::vector<std::string> list;
   };
   std::unordered_map<std::string, MirrorInfo> mirrorfilestate;
   unsigned int seedvalue;

   virtual bool URIAcquire(std::string const &Message, FetchItem *Itm) APT_OVERRIDE;

   void RedirectItem(MirrorInfo const &info, FetchItem *const Itm);
   bool MirrorListFileRecieved(MirrorInfo &info, FetchItem *const Itm);
   std::string GetMirrorFileURI(std::string const &Message, FetchItem *const Itm);
   void DealWithPendingItems(std::vector<std::string> const &baseuris, MirrorInfo const &info, FetchItem *const Itm, std::function<void()> handler);

   public:
   MirrorMethod(std::string &&pProg) : aptMethod(std::move(pProg), "2.0", SingleInstance | Pipeline | SendConfig)
   {
      SeccompFlags = aptMethod::BASE | aptMethod::DIRECTORY;

      // we want the file to be random for each different machine, but also
      // "stable" on the same machine to avoid issues like picking different
      // mirrors in different states for indexes and deb downloads
      struct utsname buf;
      seedvalue = 1;
      if (uname(&buf) == 0)
      {
	 for (size_t i = 0; buf.nodename[i] != '\0'; ++i)
	    seedvalue = seedvalue * 31 + buf.nodename[i];
      }
   }
};
									/*}}}*/
void MirrorMethod::RedirectItem(MirrorInfo const &info, FetchItem *const Itm) /*{{{*/
{
   std::string const path = Itm->Uri.substr(info.baseuri.length());
   std::string altMirrors;
   std::unordered_map<std::string, std::string> fields;
   fields.emplace("URI", Queue->Uri);
   for (auto curMirror = info.list.cbegin(); curMirror != info.list.cend(); ++curMirror)
   {
      std::string mirror = *curMirror;
      if (APT::String::Endswith(mirror, "/") == false)
	 mirror.append("/");
      mirror.append(path);
      if (curMirror == info.list.cbegin())
	 fields.emplace("New-URI", mirror);
      else if (altMirrors.empty())
	 altMirrors.append(mirror);
      else
	 altMirrors.append("\n").append(mirror);
   }
   fields.emplace("Alternate-URIs", altMirrors);
   SendMessage("103 Redirect", std::move(fields));
   Dequeue();
}
									/*}}}*/
void MirrorMethod::DealWithPendingItems(std::vector<std::string> const &baseuris, /*{{{*/
					MirrorInfo const &info, FetchItem *const Itm,
					std::function<void()> handler)
{
   FetchItem **LastItm = &Itm->Next;
   while (*LastItm != nullptr)
      LastItm = &((*LastItm)->Next);
   while (Queue != Itm)
   {
      if (APT::String::Startswith(Queue->Uri, info.baseuri) == false ||
	  std::any_of(baseuris.cbegin(), baseuris.cend(), [&](std::string const &b) { return APT::String::Startswith(Queue->Uri, b); }))
      {
	 // move the item behind the aux file not related to it
	 *LastItm = Queue;
	 Queue = QueueBack = Queue->Next;
	 (*LastItm)->Next = nullptr;
	 LastItm = &((*LastItm)->Next);
      }
      else
      {
	 handler();
      }
   }
   // now remove out trigger
   QueueBack = Queue = Queue->Next;
   delete Itm;
}
									/*}}}*/
bool MirrorMethod::MirrorListFileRecieved(MirrorInfo &info, FetchItem *const Itm) /*{{{*/
{
   std::vector<std::string> baseuris;
   for (auto const &i : mirrorfilestate)
      if (info.baseuri.length() < i.second.baseuri.length() &&
	  i.second.state == REQUESTED &&
	  APT::String::Startswith(i.second.baseuri, info.baseuri))
	 baseuris.push_back(i.second.baseuri);
   sortByLength(baseuris);

   FileFd mirrorlist;
   if (FileExists(Itm->DestFile) && mirrorlist.Open(Itm->DestFile, FileFd::ReadOnly, FileFd::Extension))
   {
      std::string mirror;
      while (mirrorlist.ReadLine(mirror))
      {
	 if (mirror.empty() || mirror[0] == '#')
	    continue;
	 info.list.push_back(mirror);
      }
      mirrorlist.Close();
      // we reseed each time to avoid "races" with multiple mirror://s
      std::mt19937 g(seedvalue);
      std::shuffle(info.list.begin(), info.list.end(), g);

      if (info.list.empty())
      {
	 info.state = FAILED;
	 DealWithPendingItems(baseuris, info, Itm, [&]() {
	    std::string msg;
	    strprintf(msg, "Mirror list %s is empty for %s", Itm->DestFile.c_str(), Queue->Uri.c_str());
	    Fail(msg, false);
	 });
      }
      else
      {
	 info.state = AVAILABLE;
	 DealWithPendingItems(baseuris, info, Itm, [&]() {
	    RedirectItem(info, Queue);
	 });
      }
   }
   else
   {
      info.state = FAILED;
      DealWithPendingItems(baseuris, info, Itm, [&]() {
	 std::string msg;
	 strprintf(msg, "Downloading mirror file %s failed for %s", Itm->DestFile.c_str(), Queue->Uri.c_str());
	 Fail(msg, false);
      });
   }
   return true;
}
									/*}}}*/
std::string MirrorMethod::GetMirrorFileURI(std::string const &Message, FetchItem *const Itm) /*{{{*/
{
   if (APT::String::Startswith(Itm->Uri, Binary))
   {
      std::string const repouri = LookupTag(Message, "Target-Repo-Uri");
      if (repouri.empty() == false && std::find(sourceslist.cbegin(), sourceslist.cend(), repouri) == sourceslist.cend())
	 sourceslist.push_back(repouri);
   }
   if (sourceslist.empty())
   {
      // read sources.list and find the matching base uri
      pkgSourceList sl;
      if (sl.ReadMainList() == false)
      {
	 _error->Error(_("The list of sources could not be read."));
	 return "";
      }
      std::string const needle = Binary + ":";
      for (auto const &SL : sl)
      {
	 std::string uristr = SL->GetURI();
	 if (APT::String::Startswith(uristr, needle))
	    sourceslist.push_back(uristr);
      }
      sortByLength(sourceslist);
   }
   for (auto uristr : sourceslist)
   {
      if (APT::String::Startswith(Itm->Uri, uristr))
      {
	 uristr.erase(uristr.length() - 1); // remove the ending '/'
	 auto const colon = uristr.find(':');
	 if (unlikely(colon == std::string::npos))
	    continue;
	 auto const plus = uristr.find("+");
	 if (plus < colon)
	    return uristr.substr(plus + 1);
	 else
	 {
	    uristr.replace(0, strlen("mirror"), "http");
	    return uristr;
	 }
      }
   }
   return "";
}
									/*}}}*/
bool MirrorMethod::URIAcquire(std::string const &Message, FetchItem *Itm) /*{{{*/
{
   auto mirrorinfo = mirrorfilestate.find(Itm->Uri);
   if (mirrorinfo != mirrorfilestate.end())
      return MirrorListFileRecieved(mirrorinfo->second, Itm);

   std::string const mirrorfileuri = GetMirrorFileURI(Message, Itm);
   if (mirrorfileuri.empty())
   {
      _error->Error("Couldn't determine mirror list to query for %s", Itm->Uri.c_str());
      return false;
   }
   if (DebugEnabled())
      std::clog << "Mirror-URI: " << mirrorfileuri << " for " << Itm->Uri << std::endl;

   // have we requested this mirror file already?
   auto const state = mirrorfilestate.find(mirrorfileuri);
   if (state == mirrorfilestate.end())
   {
      MirrorInfo info;
      info.state = REQUESTED;
      info.baseuri = mirrorfileuri + '/';
      auto const colon = info.baseuri.find(':');
      if (unlikely(colon == std::string::npos))
	 return false;
      info.baseuri.replace(0, colon, Binary);
      mirrorfilestate[mirrorfileuri] = info;
      std::unordered_map<std::string, std::string> fields;
      fields.emplace("URI", Itm->Uri);
      fields.emplace("MaximumSize", std::to_string(1 * 1024 * 1024)); //FIXME: 1 MB is enough for everyone
      fields.emplace("Aux-ShortDesc", "Mirrorlist");
      fields.emplace("Aux-Description", mirrorfileuri + " Mirrorlist");
      fields.emplace("Aux-Uri", mirrorfileuri);
      SendMessage("351 Aux Request", std::move(fields));
      return true;
   }

   switch (state->second.state)
   {
   case REQUESTED:
      // lets wait for the requested mirror file
      return true;
   case FAILED:
      Fail("Downloading mirror file failed", false);
      return true;
   case AVAILABLE:
      RedirectItem(state->second, Itm);
      return true;
   }
   return false;
}
									/*}}}*/

int main(int, const char *argv[])
{
   return MirrorMethod(flNotDir(argv[0])).Run();
}
