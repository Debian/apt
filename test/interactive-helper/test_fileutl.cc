#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <string>

static void callsystem(std::string const &call)
{
   auto ret = system(call.c_str());
   if (WIFEXITED(ret) == false || WEXITSTATUS(ret) != 0)
      _error->Error("Calling %s failed!", call.c_str());
}

int main(int, char ** argv)
{
	auto const pid = getpid();
	std::string ls;
	strprintf(ls, "ls -l /proc/%d/fd", pid);
	callsystem(ls);
	FileFd t;
	t.Open(argv[1], FileFd::ReadOnly, FileFd::Extension);
	callsystem(ls);
	char buf[1024];
	unsigned long long act;
	while (t.Read(buf, sizeof(buf), &act))
		if (act == 0)
			break;
	callsystem(ls);
	t.Seek(5);
	callsystem(ls);
	t.Close();
	callsystem(ls);
	auto const ret = _error->PendingError();
	_error->DumpErrors();
	return ret;
}
