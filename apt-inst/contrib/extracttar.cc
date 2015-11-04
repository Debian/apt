// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: extracttar.cc,v 1.8.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Extract a Tar - Tar Extractor

   Some performance measurements showed that zlib performed quite poorly
   in comparison to a forked gzip process. This tar extractor makes use
   of the fact that dup'd file descriptors have the same seek pointer
   and that gzip will not read past the end of a compressed stream, 
   even if there is more data. We use the dup property to track extraction
   progress and the gzip feature to just feed gzip a fd in the middle
   of an AR file. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/dirstream.h>
#include <apt-pkg/extracttar.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <string.h>
#include <algorithm>
#include <string>
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
ExtractTar::ExtractTar(FileFd &Fd,unsigned long long Max,string DecompressionProgram)
	: File(Fd), MaxInSize(Max), DecompressProg(DecompressionProgram)
{
   GZPid = -1;
   Eof = false;
}
									/*}}}*/
// ExtractTar::ExtractTar - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
ExtractTar::~ExtractTar()
{
   // Error close
   Done();
}
									/*}}}*/
// ExtractTar::Done - Reap the gzip sub process				/*{{{*/
bool ExtractTar::Done(bool)
{
   return Done();
}
bool ExtractTar::Done()
{
   return InFd.Close();
}
									/*}}}*/
// ExtractTar::StartGzip - Startup gzip					/*{{{*/
// ---------------------------------------------------------------------
/* This creates a gzip sub process that has its input as the file itself.
   If this tar file is embedded into something like an ar file then 
   gzip will efficiently ignore the extra bits. */
bool ExtractTar::StartGzip()
{
   if (DecompressProg.empty())
   {
      InFd.OpenDescriptor(File.Fd(), FileFd::ReadOnly, FileFd::None, false);
      return true;
   }

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   for (; compressor != compressors.end(); ++compressor) {
      if (compressor->Name == DecompressProg) {
	 return InFd.OpenDescriptor(File.Fd(), FileFd::ReadOnly, *compressor, false);
      }
   }

   return _error->Error(_("Cannot find a configured compressor for '%s'"),
			DecompressProg.c_str());

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
   string LastLongLink, ItemLink;
   string LastLongName, ItemName;
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
	 return Done();
      
      if (NewSum != CheckSum)
	 return _error->Error(_("Tar checksum failed, archive corrupted"));
   
      // Decode all of the fields
      pkgDirStream::Item Itm;
      if (StrToNum(Tar->Mode,Itm.Mode,sizeof(Tar->Mode),8) == false ||
          (Base256ToNum(Tar->UserID,Itm.UID,8) == false &&
	     StrToNum(Tar->UserID,Itm.UID,sizeof(Tar->UserID),8) == false) ||
          (Base256ToNum(Tar->GroupID,Itm.GID,8) == false &&
	     StrToNum(Tar->GroupID,Itm.GID,sizeof(Tar->GroupID),8) == false) ||
          (Base256ToNum(Tar->Size,Itm.Size,12) == false &&
	     StrToNum(Tar->Size,Itm.Size,sizeof(Tar->Size),8) == false) ||
          (Base256ToNum(Tar->MTime,Itm.MTime,12) == false &&
	     StrToNum(Tar->MTime,Itm.MTime,sizeof(Tar->MTime),8) == false) ||
	  StrToNum(Tar->Major,Itm.Major,sizeof(Tar->Major),8) == false ||
	  StrToNum(Tar->Minor,Itm.Minor,sizeof(Tar->Minor),8) == false)
	 return _error->Error(_("Corrupted archive"));

      // Grab the filename and link target: use last long name if one was
      // set, otherwise use the header value as-is, but remember that it may
      // fill the entire 100-byte block and needs to be zero-terminated.
      // See Debian Bug #689582.
      if (LastLongName.empty() == false)
	 Itm.Name = (char *)LastLongName.c_str();
      else
	 Itm.Name = (char *)ItemName.assign(Tar->Name, sizeof(Tar->Name)).c_str();
      if (Itm.Name[0] == '.' && Itm.Name[1] == '/' && Itm.Name[2] != 0)
	 Itm.Name += 2;

      if (LastLongLink.empty() == false)
	 Itm.LinkTarget = (char *)LastLongLink.c_str();
      else
	 Itm.LinkTarget = (char *)ItemLink.assign(Tar->LinkName, sizeof(Tar->LinkName)).c_str();

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
	    unsigned long long Length = Itm.Size;
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
	    unsigned long long Length = Itm.Size;
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
      unsigned long long Size = Itm.Size;
      while (Size != 0)
      {
	 unsigned char Junk[32*1024];
	 unsigned long Read = min(Size, (unsigned long long)sizeof(Junk));
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
      if (BadRecord == false)
	 if (Stream.FinishedFile(Itm,Fd) == false)
	    return false;
      
      LastLongName.erase();
      LastLongLink.erase();
   }
   
   return Done();
}
									/*}}}*/
