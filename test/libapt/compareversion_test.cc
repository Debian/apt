// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version Test - Simple program to run through a file and comare versions.
   
   Each version is compared and the result is checked against an expected
   result in the file. The format of the file is
       a b Res
   Where Res is -1, 1, 0. dpkg -D=1 --compare-versions a "<" b can be
   used to determine what Res should be. # at the start of the line
   is a comment and blank lines are skipped

   The runner will also call dpkg --compare-versions to check if APT and
   dpkg have (still) the same idea.
   
   ##################################################################### */
									/*}}}*/
#include <apt-pkg/macros.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <apt-pkg/debversion.h>
#include <apt-pkg/fileutl.h>
#include <iostream>
#include <fstream>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

bool callDPkg(const char *val, const char *ref, const char &op) {
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      const char * args[6];
      args[0] = "/usr/bin/dpkg";
      args[1] = "--compare-versions";
      args[2] = val;
      args[3] = (op == 1) ? ">>" : ( (op == 0) ? "=" : "<<");
      args[4] = ref;
      args[5] = 0;
      execv(args[0], (char**) args);
      exit(1);
   }
   int Ret;
   waitpid(Process, &Ret, 0);
   return WIFEXITED(Ret) == true && WEXITSTATUS(Ret) == 0;
}

void assertVersion(int const &CurLine, string const &A, string const &B, int const &Expected) {
   int Res = debVS.CmpVersion(A.c_str(), B.c_str());
   bool const dpkg = callDPkg(A.c_str(),B.c_str(), Expected);
   Res = (Res < 0) ? -1 : ( (Res > 0) ? 1 : Res);

   if (Res != Expected)
      _error->Error("Comparison failed on line %u. '%s' '%s' '%s' %i != %i",CurLine,A.c_str(),((Expected == 1) ? "<<" : ( (Expected == 0) ? "=" : ">>")) ,B.c_str(),Res,Expected);
   if (dpkg == false)
      _error->Error("DPkg differ with line: %u. '%s' '%s' '%s' == false",CurLine,A.c_str(),((Expected == 1) ? "<<" : ( (Expected == 0) ? "=" : ">>")),B.c_str());
}

bool RunTest(const char *File)
{
   ifstream F(File,ios::in);
   if (!F != 0)
      return false;

   char Buffer[300];
   int CurLine = 0;
   
   while (1)
   {
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      if (F.eof() != 0)
	 return true;
      if (!F != 0)
	 return _error->Error("Line %u in %s is too long",CurLine,File);

      // Comment
      if (Buffer[0] == '#' || Buffer[0] == 0)
	 continue;
      
      // First version
      char *I;
      char *Start = Buffer;
      for (I = Buffer; *I != 0 && *I != ' '; I++);
      string A(Start, I - Start);

      if (*I == 0)
	 return _error->Error("Invalid line %u",CurLine);
      
      // Second version
      I++;
      Start = I;
      for (I = Start; *I != 0 && *I != ' '; I++);
      string B(Start,I - Start);
      
      if (*I == 0 || I[1] == 0)
	 return _error->Error("Invalid line %u",CurLine);
      
      // Result
      I++;
      int const Expected = atoi(I);
      assertVersion(CurLine, A, B, Expected);
      // Check the reverse as well
      assertVersion(CurLine, B, A, Expected*-1);
   }
}

int main(int argc, char *argv[])
{
   if (argc <= 1)
      RunTest("../versions.lst");
   else
      RunTest(argv[1]);

   // Print any errors or warnings found
   _error->DumpErrors();
   return 0;
}
