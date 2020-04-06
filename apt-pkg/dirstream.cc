// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Directory Stream 
   
   This class provides a simple basic extractor that can be used for
   a number of purposes.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/dirstream.h>
#include <apt-pkg/error.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <apti18n.h>
									/*}}}*/

// DirStream::DoItem - Process an item					/*{{{*/
// ---------------------------------------------------------------------
/* This is a very simple extractor, it does not deal with things like
   overwriting directories with files and so on. */
bool pkgDirStream::DoItem(Item &Itm,int &Fd)
{
   switch (Itm.Type)
   {
      case Item::File:
      {
	 /* Open the output file, NDELAY is used to prevent this from 
	    blowing up on device special files.. */
	 int iFd = open(Itm.Name,O_NDELAY|O_WRONLY|O_CREAT|O_TRUNC|O_APPEND,
		       Itm.Mode);
	 if (iFd < 0)
	    return _error->Errno("open",_("Failed to write file %s"),
				 Itm.Name);
	 
	 // fchmod deals with umask and fchown sets the ownership
	 if (fchmod(iFd,Itm.Mode) != 0)
	 {
	    close(iFd);
	    return _error->Errno("fchmod",_("Failed to write file %s"), Itm.Name);
	 }
	 if (fchown(iFd,Itm.UID,Itm.GID) != 0 && errno != EPERM)
	 {
	    close(iFd);
	    return _error->Errno("fchown",_("Failed to write file %s"), Itm.Name);
	 }
	 Fd = iFd;
	 return true;
      }
      
      case Item::HardLink:
      case Item::SymbolicLink:
      case Item::CharDevice:
      case Item::BlockDevice:
      case Item::Directory:
      {
	 struct stat Buf;
	 // check if the dir is already there, if so return true
	 if (stat(Itm.Name,&Buf) == 0)
	 {
	    if(S_ISDIR(Buf.st_mode))
	       return true;
	    // something else is there already, return false
	    return false;
	 }
	 // nothing here, create the dir
	 if(mkdir(Itm.Name,Itm.Mode) < 0)
	    return false;
	 return true;
      }
      case Item::FIFO:
      break;
   }
   
   return true;
}
									/*}}}*/
// DirStream::FinishedFile - Finished processing a file			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDirStream::FinishedFile(Item &Itm,int Fd)
{
   if (Fd < 0)
      return true;

   /* Set the modification times. The only way it can fail is if someone
      has futzed with our file, which is intolerable :> */
   struct timeval times[2];
   times[0].tv_sec = times[1].tv_sec = Itm.MTime;
   times[0].tv_usec = times[1].tv_usec = 0;
   if (utimes(Itm.Name, times) != 0)
      _error->Errno("utimes", "Failed to set modification time for %s",Itm.Name);

   if (close(Fd) != 0)
      return _error->Errno("close",_("Failed to close file %s"),Itm.Name);
   return true;
}
									/*}}}*/
// DirStream::Fail - Failed processing a file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDirStream::Fail(Item &/*Itm*/, int Fd)
{
   if (Fd < 0)
      return true;
   
   close(Fd);
   return false;
}
									/*}}}*/
