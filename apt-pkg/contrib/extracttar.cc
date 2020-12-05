// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/dirstream.h>
#include <apt-pkg/error.h>
#include <apt-pkg/extracttar.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

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

// We need to read long names (names and link targets) into memory, so let's
// have a limit (shamelessly stolen from libarchive) to avoid people OOMing
// us with large streams.
static const unsigned long long APT_LONGNAME_LIMIT = 1048576llu;

// A file size limit that we allow extracting. Currently, that's 128 GB.
// We also should leave some wiggle room for code adding files to it, and
// possibly conversion for signed, so this should not be larger than like 2**62.
static const unsigned long long APT_FILESIZE_LIMIT = 1llu << 37;

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

      // Security check. Prevents overflows below the code when rounding up in skip/copy code,
      // and provides modest protection against decompression bombs.
      if (Itm.Size > APT_FILESIZE_LIMIT)
	 return _error->Error("Tar member too large: %llu > %llu bytes", Itm.Size, APT_FILESIZE_LIMIT);

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
	    if (Length > APT_LONGNAME_LIMIT)
	       return _error->Error("Long name to large: %llu bytes > %llu bytes", Length, APT_LONGNAME_LIMIT);
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
	    if (Length > APT_LONGNAME_LIMIT)
	       return _error->Error("Long name to large: %llu bytes > %llu bytes", Length, APT_LONGNAME_LIMIT);
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
	 _error->Warning(_("Unknown TAR header type %u"), (unsigned)Tar->LinkFlag);
	 break;
      }

      int Fd = -1;
      if (not BadRecord && not Stream.DoItem(Itm, Fd))
	 return false;

      if (Fd == -1 || Fd < -2 || BadRecord)
      {
	 if (Itm.Size > 0 && not InFd.Skip(((Itm.Size + (sizeof(Block) - 1)) / sizeof(Block)) * sizeof(Block)))
	    return false;
      }
      else if (Itm.Size != 0)
      {
	 // Copy the file over the FD
	 auto Size = Itm.Size;
	 unsigned char Junk[32*1024];
	 do
	 {
	    auto const Read = std::min<unsigned long long>(Size, sizeof(Junk));
	    if (not InFd.Read(Junk, ((Read + (sizeof(Block) - 1)) / sizeof(Block)) * sizeof(Block)))
	       return false;

	    if (Fd > 0)
	    {
	       if (not FileFd::Write(Fd, Junk, Read))
		  return Stream.Fail(Itm, Fd);
	    }
	    // An Fd of -2 means to send to a special processing function
	    else if (Fd == -2)
	    {
	       if (not Stream.Process(Itm, Junk, Read, Itm.Size - Size))
		  return Stream.Fail(Itm, Fd);
	    }

	    Size -= Read;
	 } while (Size != 0);
      }

      // And finish up
      if (not BadRecord && not Stream.FinishedFile(Itm, Fd))
	 return false;
      LastLongName.erase();
      LastLongLink.erase();
   }

   return Done();
}
									/*}}}*/
