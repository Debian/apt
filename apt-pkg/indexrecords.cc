// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexrecords.cc,v 1.1.2.4 2003/12/30 02:11:43 mdz Exp $
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/indexrecords.h"
#endif
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apti18n.h>
#include <sys/stat.h>

string indexRecords::GetDist() const
{
   return this->Dist;
}

bool indexRecords::CheckDist(const string MaybeDist) const
{
   return (this->Dist == MaybeDist
	   || this->Suite == MaybeDist);
}

string indexRecords::GetExpectedDist() const
{
   return this->ExpectedDist;
}

const indexRecords::checkSum *indexRecords::Lookup(const string MetaKey)
{
   return Entries[MetaKey];
}

bool indexRecords::Load(const string Filename)
{
   FileFd Fd(Filename, FileFd::ReadOnly);
   pkgTagFile TagFile(&Fd, Fd.Size() + 256); // XXX
   if (_error->PendingError() == true)
   {
      ErrorText = _(("Unable to parse Release file " + Filename).c_str());
      return false;
   }

   pkgTagSection Section;
   if (TagFile.Step(Section) == false)
   {
      ErrorText = _(("No sections in Release file " + Filename).c_str());
      return false;
   }

   const char *Start, *End;
   Section.Get (Start, End, 0);
   Suite = Section.FindS("Suite");
   Dist = Section.FindS("Codename");
//    if (Dist.empty())
//    {
//       ErrorText = _(("No Codename entry in Release file " + Filename).c_str());
//       return false;
//    }
   if (!Section.Find("MD5Sum", Start, End))
   {
      ErrorText = _(("No MD5Sum entry in Release file " + Filename).c_str());
      return false;
   }
   string Name;
   string MD5Hash;
   size_t Size;
   while (Start < End)
   {
      if (!parseSumData(Start, End, Name, MD5Hash, Size))
	 return false;
      indexRecords::checkSum *Sum = new indexRecords::checkSum;
      Sum->MetaKeyFilename = Name;
      Sum->MD5Hash = MD5Hash;
      Sum->Size = Size;
      Entries[Name] = Sum;
   }
   
   string Strdate = Section.FindS("Date"); // FIXME: verify this somehow?
   return true;
}

vector<string> indexRecords::MetaKeys()
{
   std::vector<std::string> keys;
   std::map<string,checkSum *>::iterator I = Entries.begin();
   while(I != Entries.end()) {
      keys.push_back((*I).first);
      ++I;
   }
   return keys;
}

bool indexRecords::parseSumData(const char *&Start, const char *End,
				   string &Name, string &Hash, size_t &Size)
{
   Name = "";
   Hash = "";
   Size = 0;
   /* Skip over the first blank */
   while ((*Start == '\t' || *Start == ' ' || *Start == '\n')
	  && Start < End)
      Start++;
   if (Start >= End)
      return false;

   /* Move EntryEnd to the end of the first entry (the hash) */
   const char *EntryEnd = Start;
   while ((*EntryEnd != '\t' && *EntryEnd != ' ')
	  && EntryEnd < End)
      EntryEnd++;
   if (EntryEnd == End)
      return false;

   Hash.append(Start, EntryEnd-Start);

   /* Skip over intermediate blanks */
   Start = EntryEnd;
   while (*Start == '\t' || *Start == ' ')
      Start++;
   if (Start >= End)
      return false;
   
   EntryEnd = Start;
   /* Find the end of the second entry (the size) */
   while ((*EntryEnd != '\t' && *EntryEnd != ' ' )
	  && EntryEnd < End)
      EntryEnd++;
   if (EntryEnd == End)
      return false;
   
   Size = strtol (Start, NULL, 10);
      
   /* Skip over intermediate blanks */
   Start = EntryEnd;
   while (*Start == '\t' || *Start == ' ')
      Start++;
   if (Start >= End)
      return false;
   
   EntryEnd = Start;
   /* Find the end of the third entry (the filename) */
   while ((*EntryEnd != '\t' && *EntryEnd != ' ' && *EntryEnd != '\n')
	  && EntryEnd < End)
      EntryEnd++;

   Name.append(Start, EntryEnd-Start);
   Start = EntryEnd; //prepare for the next round
   return true;
}

indexRecords::indexRecords()
{
}

indexRecords::indexRecords(const string ExpectedDist) :
   ExpectedDist(ExpectedDist)
{
}
