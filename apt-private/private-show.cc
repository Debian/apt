// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-output.h>
#include <apt-private/private-show.h>

#include <stdio.h>
#include <ostream>
#include <string>

#include <apti18n.h>
									/*}}}*/

namespace APT {
   namespace Cmd {

// DisplayRecord - Displays the complete record for the package		/*{{{*/
// ---------------------------------------------------------------------
static bool DisplayRecord(pkgCacheFile &CacheFile, pkgCache::VerIterator V,
                   std::ostream &out)
{
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;
   pkgDepCache *depCache = CacheFile.GetDepCache();
   if (unlikely(depCache == NULL))
      return false;

   // Find an appropriate file
   pkgCache::VerFileIterator Vf = V.FileList();
   for (; Vf.end() == false; ++Vf)
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) == 0)
	 break;
   if (Vf.end() == true)
      Vf = V.FileList();
      
   // Check and load the package list file
   pkgCache::PkgFileIterator I = Vf.File();
   if (I.IsOk() == false)
      return _error->Error(_("Package file %s is out of sync."),I.FileName());

   // find matching sources.list metaindex
   pkgSourceList *SrcList = CacheFile.GetSourceList();
   pkgIndexFile *Index;
   if (SrcList->FindIndex(I, Index) == false &&
       _system->FindIndex(I, Index) == false)
      return _error->Error("Can not find indexfile for Package %s (%s)", 
                           V.ParentPkg().Name(), V.VerStr());
   std::string source_index_file = Index->Describe(true);

   // Read the record
   FileFd PkgF;
   if (PkgF.Open(I.FileName(), FileFd::ReadOnly, FileFd::Extension) == false)
      return false;
   pkgTagSection Tags;
   pkgTagFile TagF(&PkgF);

   if (TagF.Jump(Tags, V.FileList()->Offset) == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // make size nice
   std::string installed_size;
   if (Tags.FindI("Installed-Size") > 0)
      strprintf(installed_size, "%sB", SizeToStr(Tags.FindI("Installed-Size")*1024).c_str());
   else
      installed_size = _("unknown");
   std::string package_size;
   if (Tags.FindI("Size") > 0)
      strprintf(package_size, "%sB", SizeToStr(Tags.FindI("Size")).c_str());
   else
      package_size = _("unknown");

   pkgDepCache::StateCache &state = (*depCache)[V.ParentPkg()];
   bool is_installed = V.ParentPkg().CurrentVer() == V;
   const char *manual_installed;
   if (is_installed)
      manual_installed = !(state.Flags & pkgCache::Flag::Auto) ? "yes" : "no";
   else
      manual_installed = 0;

   // FIXME: add verbose that does not do the removal of the tags?
   TFRewriteData RW[] = {
      // delete, apt-cache show has this info and most users do not care
      {"MD5sum", NULL, NULL},
      {"SHA1", NULL, NULL},
      {"SHA256", NULL, NULL},
      {"Filename", NULL, NULL},
      {"Multi-Arch", NULL, NULL},
      {"Architecture", NULL, NULL},
      {"Conffiles", NULL, NULL},
      // we use the translated description
      {"Description", NULL, NULL},
      {"Description-md5", NULL, NULL},
      // improve
      {"Installed-Size", installed_size.c_str(), NULL},
      {"Size", package_size.c_str(), "Download-Size"},
      // add
      {"APT-Manual-Installed", manual_installed, NULL},
      {"APT-Sources", source_index_file.c_str(), NULL},
      {NULL, NULL, NULL}
   };

   if(TFRewrite(stdout, Tags, NULL, RW) == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // write the description
   pkgRecords Recs(*Cache);
   // FIXME: show (optionally) all available translations(?)
   pkgCache::DescIterator Desc = V.TranslatedDescription();
   if (Desc.end() == false)
   {
      pkgRecords::Parser &P = Recs.Lookup(Desc.FileList());
      out << "Description: " << P.LongDesc();
   }
   
   // write a final newline (after the description)
   out << std::endl << std::endl;

   return true;
}
									/*}}}*/
bool ShowPackage(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   CacheSetHelperVirtuals helper(true, GlobalError::NOTICE);
   APT::VersionList::Version const select = _config->FindB("APT::Cache::AllVersions", false) ?
			APT::VersionList::ALL : APT::VersionList::CANDIDATE;
   APT::VersionList const verset = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1, select, helper);
   for (APT::VersionList::const_iterator Ver = verset.begin(); Ver != verset.end(); ++Ver)
      if (DisplayRecord(CacheFile, Ver, c1out) == false)
	 return false;

   if (select == APT::VersionList::CANDIDATE)
   {
      APT::VersionList const verset_all = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1, APT::VersionList::ALL, helper);
      int const records = verset_all.size() - verset.size();
      if (records > 0)
         _error->Notice(P_("There is %i additional record. Please use the '-a' switch to see it", "There are %i additional records. Please use the '-a' switch to see them.", records), records);
   }

   for (APT::PackageSet::const_iterator Pkg = helper.virtualPkgs.begin();
	Pkg != helper.virtualPkgs.end(); ++Pkg)
   {
       c1out << "Package: " << Pkg.FullName(true) << std::endl;
       c1out << "State: " << _("not a real package (virtual)") << std::endl;
       // FIXME: show providers, see private-cacheset.h
       //        CacheSetHelperAPTGet::showVirtualPackageErrors()
   }

   if (verset.empty() == true)
   {
      if (helper.virtualPkgs.empty() == true)
        return _error->Error(_("No packages found"));
      else
        _error->Notice(_("No packages found"));
   }

   return true;
}
									/*}}}*/
} // namespace Cmd
} // namespace APT
