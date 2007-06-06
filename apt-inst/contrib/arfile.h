// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: arfile.h,v 1.2 2001/02/20 07:03:16 jgg Exp $
/* ######################################################################

   AR File - Handle an 'AR' archive
   
   This is a reader for the usual 4.4 BSD AR format. It allows raw
   stream access to a single member at a time. Basically all this class
   provides is header parsing and verification. It is up to the client
   to correctly make use of the stream start/stop points.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ARFILE_H
#define PKGLIB_ARFILE_H


#include <string>
#include <apt-pkg/fileutl.h>

class ARArchive
{
   struct MemberHeader;
   public:
   struct Member;
   
   protected:

   // Linked list of members
   Member *List;
   
   bool LoadHeaders();

   public:
   
   // The stream file
   FileFd &File;

   // Locate a member by name
   const Member *FindMember(const char *Name) const;
   
   ARArchive(FileFd &File);
   ~ARArchive();
};

// A member of the archive
struct ARArchive::Member
{
   // Fields from the header
   string Name;
   unsigned long MTime;
   unsigned long UID;
   unsigned long GID;
   unsigned long Mode;
   unsigned long Size;
   
   // Location of the data.
   unsigned long Start;
   Member *Next;
   
   Member() : Start(0), Next(0) {};
};

#endif
