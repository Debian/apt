// Includes									/*{{{*/
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
										/*}}}*/
/** \brief RredMethod - ed-style incremential patch method			{{{
 *
 *  This method implements a patch functionality similar to "patch --ed" that is
 *  used by the "tiffany" incremental packages download stuff. It differs from
 *  "ed" insofar that it is way more restricted (and therefore secure).
 *  The currently supported ed commands are "<em>c</em>hange", "<em>a</em>dd" and
 *  "<em>d</em>elete" (diff doesn't output any other).
 *  Additionally the records must be reverse sorted by line number and
 *  may not overlap (diff *seems* to produce this kind of output).
 * */
class RredMethod : public pkgAcqMethod {
	bool Debug;
	// the size of this doesn't really matter (except for performance)
	const static int BUF_SIZE = 1024;
	// the supported ed commands
	enum Mode {MODE_CHANGED='c', MODE_DELETED='d', MODE_ADDED='a'};
	// return values
	enum State {ED_OK=0, ED_ORDERING=1, ED_PARSER=2, ED_FAILURE=3};

	State applyFile(FILE *ed_cmds, FILE *in_file, FILE *out_file,
	             unsigned long &line, char *buffer, Hashes *hash) const;
	void ignoreLineInFile(FILE *fin, char *buffer) const;
	void copyLinesFromFileToFile(FILE *fin, FILE *fout, unsigned int lines,
	                            Hashes *hash, char *buffer) const;

	State patchFile(FILE *ed_cmds, FILE *in_file, FILE *out_file, Hashes *hash) const;

protected:
	// the methods main method
	virtual bool Fetch(FetchItem *Itm);

public:
	RredMethod() : pkgAcqMethod("1.1",SingleInstance | SendConfig) {};
};
										/*}}}*/
/** \brief applyFile - in reverse order with a tail recursion			{{{
 *
 *  As it is expected that the commands are in reversed order in the patch file
 *  we check in the first half if the command is valid, but doesn't execute it
 *  and move a step deeper. After reaching the end of the file we apply the
 *  patches in the correct order: last found command first.
 *
 *  \param ed_cmds patch file to apply
 *  \param in_file base file we want to patch
 *  \param out_file file to write the patched result to
 *  \param line of command operation
 *  \param buffer internal used read/write buffer
 *  \param hash the created file for correctness
 *  \return the success State of the ed command executor
 */
RredMethod::State RredMethod::applyFile(FILE *ed_cmds, FILE *in_file, FILE *out_file,
			unsigned long &line, char *buffer, Hashes *hash) const {
	// get the current command and parse it
	if (fgets(buffer, BUF_SIZE, ed_cmds) == NULL) {
		if (Debug == true)
			std::clog << "rred: encounter end of file - we can start patching now.";
		line = 0;
		return ED_OK;
	}

	// parse in the effected linenumbers
	char* idx;
	errno=0;
	unsigned long const startline = strtol(buffer, &idx, 10);
	if (errno == ERANGE || errno == EINVAL) {
		_error->Errno("rred", "startline is an invalid number");
		return ED_PARSER;
	}
	if (startline > line) {
		_error->Error("rred: The start line (%lu) of the next command is higher than the last line (%lu). This is not allowed.", startline, line);
		return ED_ORDERING;
	}
	unsigned long stopline;
	if (*idx == ',') {
		idx++;
		errno=0;
		stopline = strtol(idx, &idx, 10);
		if (errno == ERANGE || errno == EINVAL) {
			_error->Errno("rred", "stopline is an invalid number");
			return ED_PARSER;
		}
	}
	else {
		stopline = startline;
	}
	line = startline;

	// which command to execute on this line(s)?
	switch (*idx) {
		case MODE_CHANGED:
			if (Debug == true)
				std::clog << "Change from line " << startline << " to " << stopline << std::endl;
			break;
		case MODE_ADDED:
			if (Debug == true)
				std::clog << "Insert after line " << startline << std::endl;
			break;
		case MODE_DELETED:
			if (Debug == true)
				std::clog << "Delete from line " << startline << " to " << stopline << std::endl;
			break;
		default:
			_error->Error("rred: Unknown ed command '%c'. Abort.", *idx);
			return ED_PARSER;
	}
	unsigned char mode = *idx;

	// save the current position
	unsigned const long pos = ftell(ed_cmds);

	// if this is add or change then go to the next full stop
	unsigned int data_length = 0;
	if (mode == MODE_CHANGED || mode == MODE_ADDED) {
		do {
			ignoreLineInFile(ed_cmds, buffer);
			data_length++;
		}
		while (strncmp(buffer, ".", 1) != 0);
		data_length--; // the dot should not be copied
	}

	// do the recursive call - the last command is the one we need to execute at first
	const State child = applyFile(ed_cmds, in_file, out_file, line, buffer, hash);
	if (child != ED_OK) {
		return child;
	}

	// change and delete are working on "line" - add is done after "line"
	if (mode != MODE_ADDED)
		line++;

	// first wind to the current position and copy over all unchanged lines
	if (line < startline) {
		copyLinesFromFileToFile(in_file, out_file, (startline - line), hash, buffer);
		line = startline;
	}

	if (mode != MODE_ADDED)
		line--;

	// include data from ed script
	if (mode == MODE_CHANGED || mode == MODE_ADDED) {
		fseek(ed_cmds, pos, SEEK_SET);
		copyLinesFromFileToFile(ed_cmds, out_file, data_length, hash, buffer);
	}

	// ignore the corresponding number of lines from input
	if (mode == MODE_CHANGED || mode == MODE_DELETED) {
		while (line < stopline) {
			ignoreLineInFile(in_file, buffer);
			line++;
		}
	}
	return ED_OK;
}
										/*}}}*/
void RredMethod::copyLinesFromFileToFile(FILE *fin, FILE *fout, unsigned int lines,/*{{{*/
					Hashes *hash, char *buffer) const {
	while (0 < lines--) {
		do {
			fgets(buffer, BUF_SIZE, fin);
			size_t const written = fwrite(buffer, 1, strlen(buffer), fout);
			hash->Add((unsigned char*)buffer, written);
		} while (strlen(buffer) == (BUF_SIZE - 1) &&
		       buffer[BUF_SIZE - 2] != '\n');
	}
}
										/*}}}*/
void RredMethod::ignoreLineInFile(FILE *fin, char *buffer) const {		/*{{{*/
	fgets(buffer, BUF_SIZE, fin);
	while (strlen(buffer) == (BUF_SIZE - 1) &&
	       buffer[BUF_SIZE - 2] != '\n') {
		fgets(buffer, BUF_SIZE, fin);
		buffer[0] = ' ';
	}
}
										/*}}}*/
RredMethod::State RredMethod::patchFile(FILE *ed_cmds, FILE *in_file, FILE *out_file,		/*{{{*/
      Hashes *hash) const {
   char buffer[BUF_SIZE];
   
   /* we do a tail recursion to read the commands in the right order */
   unsigned long line = -1; // assign highest possible value
   State result = applyFile(ed_cmds, in_file, out_file, line, buffer, hash);
   
   /* read the rest from infile */
   if (result == ED_OK) {
      while (fgets(buffer, BUF_SIZE, in_file) != NULL) {
         size_t const written = fwrite(buffer, 1, strlen(buffer), out_file);
         hash->Add((unsigned char*)buffer, written);
      }
   }
   return result;
}
										/*}}}*/
bool RredMethod::Fetch(FetchItem *Itm)						/*{{{*/
{
   Debug = _config->FindB("Debug::pkgAcquire::RRed", false);
   URI Get = Itm->Uri;
   string Path = Get.Host + Get.Path; // To account for relative paths

   FetchResult Res;
   Res.Filename = Itm->DestFile;
   if (Itm->Uri.empty() == true) {
      Path = Itm->DestFile;
      Itm->DestFile.append(".result");
   } else
      URIStart(Res);

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
   if (patchFile(fPatch, fFrom, fTo, &Hash) != ED_OK) {
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
   if (Itm->Uri.empty() == true) {
      Res.LastModified = Buf.st_mtime;
      Res.Size = Buf.st_size;
      Res.TakeHashes(Hash);
      URIDone(Res);
   }

   return true;
}
										/*}}}*/
/** \brief Wrapper class for testing rred */					/*{{{*/
class TestRredMethod : public RredMethod {
public:
	/** \brief Run rred in debug test mode
	 *
	 *  This method can be used to run the rred method outside
	 *  of the "normal" acquire environment for easier testing.
	 *
	 *  \param base basename of all files involved in this rred test
	 */
	bool Run(char const *base) {
		_config->CndSet("Debug::pkgAcquire::RRed", "true");
		FetchItem *test = new FetchItem;
		test->DestFile = base;
		return Fetch(test);
	}
};
										/*}}}*/
/** \brief Starter for the rred method (or its test method)			{{{
 *
 *  Used without parameters is the normal behavior for methods for
 *  the APT acquire system. While this works great for the acquire system
 *  it is very hard to test the method and therefore the method also
 *  accepts one parameter which will switch it directly to debug test mode:
 *  The test mode expects that if "Testfile" is given as parameter
 *  the file "Testfile" should be ed-style patched with "Testfile.ed"
 *  and will write the result to "Testfile.result".
 */
int main(int argc, char *argv[]) {
	if (argc == 0) {
		RredMethod Mth;
		return Mth.Run();
	} else {
		TestRredMethod Mth;
		bool result = Mth.Run(argv[1]);
		_error->DumpErrors();
		return result;
	}
}
										/*}}}*/
