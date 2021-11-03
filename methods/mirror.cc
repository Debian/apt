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
constexpr char const *const disallowLocal[] = {"ftp", "http", "https"};

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
   std::mt19937 genrng;
   std::vector<std::string> sourceslist;
   std::unordered_map<std::string, std::string> msgCache;
   enum MirrorFileState
   {
      REQUESTED,
      FAILED,
      AVAILABLE
   };
   struct MirrorInfo
   {
      std::string uri;
      unsigned long priority = std::numeric_limits<decltype(priority)>::max();
      decltype(genrng)::result_type seed = 0;
      std::unordered_map<std::string, std::vector<std::string>> tags;
      explicit MirrorInfo(std::string const &u, std::vector<std::string> &&ptags = {}) : uri(u)
      {
	 for (auto &&tag : ptags)
	 {
	    auto const colonfound = tag.find(':');
	    if (unlikely(colonfound == std::string::npos))
	       continue;
	    auto name = tag.substr(0, colonfound);
	    auto value = tag.substr(colonfound + 1);
	    if (name == "arch")
	       tags["Architecture"].emplace_back(std::move(value));
	    else if (name == "lang")
	       tags["Language"].emplace_back(std::move(value));
	    else if (name == "priority")
	       priority = std::strtoul(value.c_str(), nullptr, 10);
	    else if (likely(name.empty() == false))
	    {
	       if (name == "codename" || name == "suite")
		  tags["Release"].push_back(value);
	       name[0] = std::toupper(name[0]);
	       tags[std::move(name)].emplace_back(std::move(value));
	    }
	 }
      }
   };
   struct MirrorListInfo
   {
      MirrorFileState state;
      std::string baseuri;
      std::vector<MirrorInfo> list;
   };
   std::unordered_map<std::string, MirrorListInfo> mirrorfilestate;

   virtual bool URIAcquire(std::string const &Message, FetchItem *Itm) APT_OVERRIDE;

   void RedirectItem(MirrorListInfo const &info, FetchItem *const Itm, std::string const &Message);
   bool MirrorListFileReceived(MirrorListInfo &info, FetchItem *const Itm);
   std::string GetMirrorFileURI(std::string const &Message, FetchItem *const Itm);
   void DealWithPendingItems(std::vector<std::string> const &baseuris, MirrorListInfo const &info, FetchItem *const Itm, std::function<void()> handler);

   public:
   explicit MirrorMethod(std::string &&pProg) : aptMethod(std::move(pProg), "2.0", SingleInstance | Pipeline | SendConfig | AuxRequests | SendURIEncoded), genrng(clock())
   {
      SeccompFlags = aptMethod::BASE | aptMethod::DIRECTORY;
   }
};
									/*}}}*/
void MirrorMethod::RedirectItem(MirrorListInfo const &info, FetchItem *const Itm, std::string const &Message) /*{{{*/
{
   std::unordered_map<std::string, std::string> matchers;
   matchers.emplace("Architecture", LookupTag(Message, "Target-Architecture"));
   matchers.emplace("Codename", LookupTag(Message, "Target-Codename"));
   matchers.emplace("Component", LookupTag(Message, "Target-Component"));
   matchers.emplace("Language", LookupTag(Message, "Target-Language"));
   matchers.emplace("Release", LookupTag(Message, "Target-Release"));
   matchers.emplace("Suite", LookupTag(Message, "Target-Suite"));
   matchers.emplace("Type", LookupTag(Message, "Target-Type"));
   decltype(info.list) possMirrors;
   for (auto const &mirror : info.list)
   {
      bool failedMatch = false;
      for (auto const &m : matchers)
      {
	 if (m.second.empty())
	    continue;
	 auto const tagsetiter = mirror.tags.find(m.first);
	 if (tagsetiter == mirror.tags.end())
	    continue;
	 auto const tagset = tagsetiter->second;
	 if (tagset.empty() == false && std::find(tagset.begin(), tagset.end(), m.second) == tagset.end())
	 {
	    failedMatch = true;
	    break;
	 }
      }
      if (failedMatch)
	 continue;
      possMirrors.push_back(mirror);
   }
   for (auto &&mirror : possMirrors)
      mirror.seed = genrng();
   std::sort(possMirrors.begin(), possMirrors.end(), [](MirrorInfo const &a, MirrorInfo const &b) {
      if (a.priority != b.priority)
	 return a.priority < b.priority;
      return a.seed < b.seed;
   });
   std::string const path = Itm->Uri.substr(info.baseuri.length());
   std::string altMirrors;
   std::unordered_map<std::string, std::string> fields;
   fields.emplace("URI", Queue->Uri);
   for (auto curMirror = possMirrors.cbegin(); curMirror != possMirrors.cend(); ++curMirror)
   {
      std::string mirror = curMirror->uri;
      if (APT::String::Endswith(mirror, "/") == false)
	 mirror.append("/");
      mirror.append(path);
      if (curMirror == possMirrors.cbegin())
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
					MirrorListInfo const &info, FetchItem *const Itm,
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
bool MirrorMethod::MirrorListFileReceived(MirrorListInfo &info, FetchItem *const Itm) /*{{{*/
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
      auto const accessColon = info.baseuri.find(':');
      auto access = info.baseuri.substr(0, accessColon);
      std::string prefixAccess;
      if (APT::String::Startswith(access, "mirror") == false)
      {
	 auto const plus = info.baseuri.find('+');
	 prefixAccess = info.baseuri.substr(0, plus);
	 access.erase(0, plus + 1);
      }
      std::vector<std::string> limitAccess;
      // If the mirror file comes from an online source, allow only other online
      // sources, not e.g. file:///. If the mirrorlist comes from there we can assume
      // the admin knows what (s)he is doing through and not limit the options.
      if (std::any_of(std::begin(disallowLocal), std::end(disallowLocal),
		      [&access](char const *const a) { return APT::String::Endswith(access, std::string("+") + a); }) ||
	  access == "mirror")
      {
	 std::copy(std::begin(disallowLocal), std::end(disallowLocal), std::back_inserter(limitAccess));
      }
      std::string line;
      while (mirrorlist.ReadLine(line))
      {
	 if (line.empty() || line[0] == '#')
	    continue;
	 auto const access = line.substr(0, line.find(':'));
	 if (limitAccess.empty() == false && std::find(limitAccess.begin(), limitAccess.end(), access) == limitAccess.end())
	    continue;
	 auto const tab = line.find('\t');
	 if (tab == std::string::npos)
	 {
	    if (prefixAccess.empty())
	       info.list.emplace_back(std::move(line));
	    else
	       info.list.emplace_back(prefixAccess + '+' + line);
	 }
	 else
	 {
	    auto uri = line.substr(0, tab);
	    if (prefixAccess.empty() == false)
	       uri = prefixAccess + '+' + uri;
	    auto tagline = line.substr(tab + 1);
	    std::replace_if(tagline.begin(), tagline.end(), isspace_ascii, ' ');
	    auto tags = VectorizeString(tagline, ' ');
	    tags.erase(std::remove_if(tags.begin(), tags.end(), [](std::string const &a) { return a.empty(); }), tags.end());
	    info.list.emplace_back(std::move(uri), std::move(tags));
	 }
      }
      mirrorlist.Close();

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
	    RedirectItem(info, Queue, msgCache[Queue->Uri]);
	 });
	 msgCache.clear();
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
	 {
	    // started as tor+mirror+http we want to get the file via tor+http
	    auto const access = uristr.substr(0, colon);
	    if (APT::String::Startswith(access, "mirror") == false)
	    {
	       uristr.erase(plus, strlen("mirror") + 1);
	       return uristr;
	    }
	    else
	       return uristr.substr(plus + 1);
	 }
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
      return MirrorListFileReceived(mirrorinfo->second, Itm);

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
      msgCache[Itm->Uri] = Message;
      MirrorListInfo info;
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
      msgCache[Itm->Uri] = Message;
      return true;
   case FAILED:
      Fail("Downloading mirror file failed", false);
      return true;
   case AVAILABLE:
      RedirectItem(state->second, Itm, Message);
      return true;
   }
   return false;
}
									/*}}}*/

int main(int, const char *argv[])
{
   return MirrorMethod(flNotDir(argv[0])).Run();
}
