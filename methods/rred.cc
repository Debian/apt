// Includes									/*{{{*/
#include <apt-pkg/fileutl.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>

#include <sys/stat.h>
#include <sys/uio.h>
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
	enum State {ED_OK, ED_ORDERING, ED_PARSER, ED_FAILURE, MMAP_FAILED};

	State applyFile(FILE *ed_cmds, FILE *in_file, FILE *out_file,
	             unsigned long &line, char *buffer, Hashes *hash) const;
	void ignoreLineInFile(FILE *fin, char *buffer) const;
	void copyLinesFromFileToFile(FILE *fin, FILE *fout, unsigned int lines,
	                            Hashes *hash, char *buffer) const;

	State patchFile(FileFd &Patch, FileFd &From, FileFd &out_file, Hashes *hash) const;
	State patchMMap(FileFd &Patch, FileFd &From, FileFd &out_file, Hashes *hash) const;

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
			std::clog << "rred: encounter end of file - we can start patching now." << std::endl;
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
RredMethod::State RredMethod::patchFile(FileFd &Patch, FileFd &From,		/*{{{*/
					FileFd &out_file, Hashes *hash) const {
   char buffer[BUF_SIZE];
   FILE* fFrom = fdopen(From.Fd(), "r");
   FILE* fPatch = fdopen(Patch.Fd(), "r");
   FILE* fTo = fdopen(out_file.Fd(), "w");

   /* we do a tail recursion to read the commands in the right order */
   unsigned long line = -1; // assign highest possible value
   State const result = applyFile(fPatch, fFrom, fTo, line, buffer, hash);
   
   /* read the rest from infile */
   if (result == ED_OK) {
      while (fgets(buffer, BUF_SIZE, fFrom) != NULL) {
         size_t const written = fwrite(buffer, 1, strlen(buffer), fTo);
         hash->Add((unsigned char*)buffer, written);
      }
      fflush(fTo);
   }
   return result;
}
										/*}}}*/
struct EdCommand {								/*{{{*/
  size_t data_start;
  size_t data_end;
  size_t data_lines;
  size_t first_line;
  size_t last_line;
  char type;
};
#define IOV_COUNT 1024 /* Don't really want IOV_MAX since it can be arbitrarily large */
										/*}}}*/
RredMethod::State RredMethod::patchMMap(FileFd &Patch, FileFd &From,		/*{{{*/
					FileFd &out_file, Hashes *hash) const {
#ifdef _POSIX_MAPPED_FILES
	MMap ed_cmds(Patch, MMap::ReadOnly);
	MMap in_file(From, MMap::ReadOnly);

	if (ed_cmds.Size() == 0 || in_file.Size() == 0)
		return MMAP_FAILED;

	EdCommand* commands = 0;
	size_t command_count = 0;
	size_t command_alloc = 0;

	const char* begin = (char*) ed_cmds.Data();
	const char* end = begin;
	const char* ed_end = (char*) ed_cmds.Data() + ed_cmds.Size();

	const char* input = (char*) in_file.Data();
	const char* input_end = (char*) in_file.Data() + in_file.Size();

	size_t i;

	/* 1. Parse entire script.  It is executed in reverse order, so we cather it
	 *    in the `commands' buffer first
	 */

	for(;;) {
		EdCommand cmd;
		cmd.data_start = 0;
		cmd.data_end = 0;

		while(begin != ed_end && *begin == '\n')
			++begin;
		while(end != ed_end && *end != '\n')
			++end;
		if(end == ed_end && begin == end)
			break;

		/* Determine command range */
		const char* tmp = begin;

		for(;;) {
			/* atoll is safe despite lacking NUL-termination; we know there's an
			 * alphabetic character at end[-1]
			 */
			if(tmp == end) {
				cmd.first_line = atol(begin);
				cmd.last_line = cmd.first_line;
				break;
			}
			if(*tmp == ',') {
				cmd.first_line = atol(begin);
				cmd.last_line = atol(tmp + 1);
				break;
			}
			++tmp;
		}

		// which command to execute on this line(s)?
		switch (end[-1]) {
			case MODE_CHANGED:
				if (Debug == true)
					std::clog << "Change from line " << cmd.first_line << " to " << cmd.last_line << std::endl;
				break;
			case MODE_ADDED:
				if (Debug == true)
					std::clog << "Insert after line " << cmd.first_line << std::endl;
				break;
			case MODE_DELETED:
				if (Debug == true)
					std::clog << "Delete from line " << cmd.first_line << " to " << cmd.last_line << std::endl;
				break;
			default:
				_error->Error("rred: Unknown ed command '%c'. Abort.", end[-1]);
				free(commands);
				return ED_PARSER;
		}
		cmd.type = end[-1];

		/* Determine the size of the inserted text, so we don't have to scan this
		 * text again later.
		 */
		begin = end + 1;
		end = begin;
		cmd.data_lines = 0;

		if(cmd.type == MODE_ADDED || cmd.type == MODE_CHANGED) {
			cmd.data_start = begin - (char*) ed_cmds.Data();
			while(end != ed_end) {
				if(*end == '\n') {
					if(end[-1] == '.' && end[-2] == '\n')
						break;
					++cmd.data_lines;
				}
				++end;
			}
			cmd.data_end = end - (char*) ed_cmds.Data() - 1;
			begin = end + 1;
			end = begin;
		}
		if(command_count == command_alloc) {
			command_alloc = (command_alloc + 64) * 3 / 2;
			commands = (EdCommand*) realloc(commands, command_alloc * sizeof(EdCommand));
		}
		commands[command_count++] = cmd;
	}

	struct iovec* iov = new struct iovec[IOV_COUNT];
	size_t iov_size = 0;

	size_t amount, remaining;
	size_t line = 1;
	EdCommand* cmd;

	/* 2. Execute script.  We gather writes in a `struct iov' array, and flush
	 *    using writev to minimize the number of system calls.  Data is read
	 *    directly from the memory mappings of the input file and the script.
	 */

	for(i = command_count; i-- > 0; ) {
		cmd = &commands[i];
		if(cmd->type == MODE_ADDED)
			amount = cmd->first_line + 1;
		else
			amount = cmd->first_line;

		if(line < amount) {
			begin = input;
			while(line != amount) {
				input = (const char*) memchr(input, '\n', input_end - input);
				if(!input)
					break;
				++line;
				++input;
			}

			iov[iov_size].iov_base = (void*) begin;
			iov[iov_size].iov_len = input - begin;
			hash->Add((const unsigned char*) begin, input - begin);

			if(++iov_size == IOV_COUNT) {
				writev(out_file.Fd(), iov, IOV_COUNT);
				iov_size = 0;
			}
		}

		if(cmd->type == MODE_DELETED || cmd->type == MODE_CHANGED) {
			remaining = (cmd->last_line - cmd->first_line) + 1;
			line += remaining;
			while(remaining) {
				input = (const char*) memchr(input, '\n', input_end - input);
				if(!input)
					break;
				--remaining;
				++input;
			}
		}

		if(cmd->type == MODE_CHANGED || cmd->type == MODE_ADDED) {
			if(cmd->data_end != cmd->data_start) {
				iov[iov_size].iov_base = (void*) ((char*)ed_cmds.Data() + cmd->data_start);
				iov[iov_size].iov_len = cmd->data_end - cmd->data_start;
				hash->Add((const unsigned char*) ((char*)ed_cmds.Data() + cmd->data_start),
				iov[iov_size].iov_len);

				if(++iov_size == IOV_COUNT) {
					writev(out_file.Fd(), iov, IOV_COUNT);
					iov_size = 0;
				}
			}
		}
	}

	if(input != input_end) {
		iov[iov_size].iov_base = (void*) input;
		iov[iov_size].iov_len = input_end - input;
		hash->Add((const unsigned char*) input, input_end - input);
		++iov_size;
	}

	if(iov_size) {
		writev(out_file.Fd(), iov, iov_size);
		iov_size = 0;
	}

	for(i = 0; i < iov_size; i += IOV_COUNT) {
		if(iov_size - i < IOV_COUNT)
			writev(out_file.Fd(), iov + i, iov_size - i);
		else
			writev(out_file.Fd(), iov + i, IOV_COUNT);
	}

	delete [] iov;
	free(commands);

	return ED_OK;
#else
	return MMAP_FAILED;
#endif
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
   FileFd To(Itm->DestFile,FileFd::WriteAtomic);   
   To.EraseOnFailure();
   if (_error->PendingError() == true)
      return false;
   
   Hashes Hash;
   // now do the actual patching
   State const result = patchMMap(Patch, From, To, &Hash);
   if (result == MMAP_FAILED) {
      // retry with patchFile
      lseek(Patch.Fd(), 0, SEEK_SET);
      lseek(From.Fd(), 0, SEEK_SET);
      To.Open(Itm->DestFile,FileFd::WriteAtomic);
      if (_error->PendingError() == true)
         return false;
      if (patchFile(Patch, From, To, &Hash) != ED_OK) {
	 return _error->WarningE("rred", _("Could not patch %s with mmap and with file operation usage - the patch seems to be corrupt."), Path.c_str());
      } else if (Debug == true) {
	 std::clog << "rred: finished file patching of " << Path  << " after mmap failed." << std::endl;
      }
   } else if (result != ED_OK) {
      return _error->Errno("rred", _("Could not patch %s with mmap (but no mmap specific fail) - the patch seems to be corrupt."), Path.c_str());
   } else if (Debug == true) {
      std::clog << "rred: finished mmap patching of " << Path << std::endl;
   }

   // write out the result
   From.Close();
   Patch.Close();
   To.Close();

   /* Transfer the modification times from the patch file
      to be able to see in which state the file should be
      and use the access time from the "old" file */
   struct stat BufBase, BufPatch;
   if (stat(Path.c_str(),&BufBase) != 0 ||
       stat(string(Path+".ed").c_str(),&BufPatch) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   struct utimbuf TimeBuf;
   TimeBuf.actime = BufBase.st_atime;
   TimeBuf.modtime = BufPatch.st_mtime;
   if (utime(Itm->DestFile.c_str(),&TimeBuf) != 0)
      return _error->Errno("utime",_("Failed to set modification time"));

   if (stat(Itm->DestFile.c_str(),&BufBase) != 0)
      return _error->Errno("stat",_("Failed to stat"));

   // return done
   Res.LastModified = BufBase.st_mtime;
   Res.Size = BufBase.st_size;
   Res.TakeHashes(Hash);
   URIDone(Res);

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
	if (argc <= 1) {
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
