// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: extracttar.cc,v 1.8.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Extract a Tar - Tar Extractor

   Some performance measurements showed that zlib performed quite poorly
   in comparision to a forked gzip process. This tar extractor makes use
   of the fact that dup'd file descriptors have the same seek pointer
   and that gzip will not read past the end of a compressed stream, 
   even if there is more data. We use the dup property to track extraction
   progress and the gzip feature to just feed gzip a fd in the middle
   of an AR file. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/extracttar.h>

#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <system.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
#include <apti18n.h>
									/*}}}*/

using namespace std;
    
// The on disk header for a tar file.
struct ExtractTar::TarHeader
{
   char Name[100];
   char Mode[8];
   char UserID[8];
   char GroupID[8];
   char Size[12];
   char MTime[12];
   char Checksum[8];
   char LinkFlag;
   char LinkName[100];
   char MagicNumber[8];
   char UserName[32];
   char GroupName[32];
   char Major[8];
   char Minor[8];      
};
   
// ExtractTar::ExtractTar - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
ExtractTar::ExtractTar(FileFd &Fd,unsigned long Max,string DecompressionProgram) : File(Fd), 
                         MaxInSize(Max), DecompressProg(DecompressionProgram)

{
   GZPid = -1;
   InFd = -1;
   Eof = false;
}
									/*}}}*/
// ExtractTar::ExtractTar - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
ExtractTar::~ExtractTar()
{
   // Error close
   Done(true);
}
									/*}}}*/
// ExtractTar::Done - Reap the gzip sub process				/*{{{*/
// ---------------------------------------------------------------------
/* If the force flag is given then error messages are suppressed - this
   means we hit the end of the tar file but there was still gzip data. */
bool ExtractTar::Done(bool Force)
{
   InFd.Close();
   if (GZPid <= 0)
      return true;

   /* If there is a pending error then we are cleaning up gzip and are
      not interested in it's failures */
   if (_error->PendingError() == true)
      Force = true;
   
   // Make sure we clean it up!
   kill(GZPid,SIGINT);
   string confvar = string("dir::bin::") + DecompressProg;
   if (ExecWait(GZPid,_config->Find(confvar.c_str(),DecompressProg.c_str()).c_str(),
		Force) == false)
   {
      GZPid = -1;
      return Force;
   }
   
   GZPid = -1;
   return true;
}
									/*}}}*/
// ExtractTar::StartGzip - Startup gzip					/*{{{*/
// ---------------------------------------------------------------------
/* This creates a gzip sub process that has its input as the file itself.
   If this tar file is embedded into something like an ar file then 
   gzip will efficiently ignore the extra bits. */
bool ExtractTar::StartGzip()
{
   int Pipes[2];
   if (pipe(Pipes) != 0)
      return _error->Errno("pipe",_("Failed to create pipes"));
   
   // Fork off the process
   GZPid = ExecFork();

   // Spawn the subprocess
   if (GZPid == 0)
   {
      // Setup the FDs
      dup2(Pipes[1],STDOUT_FILENO);
      dup2(File.Fd(),STDIN_FILENO);
      int Fd = open("/dev/null",O_RDWR);
      if (Fd == -1)
	 _exit(101);
      dup2(Fd,STDERR_FILENO);
      close(Fd);
      SetCloseExec(STDOUT_FILENO,false);
      SetCloseExec(STDIN_FILENO,false);      
      SetCloseExec(STDERR_FILENO,false);
      
      const char *Args[3];
      string confvar = string("dir::bin::") + DecompressProg;
      string argv0 = _config->Find(confvar.c_str(),DecompressProg.c_str());
      Args[0] = argv0.c_str();
      Args[1] = "-d";
      Args[2] = 0;
      execvp(Args[0],(char **)Args);
      cerr << _("Failed to exec gzip ") << Args[0] << endl;
      _exit(100);
   }

   // Fix up our FDs
   InFd.Fd(Pipes[0]);
   close(Pipes[1]);
   return true;
}
									/*}}}*/
// ExtractTar::Go - Perform extraction					/*{{{*/
// ---------------------------------------------------------------------
/* This reads each 512 byte block from the archive and extracts the header
   information into the Item structure. Then it resolves the UID/GID and
   invokes the correct processing function. */
bool ExtractTar::Go(pkgDirStream &Stream)
{   
   if (StartGzip() == false)
      return false;
   
   // Loop over all blocks
   string LastLongLink;
   string LastLongName;
   while (1)
   {
      bool BadRecord = false;      
      unsigned char Block[512];      
      if (InFd.Read(Block,sizeof(Block),true) == false)
	 return false;
      
      if (InFd.Eof() == true)
	 break;

      // Get the checksum
      TarHeader *Tar = (TarHeader *)Block;
      unsigned long CheckSum;
      if (StrToNum(Tar->Checksum,CheckSum,sizeof(Tar->Checksum),8) == false)
	 return _error->Error(_("Corrupted archive"));
      
      /* Compute the checksum field. The actual checksum is blanked out
         with spaces so it is not included in the computation */
      unsigned long NewSum = 0;
      memset(Tar->Checksum,' ',sizeof(Tar->Checksum));
      for (int I = 0; I != sizeof(Block); I++)
	 NewSum += Block[I];
      
      /* Check for a block of nulls - in this case we kill gzip, GNU tar
       	 does this.. */
      if (NewSum == ' '*sizeof(Tar->Checksum))
	 return Done(true);
      
      if (NewSum != CheckSum)
	 return _error->Error(_("Tar checksum failed, archive corrupted"));
   
      // Decode all of the fields
      pkgDirStream::Item Itm;
      if (StrToNum(Tar->Mode,Itm.Mode,sizeof(Tar->Mode),8) == false ||
	  StrToNum(Tar->UserID,Itm.UID,sizeof(Tar->UserID),8) == false ||
	  StrToNum(Tar->GroupID,Itm.GID,sizeof(Tar->GroupID),8) == false ||
	  StrToNum(Tar->Size,Itm.Size,sizeof(Tar->Size),8) == false ||
	  StrToNum(Tar->MTime,Itm.MTime,sizeof(Tar->MTime),8) == false ||
	  StrToNum(Tar->Major,Itm.Major,sizeof(Tar->Major),8) == false ||
	  StrToNum(Tar->Minor,Itm.Minor,sizeof(Tar->Minor),8) == false)
	 return _error->Error(_("Corrupted archive"));
      
      // Grab the filename
      if (LastLongName.empty() == false)
	 Itm.Name = (char *)LastLongName.c_str();
      else
      {
	 Tar->Name[sizeof(Tar->Name)-1] = 0;
	 Itm.Name = Tar->Name;
      }      
      if (Itm.Name[0] == '.' && Itm.Name[1] == '/' && Itm.Name[2] != 0)
	 Itm.Name += 2;
      
      // Grab the link target
      Tar->Name[sizeof(Tar->LinkName)-1] = 0;
      Itm.LinkTarget = Tar->LinkName;

      if (LastLongLink.empty() == false)
	 Itm.LinkTarget = (char *)LastLongLink.c_str();
      
      // Convert the type over
      switch (Tar->LinkFlag)
      {
	 case NormalFile0:
	 case NormalFile:
	 Itm.Type = pkgDirStream::Item::File;
	 break;
	 
	 case HardLink:
	 Itm.Type = pkgDirStream::Item::HardLink;
	 break;
	 
	 case SymbolicLink:
	 Itm.Type = pkgDirStream::Item::SymbolicLink;
	 break;
	 
	 case CharacterDevice:
	 Itm.Type = pkgDirStream::Item::CharDevice;
	 break;
	    
	 case BlockDevice:
	 Itm.Type = pkgDirStream::Item::BlockDevice;
	 break;
	 
	 case Directory:
	 Itm.Type = pkgDirStream::Item::Directory;
	 break;
	 
	 case FIFO:
	 Itm.Type = pkgDirStream::Item::FIFO;
	 break;

	 case GNU_LongLink:
	 {
	    unsigned long Length = Itm.Size;
	    unsigned char Block[512];
	    while (Length > 0)
	    {
	       if (InFd.Read(Block,sizeof(Block),true) == false)
		  return false;
	       if (Length <= sizeof(Block))
	       {
		  LastLongLink.append(Block,Block+sizeof(Block));
		  break;
	       }	       
	       LastLongLink.append(Block,Block+sizeof(Block));
	       Length -= sizeof(Block);
	    }
	    continue;
	 }
	 
	 case GNU_LongName:
	 {
	    unsigned long Length = Itm.Size;
	    unsigned char Block[512];
	    while (Length > 0)
	    {
	       if (InFd.Read(Block,sizeof(Block),true) == false)
		  return false;
	       if (Length < sizeof(Block))
	       {
		  LastLongName.append(Block,Block+sizeof(Block));
		  break;
	       }	       
	       LastLongName.append(Block,Block+sizeof(Block));
	       Length -= sizeof(Block);
	    }
	    continue;
	 }
	 
	 default:
	 BadRecord = true;
	 _error->Warning(_("Unknown TAR header type %u, member %s"),(unsigned)Tar->LinkFlag,Tar->Name);
	 break;
      }
      
      int Fd = -1;
      if (BadRecord == false)
	 if (Stream.DoItem(Itm,Fd) == false)
	    return false;
      
      // Copy the file over the FD
      unsigned long Size = Itm.Size;
      while (Size != 0)
      {
	 unsigned char Junk[32*1024];
	 unsigned long Read = min(Size,(unsigned long)sizeof(Junk));
	 if (InFd.Read(Junk,((Read+511)/512)*512) == false)
	    return false;
	 
	 if (BadRecord == false)
	 {
	    if (Fd > 0)
	    {
	       if (write(Fd,Junk,Read) != (signed)Read)
		  return Stream.Fail(Itm,Fd);
	    }
	    else
	    {
	       /* An Fd of -2 means to send to a special processing
		  function */
	       if (Fd == -2)
		  if (Stream.Process(Itm,Junk,Read,Itm.Size - Size) == false)
		     return Stream.Fail(Itm,Fd);
	    }
	 }
	 
	 Size -= Read;
      }
      
      // And finish up
      if (Itm.Size >= 0 && BadRecord == false)
	 if (Stream.FinishedFile(Itm,Fd) == false)
	    return false;
      
      LastLongName.erase();
      LastLongLink.erase();
   }
   
   return Done(false);
}
									/*}}}*/
