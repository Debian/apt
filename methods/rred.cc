// Copyright (c) 2014 Anthony Towns
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/configuration.h>

#include <stddef.h>
#include <iostream>
#include <string>
#include <list>
#include <vector>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <apti18n.h>

#define BLOCK_SIZE (512*1024)

class MemBlock {
   char *start;
   size_t size;
   char *free;
   struct MemBlock *next;

   MemBlock(size_t size) : size(size), next(NULL)
   {
      free = start = new char[size];
   }

   size_t avail(void) { return size - (free - start); }

   public:

   MemBlock(void) {
      free = start = new char[BLOCK_SIZE];
      size = BLOCK_SIZE;
      next = NULL;
   }

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
		  char *n = k->add(src, len);
		  assert(last == n);
		  if (last == n)
		     return NULL;
		  return n;
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
	    if (len > BLOCK_SIZE)  {
	       next = new MemBlock(len);
	    } else {
	       next = new MemBlock;
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

   Change(int off)
   {
      offset = off;
      del_cnt = add_cnt = add_len = 0;
      add = NULL;
   }

   /* actually, don't write <lines> lines from <add> */
   void skip_lines(size_t lines)
   {
      while (lines > 0) {
	 char *s = (char*) memchr(add, '\n', add_len);
	 assert(s != NULL);
	 s++;
	 add_len -= (s - add);
	 add_cnt--;
	 lines--;
	 if (add_len == 0) {
	    add = NULL;
	    assert(add_cnt == 0);
	    assert(lines == 0);
	 } else {
	    add = s;
	    assert(add_cnt > 0);
	 }
      }
   }
};

class FileChanges {
   std::list<struct Change> changes;
   std::list<struct Change>::iterator where;
   size_t pos; // line number is as far left of iterator as possible

   bool pos_is_okay(void)
   {
#ifdef POSDEBUG
      size_t cpos = 0;
      std::list<struct Change>::iterator x;
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

   void add_change(Change c) {
      assert(pos_is_okay());
      go_to_change_for(c.offset);
      assert(pos + where->offset == c.offset);
      if (c.del_cnt > 0)
	 delete_lines(c.del_cnt);
      assert(pos + where->offset == c.offset);
      if (c.add_len > 0) {
	 assert(pos_is_okay());
	 if (where->add_len > 0)
	    new_change();
	 assert(where->add_len == 0 && where->add_cnt == 0);

	 where->add_len = c.add_len;
	 where->add_cnt = c.add_cnt;
	 where->add = c.add;
      }
      assert(pos_is_okay());
      merge();
      assert(pos_is_okay());
   }

   private:
   void merge(void)
   {
      while (where->offset == 0 && where != changes.begin()) {
	 left();
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
   }

   void go_to_change_for(size_t line)
   {
      while(where != changes.end()) {
	 if (line < pos) {
	    left();
	    continue;
	 }
	 if (pos + where->offset + where->add_cnt <= line) {
	    right();
	    continue;
	 }
	 // line is somewhere in this slot
	 if (line < pos + where->offset) {
	    break;
	 } else if (line == pos + where->offset) {
	    return;
	 } else {
	    split(line - pos);
	    right();
	    return;
	 }
      }
      /* it goes before this patch */
      insert(line-pos);
   }

   void new_change(void) { insert(where->offset); }

   void insert(size_t offset)
   {
      assert(pos_is_okay());
      assert(where == changes.end() || offset <= where->offset);
      if (where != changes.end())
	 where->offset -= offset;
      changes.insert(where, Change(offset));
      --where;
      assert(pos_is_okay());
   }

   void split(size_t offset)
   {
      assert(pos_is_okay());

      assert(where->offset < offset);
      assert(offset < where->offset + where->add_cnt);

      size_t keep_lines = offset - where->offset;

      Change before(*where);

      where->del_cnt = 0;
      where->offset = 0;
      where->skip_lines(keep_lines);

      before.add_cnt = keep_lines;
      before.add_len -= where->add_len;

      changes.insert(where, before);
      --where;
      assert(pos_is_okay());
   }

   void delete_lines(size_t cnt)
   {
      std::list<struct Change>::iterator x = where;
      assert(pos_is_okay());
      while (cnt > 0)
      {
	 size_t del;
	 del = x->add_cnt;
	 if (del > cnt)
	    del = cnt;
	 x->skip_lines(del);
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
      assert(pos_is_okay());
   }

   void left(void) {
      assert(pos_is_okay());
      --where;
      pos -= where->offset + where->add_cnt;
      assert(pos_is_okay());
   }

   void right(void) {
      assert(pos_is_okay());
      pos += where->offset + where->add_cnt;
      ++where;
      assert(pos_is_okay());
   }
};

class Patch {
   FileChanges filechanges;
   MemBlock add_text;

   static bool retry_fwrite(char *b, size_t l, FILE *f, Hashes *hash)
   {
      size_t r = 1;
      while (r > 0 && l > 0)
      {
         r = fwrite(b, 1, l, f);
	 if (hash)
	    hash->Add((unsigned char*)b, r);
	 l -= r;
	 b += r;
      }
      return l == 0;
   }

   static void dump_rest(FILE *o, FILE *i, Hashes *hash)
   {
      char buffer[BLOCK_SIZE];
      size_t l;
      while (0 < (l = fread(buffer, 1, sizeof(buffer), i))) {
	 if (!retry_fwrite(buffer, l, o, hash))
	    break;
      }
   }

   static void dump_lines(FILE *o, FILE *i, size_t n, Hashes *hash)
   {
      char buffer[BLOCK_SIZE];
      while (n > 0) {
	 if (fgets(buffer, sizeof(buffer), i) == 0)
	    buffer[0] = '\0';
	 size_t const l = strlen(buffer);
	 if (l == 0 || buffer[l-1] == '\n')
	    n--;
	 retry_fwrite(buffer, l, o, hash);
      }
   }

   static void skip_lines(FILE *i, int n)
   {
      char buffer[BLOCK_SIZE];
      while (n > 0) {
	 if (fgets(buffer, sizeof(buffer), i) == 0)
	    buffer[0] = '\0';
	 size_t const l = strlen(buffer);
	 if (l == 0 || buffer[l-1] == '\n')
	    n--;
      }
   }

   static void dump_mem(FILE *o, char *p, size_t s, Hashes *hash) {
      retry_fwrite(p, s, o, hash);
   }

   public:

   void read_diff(FileFd &f)
   {
      char buffer[BLOCK_SIZE];
      bool cmdwanted = true;

      Change ch(0);
      while(f.ReadLine(buffer, sizeof(buffer)))
      {
	 if (cmdwanted) {
	    char *m, *c;
	    size_t s, e;
	    s = strtol(buffer, &m, 10);
	    if (m == buffer) {
	       s = e = ch.offset + ch.add_cnt;
	       c = buffer;
	    } else if (*m == ',') {
	       m++;
	       e = strtol(m, &c, 10);
	    } else {
	       e = s;
	       c = m;
	    }
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
		  cmdwanted = false;
		  ch.add = NULL;
		  ch.add_cnt = 0;
		  ch.add_len = 0;
		  ch.offset = s - 1;
		  ch.del_cnt = e - s + 1;
		  break;
	       case 'd':
		  ch.offset = s - 1;
		  ch.del_cnt = e - s + 1;
		  ch.add = NULL;
		  ch.add_cnt = 0;
		  ch.add_len = 0;
		  filechanges.add_change(ch);
		  break;
	    }
	 } else { /* !cmdwanted */
	    if (buffer[0] == '.' && buffer[1] == '\n') {
	       cmdwanted = true;
	       filechanges.add_change(ch);
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
		     filechanges.add_change(ch);
		     ch.del_cnt = 0;
		  }
		  ch.offset += ch.add_cnt;
		  ch.add = add;
		  ch.add_len = l;
		  ch.add_cnt = 1;
	       }
	    }
	 }
      }
   }

   void write_diff(FILE *f)
   {
      unsigned long long line = 0;
      std::list<struct Change>::reverse_iterator ch;
      for (ch = filechanges.rbegin(); ch != filechanges.rend(); ++ch) {
	 line += ch->offset + ch->del_cnt;
      }

      for (ch = filechanges.rbegin(); ch != filechanges.rend(); ++ch) {
	 std::list<struct Change>::reverse_iterator mg_i, mg_e = ch;
	 while (ch->del_cnt == 0 && ch->offset == 0)
	    ++ch;
	 line -= ch->del_cnt;
	 if (ch->add_cnt > 0) {
	    if (ch->del_cnt == 0) {
	       fprintf(f, "%llua\n", line);
	    } else if (ch->del_cnt == 1) {
	       fprintf(f, "%lluc\n", line+1);
	    } else {
	       fprintf(f, "%llu,%lluc\n", line+1, line+ch->del_cnt);
	    }

	    mg_i = ch;
	    do {
	       dump_mem(f, mg_i->add, mg_i->add_len, NULL);
	    } while (mg_i-- != mg_e);

	    fprintf(f, ".\n");
	 } else if (ch->del_cnt == 1) {
	    fprintf(f, "%llud\n", line+1);
	 } else if (ch->del_cnt > 1) {
	    fprintf(f, "%llu,%llud\n", line+1, line+ch->del_cnt);
	 }
	 line -= ch->offset;
      }
   }

   void apply_against_file(FILE *out, FILE *in, Hashes *hash = NULL)
   {
      std::list<struct Change>::iterator ch;
      for (ch = filechanges.begin(); ch != filechanges.end(); ++ch) {
	 dump_lines(out, in, ch->offset, hash);
	 skip_lines(in, ch->del_cnt);
	 dump_mem(out, ch->add, ch->add_len, hash);
      }
      dump_rest(out, in, hash);
   }
};

class RredMethod : public pkgAcqMethod {
   private:
      bool Debug;

   protected:
      virtual bool Fetch(FetchItem *Itm) {
	 Debug = _config->FindB("Debug::pkgAcquire::RRed", false);
	 URI Get = Itm->Uri;
	 std::string Path = Get.Host + Get.Path; // rred:/path - no host

	 FetchResult Res;
	 Res.Filename = Itm->DestFile;
	 if (Itm->Uri.empty())
	 {
	    Path = Itm->DestFile;
	    Itm->DestFile.append(".result");
	 } else
	    URIStart(Res);

	 std::vector<std::string> patchpaths;
	 Patch patch;

	 if (FileExists(Path + ".ed") == true)
	    patchpaths.push_back(Path + ".ed");
	 else
	 {
	    _error->PushToStack();
	    std::vector<std::string> patches = GetListOfFilesInDir(flNotFile(Path), "gz", true, false);
	    _error->RevertToStack();

	    std::string const baseName = Path + ".ed.";
	    for (std::vector<std::string>::const_iterator p = patches.begin();
		  p != patches.end(); ++p)
	       if (p->compare(0, baseName.length(), baseName) == 0)
		  patchpaths.push_back(*p);
	 }

	 std::string patch_name;
	 for (std::vector<std::string>::iterator I = patchpaths.begin();
	       I != patchpaths.end();
	       ++I)
	 {
	    patch_name = *I;
	    if (Debug == true)
	       std::clog << "Patching " << Path << " with " << patch_name
		  << std::endl;

	    FileFd p;
	    // all patches are compressed, even if the name doesn't reflect it
	    if (p.Open(patch_name, FileFd::ReadOnly, FileFd::Gzip) == false) {
	       std::cerr << "Could not open patch file " << patch_name << std::endl;
	       _error->DumpErrors(std::cerr);
	       abort();
	    }
	    patch.read_diff(p);
	    p.Close();
	 }

	 if (Debug == true)
	    std::clog << "Applying patches against " << Path
	       << " and writing results to " << Itm->DestFile
	       << std::endl;

	 FILE *inp = fopen(Path.c_str(), "r");
	 FILE *out = fopen(Itm->DestFile.c_str(), "w");

	 Hashes hash;

	 patch.apply_against_file(out, inp, &hash);

	 fclose(out);
	 fclose(inp);

	 if (Debug == true) {
	    std::clog << "rred: finished file patching of " << Path  << "." << std::endl;
	 }

	 struct stat bufbase, bufpatch;
	 if (stat(Path.c_str(), &bufbase) != 0 ||
	       stat(patch_name.c_str(), &bufpatch) != 0)
	    return _error->Errno("stat", _("Failed to stat"));

	 struct timeval times[2];
	 times[0].tv_sec = bufbase.st_atime;
	 times[1].tv_sec = bufpatch.st_mtime;
	 times[0].tv_usec = times[1].tv_usec = 0;
	 if (utimes(Itm->DestFile.c_str(), times) != 0)
	    return _error->Errno("utimes",_("Failed to set modification time"));

	 if (stat(Itm->DestFile.c_str(), &bufbase) != 0)
	    return _error->Errno("stat", _("Failed to stat"));

	 Res.LastModified = bufbase.st_mtime;
	 Res.Size = bufbase.st_size;
	 Res.TakeHashes(hash);
	 URIDone(Res);

	 return true;
      }

   public:
      RredMethod() : pkgAcqMethod("2.0",SingleInstance | SendConfig), Debug(false) {}
};

int main(int argc, char **argv)
{
   int i;
   bool just_diff = true;
   Patch patch;

   if (argc <= 1) {
      RredMethod Mth;
      return Mth.Run();
   }

   if (argc > 1 && strcmp(argv[1], "-f") == 0) {
      just_diff = false;
      i = 2;
   } else {
      i = 1;
   }

   for (; i < argc; i++) {
      FileFd p;
      if (p.Open(argv[i], FileFd::ReadOnly) == false) {
	 _error->DumpErrors(std::cerr);
	 exit(1);
      }
      patch.read_diff(p);
   }

   if (just_diff) {
      patch.write_diff(stdout);
   } else {
      FILE *out, *inp;
      out = stdout;
      inp = stdin;

      patch.apply_against_file(out, inp);
   }
   return 0;
}
