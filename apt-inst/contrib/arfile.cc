// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: arfile.cc,v 1.6.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   AR File - Handle an 'AR' archive
   
   AR Archives have plain text headers at the start of each file
   section. The headers are aligned on a 2 byte boundry.
   
   Information about the structure of AR files can be found in ar(5)
   on a BSD system, or in the binutils source.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/arfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <stdlib.h>
									/*}}}*/
#include <apti18n.h>

struct ARArchive::MemberHeader
{
   char Name[16];
   char MTime[12];
   char UID[6];
   char GID[6];
   char Mode[8];
   char Size[10];
   char Magic[2];
};

// ARArchive::ARArchive - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
ARArchive::ARArchive(FileFd &File) : List(0), File(File)
{
   LoadHeaders();
}
									/*}}}*/
// ARArchive::~ARArchive - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
ARArchive::~ARArchive()
{
   while (List != 0)
   {
      Member *Tmp = List;
      List = List->Next;
      delete Tmp;
   }   
}
									/*}}}*/
// ARArchive::LoadHeaders - Load the headers from each file		/*{{{*/
// ---------------------------------------------------------------------
/* AR files are structured with a 8 byte magic string followed by a 60
   byte plain text header then the file data, another header, data, etc */
bool ARArchive::LoadHeaders()
{
   signed long Left = File.Size();
   
   // Check the magic byte
   char Magic[8];
   if (File.Read(Magic,sizeof(Magic)) == false)
      return false;
   if (memcmp(Magic,"!<arch>\012",sizeof(Magic)) != 0)
      return _error->Error(_("Invalid archive signature"));
   Left -= sizeof(Magic);
   
   // Read the member list
   while (Left > 0)
   {
      MemberHeader Head;
      if (File.Read(&Head,sizeof(Head)) == false)
	 return _error->Error(_("Error reading archive member header"));
      Left -= sizeof(Head);

      // Convert all of the integer members
      Member *Memb = new Member();
      if (StrToNum(Head.MTime,Memb->MTime,sizeof(Head.MTime)) == false ||
	  StrToNum(Head.UID,Memb->UID,sizeof(Head.UID)) == false ||
	  StrToNum(Head.GID,Memb->GID,sizeof(Head.GID)) == false ||
	  StrToNum(Head.Mode,Memb->Mode,sizeof(Head.Mode),8) == false ||
	  StrToNum(Head.Size,Memb->Size,sizeof(Head.Size)) == false)
      {
	 delete Memb;
	 return _error->Error(_("Invalid archive member header"));
      }
	 
      // Check for an extra long name string
      if (memcmp(Head.Name,"#1/",3) == 0)
      {
	 char S[300];
	 unsigned long Len;
	 if (StrToNum(Head.Name+3,Len,sizeof(Head.Size)-3) == false ||
	     Len >= strlen(S))
	 {
	    delete Memb;
	    return _error->Error(_("Invalid archive member header"));
	 }
	 if (File.Read(S,Len) == false)
	    return false;
	 S[Len] = 0;
	 Memb->Name = S;
	 Memb->Size -= Len;
	 Left -= Len;
      }
      else
      {
	 unsigned int I = sizeof(Head.Name) - 1;
	 for (; Head.Name[I] == ' ' || Head.Name[I] == '/'; I--);
	 Memb->Name = string(Head.Name,I+1);
      }

      // Account for the AR header alignment 
      unsigned Skip = Memb->Size % 2;
      
      // Add it to the list
      Memb->Next = List;
      List = Memb;
      Memb->Start = File.Tell();
      if (File.Skip(Memb->Size + Skip) == false)
	 return false;
      if (Left < (signed)(Memb->Size + Skip))
	 return _error->Error(_("Archive is too short"));
      Left -= Memb->Size + Skip;
   }   
   if (Left != 0)
      return _error->Error(_("Failed to read the archive headers"));
   
   return true;
}
									/*}}}*/
// ARArchive::FindMember - Find a name in the member list		/*{{{*/
// ---------------------------------------------------------------------
/* Find a member with the given name */
const ARArchive::Member *ARArchive::FindMember(const char *Name) const
{
   const Member *Res = List;
   while (Res != 0)
   {
      if (Res->Name == Name)
	 return Res;
      Res = Res->Next;
   }
   
   return 0;
}
									/*}}}*/
