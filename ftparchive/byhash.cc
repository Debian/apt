// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   ByHash 
   
   ByHash helper functions
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include<algorithm>
#include<string>

#include <unistd.h>
#include <sys/stat.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include "byhash.h"

// Delete all files in a directory except the most recent N ones
void DeleteAllButMostRecent(std::string dir, int KeepFiles)
{
   struct Cmp {
      bool operator() (const std::string& lhs, const std::string& rhs) {
         struct stat buf_l, buf_r;
         stat(lhs.c_str(), &buf_l);
         stat(rhs.c_str(), &buf_r);
         if (buf_l.st_mtim.tv_sec == buf_r.st_mtim.tv_sec)
            return buf_l.st_mtim.tv_nsec < buf_r.st_mtim.tv_nsec;
         return buf_l.st_mtim.tv_sec < buf_r.st_mtim.tv_sec;
      }
   };

   if (!DirectoryExists(dir))
      return;

   auto files = GetListOfFilesInDir(dir, false);
   std::sort(files.begin(), files.end(), Cmp());

   for (auto I=files.begin(); I<files.end()-KeepFiles; ++I)
      RemoveFile("DeleteAllButMostRecent", *I);
}

// Takes a input filename (e.g. binary-i386/Packages) and a hashstring
// of the Input data and transforms it into a suitable by-hash filename
std::string GenByHashFilename(std::string ByHashOutputFile, HashString const &h)
{
   std::string const ByHash = "/by-hash/" + h.HashType() + "/" + h.HashValue();
   size_t trailing_slash = ByHashOutputFile.find_last_of("/");
   if (trailing_slash == std::string::npos)
      trailing_slash = 0;
   ByHashOutputFile = ByHashOutputFile.replace(
      trailing_slash,
      ByHashOutputFile.substr(trailing_slash+1).size()+1,
      ByHash);
   return ByHashOutputFile;
}
