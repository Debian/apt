// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debfile.cc,v 1.3.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   Debian Archive File (.deb)
   
   .DEB archives are AR files containing two tars and an empty marker
   member called 'debian-binary'. The two tars contain the meta data and
   the actual archive contents. Thus this class is a very simple wrapper
   around ar/tar to simply extract the right tar files.
   
   It also uses the deb package list parser to parse the control file 
   into the cache.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/debfile.h>
#include <apt-pkg/extracttar.h>
#include <apt-pkg/error.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/arfile.h>
#include <apt-pkg/dirstream.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/tagfile.h>

#include <string.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#include <apti18n.h>
									/*}}}*/

// DebFile::debDebFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Open the AR file and check for consistency */
debDebFile::debDebFile(FileFd &File) : File(File), AR(File)
{
   if (_error->PendingError() == true)
      return;

   if (!CheckMember("debian-binary")) {
      _error->Error(_("This is not a valid DEB archive, missing '%s' member"), "debian-binary");
      return;
   }

   if (!CheckMember("control.tar") &&
       !CheckMember("control.tar.gz") &&
       !CheckMember("control.tar.xz")) {
      _error->Error(_("This is not a valid DEB archive, missing '%s' member"), "control.tar");
      return;
   }

   if (!CheckMember("data.tar") &&
       !CheckMember("data.tar.gz") &&
       !CheckMember("data.tar.bz2") &&
       !CheckMember("data.tar.lzma") &&
       !CheckMember("data.tar.xz")) {
      _error->Error(_("This is not a valid DEB archive, missing '%s' member"), "data.tar");
      return;
   }
}
									/*}}}*/
// DebFile::CheckMember - Check if a named member is in the archive	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to check for a correct deb and to give nicer error messages
   for people playing around. */
bool debDebFile::CheckMember(const char *Name)
{
   if (AR.FindMember(Name) == 0)
      return false;
   return true;
}
									/*}}}*/
// DebFile::GotoMember - Jump to a Member				/*{{{*/
// ---------------------------------------------------------------------
/* Jump in the file to the start of a named member and return the information
   about that member. The caller can then read from the file up to the 
   returned size. Note, since this relies on the file position this is
   a destructive operation, it also changes the last returned Member
   structure - so don't nest them! */
const ARArchive::Member *debDebFile::GotoMember(const char *Name)
{
   // Get the archive member and positition the file
   const ARArchive::Member *Member = AR.FindMember(Name);
   if (Member == 0)
   {
      return 0;
   }
   if (File.Seek(Member->Start) == false)
      return 0;
      
   return Member;
}
									/*}}}*/
// DebFile::ExtractTarMember - Extract the contents of a tar member	/*{{{*/
// ---------------------------------------------------------------------
/* Simple wrapper around tar.. */
bool debDebFile::ExtractTarMember(pkgDirStream &Stream,const char *Name)
{
   // Get the archive member
   const ARArchive::Member *Member = NULL;
   std::string Compressor;

   std::vector<APT::Configuration::Compressor> compressor = APT::Configuration::getCompressors();
   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	c != compressor.end(); ++c)
   {
      Member = AR.FindMember(std::string(Name).append(c->Extension).c_str());
      if (Member == NULL)
	 continue;
      Compressor = c->Name;
      break;
   }

   if (Member == NULL)
      Member = AR.FindMember(std::string(Name).c_str());

   if (Member == NULL)
   {
      std::string ext = std::string(Name) + ".{";
      for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	   c != compressor.end(); ++c) {
	 if (!c->Extension.empty())
	    ext.append(c->Extension.substr(1));
      }
      ext.append("}");
      return _error->Error(_("Internal error, could not locate member %s"), ext.c_str());
   }

   if (File.Seek(Member->Start) == false)
      return false;

   // Prepare Tar
   ExtractTar Tar(File,Member->Size,Compressor);
   if (_error->PendingError() == true)
      return false;
   return Tar.Go(Stream);
}
									/*}}}*/
// DebFile::ExtractArchive - Extract the archive data itself		/*{{{*/
// ---------------------------------------------------------------------
/* Simple wrapper around DebFile::ExtractTarMember. */
bool debDebFile::ExtractArchive(pkgDirStream &Stream)
{
   return ExtractTarMember(Stream, "data.tar");
}
									/*}}}*/

// DebFile::ControlExtract::DoItem - Control Tar Extraction		/*{{{*/
// ---------------------------------------------------------------------
/* This directory stream handler for the control tar handles extracting
   it into the temporary meta directory. It only extracts files, it does
   not create directories, links or anything else. */
bool debDebFile::ControlExtract::DoItem(Item &Itm,int &Fd)
{
   if (Itm.Type != Item::File)
      return true;
   
   /* Cleanse the file name, prevent people from trying to unpack into
      absolute paths, .., etc */
   for (char *I = Itm.Name; *I != 0; I++)
      if (*I == '/')
	 *I = '_';

   /* Force the ownership to be root and ensure correct permissions, 
      go-w, the rest are left untouched */
   Itm.UID = 0;
   Itm.GID = 0;
   Itm.Mode &= ~(S_IWGRP | S_IWOTH);
   
   return pkgDirStream::DoItem(Itm,Fd);
}
									/*}}}*/

// MemControlExtract::DoItem - Check if it is the control file		/*{{{*/
// ---------------------------------------------------------------------
/* This sets up to extract the control block member file into a memory 
   block of just the right size. All other files go into the bit bucket. */
bool debDebFile::MemControlExtract::DoItem(Item &Itm,int &Fd)
{
   // At the control file, allocate buffer memory.
   if (Member == Itm.Name)
   {
      delete [] Control;
      Control = new char[Itm.Size+2];
      IsControl = true;
      Fd = -2; // Signal to pass to Process
      Length = Itm.Size;
   }   
   else
      IsControl = false;
   
   return true;
}
									/*}}}*/
// MemControlExtract::Process - Process extracting the control file	/*{{{*/
// ---------------------------------------------------------------------
/* Just memcopy the block from the tar extractor and put it in the right
   place in the pre-allocated memory block. */
bool debDebFile::MemControlExtract::Process(Item &/*Itm*/,const unsigned char *Data,
			     unsigned long long Size,unsigned long long Pos)
{
   memcpy(Control + Pos, Data,Size);
   return true;
}
									/*}}}*/
// MemControlExtract::Read - Read the control information from the deb	/*{{{*/
// ---------------------------------------------------------------------
/* This uses the internal tar extractor to fetch the control file, and then
   it parses it into a tag section parser. */
bool debDebFile::MemControlExtract::Read(debDebFile &Deb)
{
   if (Deb.ExtractTarMember(*this, "control.tar") == false)
      return false;

   if (Control == 0)
      return true;
   
   Control[Length] = '\n';
   Control[Length+1] = '\n';
   if (Section.Scan(Control,Length+2) == false)
      return _error->Error(_("Unparsable control file"));
   return true;
}
									/*}}}*/
// MemControlExtract::TakeControl - Parse a memory block		/*{{{*/
// ---------------------------------------------------------------------
/* The given memory block is loaded into the parser and parsed as a control
   record. */
bool debDebFile::MemControlExtract::TakeControl(const void *Data,unsigned long long Size)
{
   delete [] Control;
   Control = new char[Size+2];
   Length = Size;
   memcpy(Control,Data,Size);
   
   Control[Length] = '\n';
   Control[Length+1] = '\n';
   return Section.Scan(Control,Length+2);
}
									/*}}}*/

