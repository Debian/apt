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

const char *Prog;

class RredMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm);
   
   public:
   
   RredMethod() : pkgAcqMethod("1.1",SingleInstance | SendConfig) {};

};

#define BUF_SIZE	(1024)

// XX use enums
#define MODE_CHANGED	0
#define MODE_DELETED	1
#define MODE_ADDED		2

#define ED_ORDERING		1
#define ED_PARSER		2
#define ED_FAILURE		3

int ed_rec(FILE *ed_cmds, FILE *in_file, FILE *out_file, int line, 
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
	}
	else if (*idx == 'a') {
		mode = MODE_ADDED;
	}
	else if (*idx == 'd') {
		mode = MODE_DELETED;
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

int ed_file(FILE *ed_cmds, FILE *in_file, FILE *out_file, Hashes *hash) {
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
			// XXX add to hash
			// sha_process_bytes(buffer, written, &sha);
		}
	}
	else {
		// XXX better error handling
		fprintf(stderr, "Error: %i\n", result);
		return ED_FAILURE;
	}
	return 0;
}


// XXX do we need modification times as well?
bool RredMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths
   // Path contains the filename to patch
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);
   // Res.Filename the destination filename

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
   ed_file(fPatch, fFrom, fTo, &Hash);

   // XXX need to get the size 
   // Res.Size = Buf.st_size;
   Res.TakeHashes(Hash);
   URIDone(Res);
   
   return true;
}
									/*}}}*/

int main(int argc, char *argv[])
{
   RredMethod Mth;

   Prog = strrchr(argv[0],'/');
   Prog++;
   
   return Mth.Run();
}
