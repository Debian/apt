// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   These are here for the cdrom method and apt-cdrom.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cdromutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <iostream>
#include <string>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

// IsMounted - Returns true if the mount point is mounted		/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple algorithm that should always work, we stat the mount point
   and the '..' file in the mount point and see if they are on the same device.
   By definition if they are the same then it is not mounted. This should 
   account for symlinked mount points as well. */
bool IsMounted(string &Path)
{
   if (Path.empty() == true)
      return false;

   // Need that trailing slash for directories
   if (Path[Path.length() - 1] != '/')
      Path += '/';

   // if the path has a ".disk" directory we treat it as mounted
   // this way even extracted copies of disks are recognized
   if (DirectoryExists(Path + ".disk/") == true)
      return true;

   /* First we check if the path is actually mounted, we do this by
      stating the path and the previous directory (careful of links!)
      and comparing their device fields. */
   struct stat Buf,Buf2;
   if (stat(Path.c_str(),&Buf) != 0 || 
       stat((Path + "../").c_str(),&Buf2) != 0)
      return _error->Errno("stat",_("Unable to stat the mount point %s"),Path.c_str());

   if (Buf.st_dev == Buf2.st_dev)
      return false;
   return true;
}
									/*}}}*/
// UnmountCdrom - Unmount a cdrom					/*{{{*/
// ---------------------------------------------------------------------
/* Forking umount works much better than the umount syscall which can 
   leave /etc/mtab inconsistent. We drop all messages this produces. */
bool UnmountCdrom(string Path)
{
   // do not generate errors, even if the mountpoint does not exist
   // the mountpoint might be auto-created by the mount command
   // and a non-existing mountpoint is surely not mounted
   _error->PushToStack();
   bool const mounted = IsMounted(Path);
   _error->RevertToStack();
   if (mounted == false)
      return true;

   for (int i=0;i<3;i++)
   {
   
      int Child = ExecFork();

      // The child
      if (Child == 0)
      {
	 // Make all the fds /dev/null
	 int const null_fd = open("/dev/null",O_RDWR);
	 for (int I = 0; I != 3; ++I)
	    dup2(null_fd, I);

	 if (_config->Exists("Acquire::cdrom::"+Path+"::UMount") == true)
	 {
	    if (system(_config->Find("Acquire::cdrom::"+Path+"::UMount").c_str()) != 0)
	       _exit(100);
	    _exit(0);	 	 
	 }
	 else
	 {
	    const char * const Args[] = {
	       "umount",
	       Path.c_str(),
	       nullptr
	    };
	    execvp(Args[0], const_cast<char **>(Args));
	    _exit(100);
	 }
      }

      // if it can not be umounted, give it a bit more time
      // this can happen when auto-mount magic or fs/cdrom prober attack
      if (ExecWait(Child,"umount",true) == true)
	 return true;
      sleep(1);
   }

   return false;
}
									/*}}}*/
// MountCdrom - Mount a cdrom						/*{{{*/
// ---------------------------------------------------------------------
/* We fork mount and drop all messages */
bool MountCdrom(string Path, string DeviceName)
{
   // do not generate errors, even if the mountpoint does not exist
   // the mountpoint might be auto-created by the mount command
   _error->PushToStack();
   bool const mounted = IsMounted(Path);
   _error->RevertToStack();
   if (mounted == true)
      return true;

   int Child = ExecFork();

   // The child
   if (Child == 0)
   {
      // Make all the fds /dev/null
      int const null_fd = open("/dev/null",O_RDWR);
      for (int I = 0; I != 3; ++I)
	 dup2(null_fd, I);

      if (_config->Exists("Acquire::cdrom::"+Path+"::Mount") == true)
      {
	 if (system(_config->Find("Acquire::cdrom::"+Path+"::Mount").c_str()) != 0)
	    _exit(100);
	 _exit(0);	 
      }
      else
      {
	 const char *Args[10];
	 Args[0] = "mount";
	 if (DeviceName == "") 
	 {
	    Args[1] = Path.c_str();
	    Args[2] = 0;
	 } else {
	    Args[1] = DeviceName.c_str();
	    Args[2] = Path.c_str();
	    Args[3] = 0;
	 }
	 execvp(Args[0],(char **)Args);      
	 _exit(100);
      }      
   }

   // Wait for mount
   return ExecWait(Child,"mount",true);
}
									/*}}}*/
// IdentCdrom - Generate a unique string for this CD			/*{{{*/
// ---------------------------------------------------------------------
/* We convert everything we hash into a string, this prevents byte size/order
   from effecting the outcome. */
bool IdentCdrom(string CD,string &Res,unsigned int Version)
{
   Hashes Hash(Hashes::MD5SUM);
   bool writable_media = false;

   int dirfd = open(CD.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
   if (dirfd == -1)
      return _error->Errno("open",_("Unable to read %s"),CD.c_str());

   // if we are on a writable medium (like a usb-stick) that is just
   // used like a cdrom don't use "." as it will constantly change,
   // use .disk instead
   if (faccessat(dirfd, ".", W_OK, 0) == 0)
   {
      int diskfd = openat(dirfd, "./.disk", O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
      if (diskfd != -1)
      {
	 close(dirfd);
	 dirfd = diskfd;
	 writable_media = true;
	 CD = CD.append("/.disk");
	 if (_config->FindB("Debug::aptcdrom",false) == true)
	    std::clog << "Found writable cdrom, using alternative path: " << CD
	       << std::endl;
      }
   }

   DIR * const D = fdopendir(dirfd);
   if (D == nullptr)
      return _error->Errno("opendir",_("Unable to read %s"),CD.c_str());

   /* Run over the directory, we assume that the reader order will never
      change as the media is read-only. In theory if the kernel did
      some sort of wacked caching this might not be true.. */
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;

      std::string S;
      if (Version <= 1)
	 S = std::to_string(Dir->d_ino);
      else
      {
	 struct stat Buf;
	 if (fstatat(dirfd, Dir->d_name, &Buf, 0) != 0)
	    continue;
	 S = std::to_string(Buf.st_mtime);
      }

      Hash.Add(S.c_str());
      Hash.Add(Dir->d_name);
   }

   // Some stats from the fsys
   std::string S;
   if (_config->FindB("Debug::identcdrom",false) == false)
   {
      struct statvfs Buf;
      if (fstatvfs(dirfd, &Buf) != 0)
	 return _error->Errno("statfs",_("Failed to stat the cdrom"));

      // We use a kilobyte block size to avoid overflow
      S = std::to_string(Buf.f_blocks * (Buf.f_bsize / 1024));
      if (writable_media == false)
	 S.append(" ").append(std::to_string(Buf.f_bfree * (Buf.f_bsize / 1024)));
      Hash.Add(S.c_str(), S.length());
      strprintf(S, "-%u", Version);
   }
   else
      strprintf(S, "-%u.debug", Version);

   closedir(D);
   Res = Hash.GetHashString(Hashes::MD5SUM).HashValue().append(std::move(S));
   return true;
}
									/*}}}*/
// FindMountPointForDevice - Find mountpoint for the given device	/*{{{*/
string FindMountPointForDevice(const char *devnode)
{
   // this is the order that mount uses as well
   std::vector<std::string> const mounts = _config->FindVector("Dir::state::MountPoints", "/etc/mtab,/proc/mount");

   for (std::vector<std::string>::const_iterator m = mounts.begin(); m != mounts.end(); ++m)
      if (FileExists(*m) == true)
      {
	 char * line = NULL;
	 size_t line_len = 0;
	 FILE * f = fopen(m->c_str(), "r");
	 while(getline(&line, &line_len, f) != -1)
	 {
	    char * out[] = { NULL, NULL, NULL };
	    TokSplitString(' ', line, out, 3);
	    if (out[2] != NULL || out[1] == NULL || out[0] == NULL)
	       continue;
	    if (strcmp(out[0], devnode) != 0)
	       continue;
	    fclose(f);
	    // unescape the \0XXX chars in the path
	    string mount_point = out[1];
	    free(line);
	    return DeEscapeString(mount_point);
	 }
	 fclose(f);
	 free(line);
      }

   return string();
}
									/*}}}*/
