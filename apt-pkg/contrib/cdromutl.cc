// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cdromutl.cc,v 1.12 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################
   
   CDROM Utilities - Some functions to manipulate CDROM mounts.
   
   These are here for the cdrom method and apt-cdrom.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/cdromutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <sys/statvfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

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
   leave /etc/mtab inconsitant. We drop all messages this produces. */
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
	    const char *Args[10];
	    Args[0] = "umount";
	    Args[1] = Path.c_str();
	    Args[2] = 0;
	    execvp(Args[0],(char **)Args);      
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
   MD5Summation Hash;
   bool writable_media = false;

   // if we are on a writable medium (like a usb-stick) that is just
   // used like a cdrom don't use "." as it will constantly change,
   // use .disk instead
   if (access(CD.c_str(), W_OK) == 0 && DirectoryExists(CD+string("/.disk"))) 
   {
      writable_media = true;
      CD = CD.append("/.disk");
      if (_config->FindB("Debug::aptcdrom",false) == true)
         std::clog << "Found writable cdrom, using alternative path: " << CD
                   << std::endl;
   }

   string StartDir = SafeGetCWD();
   if (chdir(CD.c_str()) != 0)
      return _error->Errno("chdir",_("Unable to change to %s"),CD.c_str());
   
   DIR *D = opendir(".");
   if (D == 0)
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
      {
	 strprintf(S, "%lu", (unsigned long)Dir->d_ino);
      }
      else
      {
	 struct stat Buf;
	 if (stat(Dir->d_name,&Buf) != 0)
	    continue;
	 strprintf(S, "%lu", (unsigned long)Buf.st_mtime);
      }

      Hash.Add(S.c_str());
      Hash.Add(Dir->d_name);
   };

   if (chdir(StartDir.c_str()) != 0) {
      _error->Errno("chdir",_("Unable to change to %s"),StartDir.c_str());
      closedir(D);
      return false;
   }
   closedir(D);

   // Some stats from the fsys
   std::string S;
   if (_config->FindB("Debug::identcdrom",false) == false)
   {
      struct statvfs Buf;
      if (statvfs(CD.c_str(),&Buf) != 0)
	 return _error->Errno("statfs",_("Failed to stat the cdrom"));

      // We use a kilobyte block size to advoid overflow
      if (writable_media)
      {
         strprintf(S, "%lu", (unsigned long)(Buf.f_blocks*(Buf.f_bsize/1024)));
      } else {
         strprintf(S, "%lu %lu", (unsigned long)(Buf.f_blocks*(Buf.f_bsize/1024)),
                 (unsigned long)(Buf.f_bfree*(Buf.f_bsize/1024)));
      }
      Hash.Add(S.c_str());
      strprintf(S, "-%u", Version);
   }
   else
      strprintf(S, "-%u.debug", Version);

   Res = Hash.Result().Value() + S;
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
