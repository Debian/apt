// Copyright (c) 2014 Anthony Towns
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <config.h>

#ifndef APT_EXCLUDE_RRED_METHOD_CODE
#include "aptmethod.h"
#include <apt-pkg/configuration.h>
#include <apt-pkg/init.h>
#endif

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>

#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <stddef.h>

#include <cassert>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <apti18n.h>

#ifndef APT_MEMBLOCK_SIZE
#define APT_MEMBLOCK_SIZE (512*1024)
#endif

static bool ShowHelp(CommandLine &)
{
   std::cout <<
      "Usage: rred [options] -t input output patch-1 … patch-N\n"
      "       rred [options] -f patch-1 … patch-N < input > output\n"
      "       rred [options] patch-1 … patch-N > merged-patch\n"
      "\n"
      "The main use of this binary is by APTs acquire system, a mode reached\n"
      "by calling it without any arguments and driven via messages on stdin.\n"
      "\n"
      "For the propose of testing as well as simpler direct usage the above\n"
      "mentioned modes to work with \"reversed restricted ed\" patches as well.\n"
      "\n"
      "The arguments used above are:\n"
      "* input: denotes a file you want to patch.\n"
      "* output: a file you want to store the patched content in.\n"
      "* patch-1 … patch-N: One or more files containing a patch.\n"
      "* merged-patch: All changes by patch-1 … patch-N in one patch.\n"
      "\n"
      "This rred supports the commands 'a', 'c' and 'd', both single as well\n"
      "as multi line. Other commands are not supported (hence 'restricted').\n"
      "The command to patch the last line must appear first in the patch\n"
      "(hence 'reversed'). Such a patch can e.g. be produced with 'diff --ed'.\n"
      ;
   return true;
}

class MemBlock {
   char *start;
   size_t size;
   char *free;
   MemBlock *next = nullptr;

   explicit MemBlock(size_t size) : size(size)
   {
      free = start = new char[size];
   }

   size_t avail(void) { return size - (free - start); }

   public:

   MemBlock() : MemBlock(APT_MEMBLOCK_SIZE) {}

   ~MemBlock() {
      delete [] start;
      delete next;
   }

   void clear(void) {
      free = start;
      if (next)
	 next->clear();
   }

   char *add_easy(char *src, size_t len, char *last)
   {
      if (last) {
	 for (MemBlock *k = this; k; k = k->next) {
	    if (k->free == last) {
	       if (len <= k->avail()) {
		  char * const n = k->add(src, len);
		  assert(last == n); // we checked already that the block is big enough, so a new one shouldn't be used
		  return (last == n) ? nullptr : n;
	       } else {
		  break;
	       }
	    } else if (last >= start && last < free) {
	       break;
	    }
	 }
      }
      return add(src, len);
   }

   char *add(char *src, size_t len) {
      if (len > avail()) {
	 if (!next) {
	    if (len > APT_MEMBLOCK_SIZE)  {
	       next = new MemBlock(len);
	    } else {
	       next = new MemBlock();
	    }
	 }
	 return next->add(src, len);
      }
      char *dst = free;
      free += len;
      memcpy(dst, src, len);
      return dst;
   }
};

struct Change {
   /* Ordering:
    *
    *   1. write out <offset> lines unchanged
    *   2. skip <del_cnt> lines from source
    *   3. write out <add_cnt> lines (<add>/<add_len>)
    */
   size_t offset;
   size_t del_cnt;
   size_t add_cnt; /* lines */
   size_t add_len; /* bytes */
   char *add;

   explicit Change(size_t off)
   {
      offset = off;
      del_cnt = add_cnt = add_len = 0;
      add = NULL;
   }

   /* actually, don't write <lines> lines from <add> */
   bool skip_lines(size_t lines)
   {
      while (lines > 0) {
	 char *s = (char*) memchr(add, '\n', add_len);
	 if (s == nullptr)
	    return _error->Error("No line left in add_len data to skip (1)");
	 s++;
	 add_len -= (s - add);
	 add_cnt--;
	 lines--;
	 if (add_len == 0) {
	    add = nullptr;
	    if (add_cnt != 0 || lines != 0)
	       return _error->Error("No line left in add_len data to skip (2)");
	 } else {
	    add = s;
	    if (add_cnt == 0)
	       return _error->Error("No line left in add_len data to skip (3)");
	 }
      }
      return true;
   }
};

class FileChanges {
   std::list<struct Change> changes;
   std::list<struct Change>::iterator where;
   size_t pos; // line number is as far left of iterator as possible

   bool pos_is_okay(void) const
   {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
      // this isn't unsafe, it is just a moderately expensive check we want to avoid normally
      size_t cpos = 0;
      std::list<struct Change>::const_iterator x;
      for (x = changes.begin(); x != where; ++x) {
	 assert(x != changes.end());
	 cpos += x->offset + x->add_cnt;
      }
      return cpos == pos;
#else
      return true;
#endif
   }

   public:
   FileChanges() {
      where = changes.end();
      pos = 0;
   }

   std::list<struct Change>::iterator begin(void) { return changes.begin(); }
   std::list<struct Change>::iterator end(void) { return changes.end(); }

   std::list<struct Change>::reverse_iterator rbegin(void) { return changes.rbegin(); }
   std::list<struct Change>::reverse_iterator rend(void) { return changes.rend(); }

   bool add_change(Change c) {
      assert(pos_is_okay());
      if (not go_to_change_for(c.offset) ||
	  pos + where->offset != c.offset)
	 return false;
      if (c.del_cnt > 0)
	 if (not delete_lines(c.del_cnt))
	    return false;
      if (pos + where->offset != c.offset)
	 return false;
      if (c.add_len > 0) {
	 assert(pos_is_okay());
	 if (where->add_len > 0)
	    if (not new_change())
	       return false;
	 if (where->add_len != 0 || where->add_cnt != 0)
	    return false;

	 where->add_len = c.add_len;
	 where->add_cnt = c.add_cnt;
	 where->add = c.add;
      }
      assert(pos_is_okay());
      if (not merge())
	 return false;
      return pos_is_okay();
   }

   private:
   bool merge(void)
   {
      while (where->offset == 0 && where != changes.begin()) {
	 if (not left())
	    return false;
      }
      std::list<struct Change>::iterator next = where;
      ++next;

      while (next != changes.end() && next->offset == 0) {
	 where->del_cnt += next->del_cnt;
	 next->del_cnt = 0;
	 if (next->add == NULL) {
	    next = changes.erase(next);
	 } else if (where->add == NULL) {
	    where->add = next->add;
	    where->add_len = next->add_len;
	    where->add_cnt = next->add_cnt;
	    next = changes.erase(next);
	 } else {
	    ++next;
	 }
      }
      return true;
   }

   bool go_to_change_for(size_t line)
   {
      while(where != changes.end()) {
	 if (line < pos) {
	    if (not left())
	       return false;
	    continue;
	 }
	 if (pos + where->offset + where->add_cnt <= line) {
	    if (not right())
	       return false;
	    continue;
	 }
	 // line is somewhere in this slot
	 if (line < pos + where->offset) {
	    break;
	 } else if (line == pos + where->offset) {
	    return true;
	 } else {
	    if (not split(line - pos))
	       return false;
	    return right();
	 }
      }
      /* it goes before this patch */
      return insert(line-pos);
   }

   bool new_change(void) { return insert(where->offset); }

   bool insert(size_t offset)
   {
      assert(pos_is_okay());
      if (where != changes.end() && offset > where->offset)
	 return false;
      if (where != changes.end())
	 where->offset -= offset;
      changes.insert(where, Change(offset));
      --where;
      return pos_is_okay();
   }

   bool split(size_t offset)
   {
      assert(pos_is_okay());
      if (where->offset >= offset || offset >= where->offset + where->add_cnt)
	 return false;

      size_t keep_lines = offset - where->offset;

      Change before(*where);

      where->del_cnt = 0;
      where->offset = 0;
      if (not where->skip_lines(keep_lines))
	 return false;

      before.add_cnt = keep_lines;
      before.add_len -= where->add_len;

      changes.insert(where, before);
      --where;
      return pos_is_okay();
   }

   bool delete_lines(size_t cnt)
   {
      assert(pos_is_okay());
      std::list<struct Change>::iterator x = where;
      while (cnt > 0)
      {
	 size_t del;
	 del = x->add_cnt;
	 if (del > cnt)
	    del = cnt;
	 if (not x->skip_lines(del))
	    return false;
	 cnt -= del;

	 ++x;
	 if (x == changes.end()) {
	    del = cnt;
	 } else {
	    del = x->offset;
	    if (del > cnt)
	       del = cnt;
	    x->offset -= del;
	 }
	 where->del_cnt += del;
	 cnt -= del;
      }
      return pos_is_okay();
   }

   bool left(void) {
      assert(pos_is_okay());
      --where;
      pos -= where->offset + where->add_cnt;
      return pos_is_okay();
   }

   bool right(void) {
      assert(pos_is_okay());
      pos += where->offset + where->add_cnt;
      ++where;
      return pos_is_okay();
   }
};

class Patch {
   FileChanges filechanges;
   MemBlock add_text;

   static bool retry_fwrite(char *b, size_t l, FileFd &f, Hashes * const start_hash, Hashes * const end_hash = nullptr) APT_NONNULL(1)
   {
      if (f.Write(b, l) == false)
	 return false;
      if (start_hash)
	 start_hash->Add((unsigned char*)b, l);
      if (end_hash)
	 end_hash->Add((unsigned char*)b, l);
      return true;
   }

   static void dump_rest(FileFd &o, FileFd &i,
	 Hashes * const start_hash, Hashes * const end_hash)
   {
      char buffer[APT_MEMBLOCK_SIZE];
      unsigned long long l = 0;
      while (i.Read(buffer, sizeof(buffer), &l)) {
	 if (l ==0  || !retry_fwrite(buffer, l, o, start_hash, end_hash))
	    break;
      }
   }

   static void dump_lines(FileFd &o, FileFd &i, size_t n,
	 Hashes * const start_hash, Hashes * const end_hash)
   {
      char buffer[APT_MEMBLOCK_SIZE];
      while (n > 0) {
	 if (i.ReadLine(buffer, sizeof(buffer)) == NULL)
	    buffer[0] = '\0';
	 size_t const l = strlen(buffer);
	 if (l == 0 || buffer[l-1] == '\n')
	    n--;
	 retry_fwrite(buffer, l, o, start_hash, end_hash);
      }
   }

   static void skip_lines(FileFd &i, int n, Hashes * const start_hash)
   {
      char buffer[APT_MEMBLOCK_SIZE];
      while (n > 0) {
	 if (i.ReadLine(buffer, sizeof(buffer)) == NULL)
	    buffer[0] = '\0';
	 size_t const l = strlen(buffer);
	 if (l == 0 || buffer[l-1] == '\n')
	    n--;
	 if (start_hash)
	    start_hash->Add((unsigned char*)buffer, l);
      }
   }

   static void dump_mem(FileFd &o, char *p, size_t s, Hashes *hash) APT_NONNULL(2) {
      retry_fwrite(p, s, o, nullptr, hash);
   }

   public:

   bool read_diff(FileFd &f, Hashes * const h)
   {
      char buffer[APT_MEMBLOCK_SIZE];
      bool cmdwanted = true;

      Change ch(std::numeric_limits<size_t>::max());
      if (f.ReadLine(buffer, sizeof(buffer)) == nullptr)
      {
	 if (f.Eof())
	    return true;
	 return _error->Error("Reading first line of patchfile %s failed", f.Name().c_str());
      }
      do {
	 if (h != NULL)
	    h->Add(buffer);
	 if (cmdwanted) {
	    char *m, *c;
	    size_t s, e;
	    errno = 0;
	    s = strtoul(buffer, &m, 10);
	    if (unlikely(m == buffer || s == std::numeric_limits<unsigned long>::max() || errno != 0))
	       return _error->Error("Parsing patchfile %s failed: Expected an effected line start", f.Name().c_str());
	    else if (*m == ',') {
	       ++m;
	       e = strtol(m, &c, 10);
	       if (unlikely(m == c || e == std::numeric_limits<unsigned long>::max() || errno != 0))
		  return _error->Error("Parsing patchfile %s failed: Expected an effected line end", f.Name().c_str());
	       if (unlikely(e < s))
		  return _error->Error("Parsing patchfile %s failed: Effected lines end %lu is before start %lu", f.Name().c_str(), e, s);
	    } else {
	       e = s;
	       c = m;
	    }
	    if (s > ch.offset)
	       return _error->Error("Parsing patchfile %s failed: Effected line is after previous effected line", f.Name().c_str());
	    switch(*c) {
	       case 'a':
		  cmdwanted = false;
		  ch.add = NULL;
		  ch.add_cnt = 0;
		  ch.add_len = 0;
		  ch.offset = s;
		  ch.del_cnt = 0;
		  break;
	       case 'c':
		  if (unlikely(s == 0))
		     return _error->Error("Parsing patchfile %s failed: Change command can't effect line zero", f.Name().c_str());
		  cmdwanted = false;
		  ch.add = NULL;
		  ch.add_cnt = 0;
		  ch.add_len = 0;
		  ch.offset = s - 1;
		  ch.del_cnt = e - s + 1;
		  break;
	       case 'd':
		  if (unlikely(s == 0))
		     return _error->Error("Parsing patchfile %s failed: Delete command can't effect line zero", f.Name().c_str());
		  ch.offset = s - 1;
		  ch.del_cnt = e - s + 1;
		  ch.add = NULL;
		  ch.add_cnt = 0;
		  ch.add_len = 0;
		  if (not filechanges.add_change(ch))
		     return _error->Error("Parsing patchfile %s failed: Delete command could not be added to changes", f.Name().c_str());
		  break;
	       default:
		  return _error->Error("Parsing patchfile %s failed: Unknown command", f.Name().c_str());
	    }
	 } else { /* !cmdwanted */
	    if (strcmp(buffer, ".\n") == 0) {
	       cmdwanted = true;
	       if (not filechanges.add_change(ch))
		  return _error->Error("Parsing patchfile %s failed: Data couldn't be added for command (1)", f.Name().c_str());
	    } else {
	       char *last = NULL;
	       char *add;
	       size_t l;
	       if (ch.add)
		  last = ch.add + ch.add_len;
	       l = strlen(buffer);
	       add = add_text.add_easy(buffer, l, last);
	       if (!add) {
		  ch.add_len += l;
		  ch.add_cnt++;
	       } else {
		  if (ch.add) {
		     if (not filechanges.add_change(ch))
			return _error->Error("Parsing patchfile %s failed: Data couldn't be added for command (2)", f.Name().c_str());
		     ch.del_cnt = 0;
		  }
		  ch.offset += ch.add_cnt;
		  ch.add = add;
		  ch.add_len = l;
		  ch.add_cnt = 1;
	       }
	    }
	 }
      } while(f.ReadLine(buffer, sizeof(buffer)));
      return true;
   }

   void write_diff(FileFd &f)
   {
      unsigned long long line = 0;
      std::list<struct Change>::reverse_iterator ch;
      for (ch = filechanges.rbegin(); ch != filechanges.rend(); ++ch) {
	 line += ch->offset + ch->del_cnt;
      }

      for (ch = filechanges.rbegin(); ch != filechanges.rend(); ++ch) {
	 std::list<struct Change>::reverse_iterator mg_i, mg_e = ch;
	 while (ch->del_cnt == 0 && ch->offset == 0)
	 {
	    ++ch;
	    if (unlikely(ch == filechanges.rend()))
	       return;
	 }
	 line -= ch->del_cnt;
	 std::string buf;
	 if (ch->add_cnt > 0) {
	    if (ch->del_cnt == 0) {
	       strprintf(buf, "%llua\n", line);
	    } else if (ch->del_cnt == 1) {
	       strprintf(buf, "%lluc\n", line+1);
	    } else {
	       strprintf(buf, "%llu,%lluc\n", line+1, line+ch->del_cnt);
	    }
	    f.Write(buf.c_str(), buf.length());

	    mg_i = ch;
	    do {
	       dump_mem(f, mg_i->add, mg_i->add_len, NULL);
	    } while (mg_i-- != mg_e);

	    buf = ".\n";
	    f.Write(buf.c_str(), buf.length());
	 } else if (ch->del_cnt == 1) {
	    strprintf(buf, "%llud\n", line+1);
	    f.Write(buf.c_str(), buf.length());
	 } else if (ch->del_cnt > 1) {
	    strprintf(buf, "%llu,%llud\n", line+1, line+ch->del_cnt);
	    f.Write(buf.c_str(), buf.length());
	 }
	 line -= ch->offset;
      }
   }

   void apply_against_file(FileFd &out, FileFd &in,
	 Hashes * const start_hash = nullptr, Hashes * const end_hash = nullptr)
   {
      std::list<struct Change>::iterator ch;
      for (ch = filechanges.begin(); ch != filechanges.end(); ++ch) {
	 dump_lines(out, in, ch->offset, start_hash, end_hash);
	 skip_lines(in, ch->del_cnt, start_hash);
	 if (ch->add_len != 0)
	    dump_mem(out, ch->add, ch->add_len, end_hash);
      }
      dump_rest(out, in, start_hash, end_hash);
      out.Flush();
   }
};

#ifndef APT_EXCLUDE_RRED_METHOD_CODE
class RredMethod : public aptMethod {
   private:
      bool Debug;

      struct PDiffFile {
	 std::string FileName;
	 HashStringList ExpectedHashes;
	 PDiffFile(std::string const &FileName, HashStringList const &ExpectedHashes) :
	    FileName(FileName), ExpectedHashes(ExpectedHashes) {}
      };

      HashStringList ReadExpectedHashesForPatch(unsigned int const patch, std::string const &Message)
      {
	 HashStringList ExpectedHashes;
	 for (char const * const * type = HashString::SupportedHashes(); *type != NULL; ++type)
	 {
	    std::string tagname;
	    strprintf(tagname, "Patch-%d-%s-Hash", patch, *type);
	    std::string const hashsum = LookupTag(Message, tagname.c_str());
	    if (hashsum.empty() == false)
	       ExpectedHashes.push_back(HashString(*type, hashsum));
	 }
	 return ExpectedHashes;
      }

   protected:
      virtual bool URIAcquire(std::string const &Message, FetchItem *Itm) APT_OVERRIDE {
	 Debug = DebugEnabled();
	 URI Get(Itm->Uri);
	 std::string Path = DecodeSendURI(Get.Host + Get.Path); // rred:/path - no host

	 FetchResult Res;
	 Res.Filename = Itm->DestFile;
	 if (Itm->Uri.empty())
	 {
	    Path = Itm->DestFile;
	    Itm->DestFile.append(".result");
	 } else
	    URIStart(Res);

	 std::vector<PDiffFile> patchfiles;
	 Patch patch;

	 HashStringList StartHashes;
	 for (char const * const * type = HashString::SupportedHashes(); *type != nullptr; ++type)
	 {
	    std::string tagname;
	    strprintf(tagname, "Start-%s-Hash", *type);
	    std::string const hashsum = LookupTag(Message, tagname.c_str());
	    if (hashsum.empty() == false)
	       StartHashes.push_back(HashString(*type, hashsum));
	 }

	 if (FileExists(Path + ".ed") == true)
	 {
	    HashStringList const ExpectedHashes = ReadExpectedHashesForPatch(0, Message);
	    std::string const FileName = Path + ".ed";
	    if (ExpectedHashes.usable() == false)
	       return _error->Error("No hashes found for uncompressed patch: %s", FileName.c_str());
	    patchfiles.push_back(PDiffFile(FileName, ExpectedHashes));
	 }
	 else
	 {
	    _error->PushToStack();
	    std::vector<std::string> patches = GetListOfFilesInDir(flNotFile(Path), "gz", true, false);
	    _error->RevertToStack();

	    std::string const baseName = Path + ".ed.";
	    unsigned int seen_patches = 0;
	    for (std::vector<std::string>::const_iterator p = patches.begin();
		  p != patches.end(); ++p)
	    {
	       if (p->compare(0, baseName.length(), baseName) == 0)
	       {
		  HashStringList const ExpectedHashes = ReadExpectedHashesForPatch(seen_patches, Message);
		  if (ExpectedHashes.usable() == false)
		     return _error->Error("No hashes found for uncompressed patch %d: %s", seen_patches, p->c_str());
		  patchfiles.push_back(PDiffFile(*p, ExpectedHashes));
		  ++seen_patches;
	       }
	    }
	 }

	 std::string patch_name;
	 for (std::vector<PDiffFile>::iterator I = patchfiles.begin();
	       I != patchfiles.end();
	       ++I)
	 {
	    patch_name = I->FileName;
	    if (Debug == true)
	       std::clog << "Patching " << Path << " with " << patch_name
		  << std::endl;

	    FileFd p;
	    Hashes patch_hash(I->ExpectedHashes);
	    // all patches are compressed, even if the name doesn't reflect it
	    if (p.Open(patch_name, FileFd::ReadOnly, FileFd::Gzip) == false ||
		  patch.read_diff(p, &patch_hash) == false)
	    {
	       _error->DumpErrors(std::cerr, GlobalError::DEBUG, false);
	       return false;
	    }
	    p.Close();
	    HashStringList const hsl = patch_hash.GetHashStringList();
	    if (hsl != I->ExpectedHashes)
	       return _error->Error("Hash Sum mismatch for uncompressed patch %s", patch_name.c_str());
	 }

	 if (Debug == true)
	    std::clog << "Applying patches against " << Path
	       << " and writing results to " << Itm->DestFile
	       << std::endl;

	 FileFd inp, out;
	 if (inp.Open(Path, FileFd::ReadOnly, FileFd::Extension) == false)
	 {
	    if (Debug == true)
	       std::clog << "FAILED to open inp " << Path << std::endl;
	    return _error->Error("Failed to open inp %s", Path.c_str());
	 }
	 if (out.Open(Itm->DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Empty | FileFd::BufferedWrite, FileFd::Extension) == false)
	 {
	    if (Debug == true)
	       std::clog << "FAILED to open out " << Itm->DestFile << std::endl;
	    return _error->Error("Failed to open out %s", Itm->DestFile.c_str());
	 }

	 Hashes end_hash(Itm->ExpectedHashes);
	 if (StartHashes.usable())
	 {
	    Hashes start_hash(StartHashes);
	    patch.apply_against_file(out, inp, &start_hash, &end_hash);
	    if (start_hash.GetHashStringList() != StartHashes)
	       _error->Error("The input file hadn't the expected hash!");
	 }
	 else
	    patch.apply_against_file(out, inp, nullptr, &end_hash);

	 out.Close();
	 inp.Close();

	 if (_error->PendingError() == true) {
	    if (Debug == true)
	       std::clog << "FAILED to read or write files" << std::endl;
	    return false;
	 }

	 if (Debug == true) {
	    std::clog << "rred: finished file patching of " << Path  << "." << std::endl;
	 }

	 struct stat bufbase, bufpatch;
	 if (stat(Path.c_str(), &bufbase) != 0 ||
	       stat(patch_name.c_str(), &bufpatch) != 0)
	    return _error->Errno("stat", _("Failed to stat %s"), Path.c_str());

	 struct timeval times[2];
	 times[0].tv_sec = bufbase.st_atime;
	 times[1].tv_sec = bufpatch.st_mtime;
	 times[0].tv_usec = times[1].tv_usec = 0;
	 if (utimes(Itm->DestFile.c_str(), times) != 0)
	    return _error->Errno("utimes",_("Failed to set modification time"));

	 if (stat(Itm->DestFile.c_str(), &bufbase) != 0)
	    return _error->Errno("stat", _("Failed to stat %s"), Itm->DestFile.c_str());

	 Res.LastModified = bufbase.st_mtime;
	 Res.Size = bufbase.st_size;
	 Res.TakeHashes(end_hash);
	 URIDone(Res);

	 return true;
      }

   public:
   RredMethod() : aptMethod("rred", "2.0", SendConfig | SendURIEncoded), Debug(false)
   {
      SeccompFlags = aptMethod::BASE | aptMethod::DIRECTORY;
   }
};

static const APT::Configuration::Compressor *FindCompressor(std::vector<APT::Configuration::Compressor> const &compressors, std::string const &name) /*{{{*/
{
   APT::Configuration::Compressor const * compressor = nullptr;
   for (auto const & c : compressors)
   {
      if (compressor != nullptr && c.Cost >= compressor->Cost)
         continue;
      if (c.Name == name || c.Extension == name || (!c.Extension.empty() && c.Extension.substr(1) == name))
         compressor = &c;
   }
   return compressor;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()
{
   return {{nullptr, nullptr, nullptr}};
}
int main(int argc, const char *argv[])
{
   if (argc <= 1)
      return RredMethod().Run();

   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::RRED, &_config, nullptr, argc, argv, &ShowHelp, &GetCommands);

   FileFd input, output;
   unsigned int argi = 0;
   auto const argmax = CmdL.FileSize();
   bool const quiet = _config->FindI("quiet", 0) >= 2;

   std::string const compressorName = _config->Find("Rred::Compress", "");
   auto const compressors = APT::Configuration::getCompressors();
   APT::Configuration::Compressor const * compressor = nullptr;
   if (not compressorName.empty())
   {
      compressor = FindCompressor(compressors, compressorName);
      if (compressor == nullptr)
      {
	 std::cerr << "E: Could not find compressor: " << compressorName << '\n';
	 return 101;
      }
   }

   bool just_diff = false;
   if (_config->FindB("Rred::T", false))
   {
      if (argmax < 3)
      {
	 std::cerr << "E: Not enough filenames given on the command line for mode 't'\n";
	 return 101;
      }
      if (not quiet)
	 std::clog << "Patching " << CmdL.FileList[0] << " into " << CmdL.FileList[1] << "\n";
      input.Open(CmdL.FileList[0], FileFd::ReadOnly,FileFd::Extension);
      if (compressor == nullptr)
	 output.Open(CmdL.FileList[1], FileFd::WriteOnly | FileFd::Create | FileFd::Empty | FileFd::BufferedWrite, FileFd::Extension);
      else
	 output.Open(CmdL.FileList[1], FileFd::WriteOnly | FileFd::Create | FileFd::Empty | FileFd::BufferedWrite, *compressor);
      argi = 2;
   }
   else
   {
      if (compressor == nullptr)
	 output.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly | FileFd::Create | FileFd::BufferedWrite);
      else
	 output.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly | FileFd::Create | FileFd::BufferedWrite, *compressor);
      if (_config->FindB("Rred::F", false))
	 input.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly);
      else
	 just_diff = true;
   }

   if (argi + 1 > argmax)
   {
	 std::cerr << "E: At least one patch needs to be given on the command line\n";
	 return 101;
   }

   Patch merged_patch;
   for (; argi < argmax; ++argi)
   {
      FileFd patch;
      if (not patch.Open(CmdL.FileList[argi], FileFd::ReadOnly, FileFd::Extension))
      {
	 _error->DumpErrors(std::cerr);
	 return 1;
      }
      if (not merged_patch.read_diff(patch, nullptr))
      {
	 _error->DumpErrors(std::cerr);
	 return 2;
      }
   }

   if (just_diff)
      merged_patch.write_diff(output);
   else
      merged_patch.apply_against_file(output, input);

   output.Close();
   input.Close();
   return DispatchCommandLine(CmdL, {});
}
#endif
