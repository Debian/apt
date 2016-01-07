// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: tagfile.cc,v 1.37.2.2 2003/12/31 16:02:30 mdz Exp $
/* ######################################################################

   Fast scanner for RFC-822 type header information

   This uses a rotating buffer to load the package information into.
   The scanner runs over it and isolates and indexes a single section.

   This defines compat functions for the external code.

   ##################################################################### */
									/*}}}*/

#include<config.h>
#define APT_COMPILING_TAGFILE_COMPAT_CC
#include <apt-pkg/tagfile.h>

using std::string;
using APT::StringView;


bool pkgTagSection::Exists(const char* const Tag) const
{
   return Exists(StringView(Tag));
}

bool pkgTagSection::Find(const char *Tag,unsigned int &Pos) const
{
   return Find(StringView(Tag), Pos);
}

bool pkgTagSection::Find(const char *Tag,const char *&Start,
		         const char *&End) const
{
   return Find(StringView(Tag), Start, End);
}

string pkgTagSection::FindS(const char *Tag) const
{
   return Find(StringView(Tag)).to_string();
}

string pkgTagSection::FindRawS(const char *Tag) const
{
   return FindRaw(StringView(Tag)).to_string();
}

signed int pkgTagSection::FindI(const char *Tag,signed long Default) const
{
    return FindI(StringView(Tag), Default);
}

unsigned long long pkgTagSection::FindULL(const char *Tag, unsigned long long const &Default) const
{
   return FindULL(StringView(Tag), Default);
}
									/*}}}*/

bool pkgTagSection::FindB(const char *Tag, bool const &Default) const
{
   return FindB(StringView(Tag), Default);
}

bool pkgTagSection::FindFlag(const char * const Tag, uint8_t &Flags,
			     uint8_t const Flag) const
{
    return FindFlag(StringView(Tag), Flags, Flag);
}

bool pkgTagSection::FindFlag(const char *Tag,unsigned long &Flags,
			     unsigned long Flag) const
{
   return FindFlag(StringView(Tag), Flags, Flag);
}
