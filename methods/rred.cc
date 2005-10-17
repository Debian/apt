#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <apti18n.h>

/* this method implements a patch functionality similar to "patch --ed" that is
 * used by the "tiffany" incremental packages download stuff. it differs from 
 * "ed" insofar that it is way more restricted (and therefore secure). in the
 * moment only the "c", "a" and "d" commands of ed are implemented (diff 
 * doesn't output any other). additionally the records must be reverse sorted 
 * by line number and may not overlap (diff *seems* to produce this kind of 
 * output). 
 * */

const char *Prog;

class RredMethod : public pkgAcqMethod
{
   bool Debug;
   // the size of this doesn't really matter (except for performance)    
   const static int BUF_SIZE = 1024;
   // the ed commands
   enum Mode {MODE_CHANGED, MODE_DELETED, MODE_ADDED};
   // return values
   enum State {ED_OK, ED_ORDERING, ED_PARSER, ED_FAILURE};
   // this applies a single hunk, it uses a tail recursion to 
   // reverse the hunks in the file
   int ed_rec(FILE *ed_cmds, FILE *in_file, FILE *out_file, int line, 
      char *buffer, unsigned int bufsize, Hashes *hash);
   // apply a patch file
   int ed_file(FILE *ed_cmds, FILE *in_file, FILE *out_file, Hashes *hash);
   // the methods main method
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   RredMethod() : pkgAcqMethod("1.1",SingleInstance | SendConfig) {};
};

int RredMethod::ed_rec(FILE *ed_cmds, FILE *in_file, FILE *out_file, int line, 
      char *buffer, unsigned int bufsize, Hashes *hash) {
   int pos;
   int startline;
   int stopline;
   int mode;
   int written;
   char *idx;

   /* get the current command and parse it*/
   if (fgets(buffer, bufsize, ed_cmds) == NULL) {
      return line;
   }
   startline = strtol(buffer, &idx, 10);
   if (startline < line) {
      return ED_ORDERING;
   }
   if (*idx == ',') {
      idx++;
      stopline = strtol(idx, &idx, 10);
   }
   else {
      stopline = startline;
   }
   if (*idx == 'c') {
      mode = MODE_CHANGED;
	   if (Debug == true) {
		   std::clog << "changing from line " << startline 
			     << " to " << stopline << std::endl;
	   }
   }
   else if (*idx == 'a') {
      mode = MODE_ADDED;
	   if (Debug == true) {
		   std::clog << "adding after line " << startline << std::endl;
	   }
   }
   else if (*idx == 'd') {
      mode = MODE_DELETED;
	   if (Debug == true) {
		   std::clog << "deleting from line " << startline 
			     <<  " to " << stopline << std::endl;
	   }
   }
   else {
      return ED_PARSER;
   }
   /* get the current position */
   pos = ftell(ed_cmds);
   /* if this is add or change then go to the next full stop */
   if ((mode == MODE_CHANGED) || (mode == MODE_ADDED)) {
      do {
         fgets(buffer, bufsize, ed_cmds);
         while ((strlen(buffer) == (bufsize - 1)) 
               && (buffer[bufsize - 2] != '\n')) {
            fgets(buffer, bufsize, ed_cmds);
            buffer[0] = ' ';
         }
      } while (strncmp(buffer, ".", 1) != 0);
   }
   /* do the recursive call */
   line = ed_rec(ed_cmds, in_file, out_file, line, buffer, bufsize, 
         hash);
   /* pass on errors */
   if (line < 0) {
      return line;
   }
   /* apply our hunk */
   fseek(ed_cmds, pos, SEEK_SET); 
   /* first wind to the current position */
   if (mode != MODE_ADDED) {
      startline -= 1;
   }
   while (line < startline) {
      fgets(buffer, bufsize, in_file);
      written = fwrite(buffer, 1, strlen(buffer), out_file);
      hash->Add((unsigned char*)buffer, written);
      while ((strlen(buffer) == (bufsize - 1)) 
            && (buffer[bufsize - 2] != '\n')) {
         fgets(buffer, bufsize, in_file);
         written = fwrite(buffer, 1, strlen(buffer), out_file);
         hash->Add((unsigned char*)buffer, written);
      }
      line++;
   }
   /* include from ed script */
   if ((mode == MODE_ADDED) || (mode == MODE_CHANGED)) {
      do {
         fgets(buffer, bufsize, ed_cmds);
         if (strncmp(buffer, ".", 1) != 0) {
            written = fwrite(buffer, 1, strlen(buffer), out_file);
            hash->Add((unsigned char*)buffer, written);
            while ((strlen(buffer) == (bufsize - 1)) 
                  && (buffer[bufsize - 2] != '\n')) {
               fgets(buffer, bufsize, ed_cmds);
               written = fwrite(buffer, 1, strlen(buffer), out_file);
               hash->Add((unsigned char*)buffer, written);
            }
         }
         else {
            break;
         }
      } while (1);
   }
   /* ignore the corresponding number of lines from input */
   if ((mode == MODE_DELETED) || (mode == MODE_CHANGED)) {
      while (line < stopline) {
         fgets(buffer, bufsize, in_file);
         while ((strlen(buffer) == (bufsize - 1)) 
               && (buffer[bufsize - 2] != '\n')) {
            fgets(buffer, bufsize, in_file);
         }
         line++;
      }
   }
   return line;
}

int RredMethod::ed_file(FILE *ed_cmds, FILE *in_file, FILE *out_file, 
      Hashes *hash) {
   char buffer[BUF_SIZE];
   int result;
   int written;
   
   /* we do a tail recursion to read the commands in the right order */
   result = ed_rec(ed_cmds, in_file, out_file, 0, buffer, BUF_SIZE, 
         hash);
   
   /* read the rest from infile */
   if (result > 0) {
      while (fgets(buffer, BUF_SIZE, in_file) != NULL) {
         written = fwrite(buffer, 1, strlen(buffer), out_file);
         hash->Add((unsigned char*)buffer, written);
      }
   }
   else {
      return ED_FAILURE;
   }
   return ED_OK;
}


bool RredMethod::Fetch(FetchItem *Itm)
{
   Debug = _config->FindB("Debug::pkgAcquire::RRed",false);
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths
   // Path contains the filename to patch
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);
   // Res.Filename the destination filename

   if (Debug == true) 
      std::clog << "Patching " << Path << " with " << Path 
         << ".ed and putting result into " << Itm->DestFile << std::endl;
   // Open the source and destination files (the d'tor of FileFd will do 
   // the cleanup/closing of the fds)
   FileFd From(Path,FileFd::ReadOnly);
   FileFd Patch(Path+".ed",FileFd::ReadOnly);
   FileFd To(Itm->DestFile,FileFd::WriteEmpty);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   Hashes Hash;
   FILE* fFrom = fdopen(From.Fd(), "r");
   FILE* fPatch = fdopen(Patch.Fd(), "r");
   FILE* fTo = fdopen(To.Fd(), "w");
   // now do the actual patching
   if (ed_file(fPatch, fFrom, fTo, &Hash) != ED_OK) {
     _error->Errno("rred", _("Could not patch file"));  
      return false;
   }

   // write out the result
   fflush(fFrom);
   fflush(fPatch);
   fflush(fTo);
   From.Close();
   Patch.Close();
   To.Close();

   // Transfer the modification times
   struct stat Buf;
   if (stat(Path.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   struct utimbuf TimeBuf;
   TimeBuf.actime = Buf.st_atime;
   TimeBuf.modtime = Buf.st_mtime;
   if (utime(Itm->DestFile.c_str(),&TimeBuf) != 0)
      return _error->Errno("utime",_("Failed to set modification time"));

   if (stat(Itm->DestFile.c_str(),&Buf) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   // return done
   Res.LastModified = Buf.st_mtime;
   Res.Size = Buf.st_size;
   Res.TakeHashes(Hash);
   URIDone(Res);

   return true;
}

int main(int argc, char *argv[])
{
   RredMethod Mth;

   Prog = strrchr(argv[0],'/');
   Prog++;
   
   return Mth.Run();
}
