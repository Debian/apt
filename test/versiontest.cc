// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: versiontest.cc,v 1.5 2003/08/18 15:55:19 mdz Exp $
/* ######################################################################

   Version Test - Simple program to run through a file and comare versions.
   
   Each version is compared and the result is checked against an expected
   result in the file. The format of the file is
       a b Res
   Where Res is -1, 1, 0. dpkg -D=1 --compare-versions a "<" b can be
   used to determine what Res should be. # at the start of the line
   is a comment and blank lines are skipped
   
   ##################################################################### */
									/*}}}*/
#include <system.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <apt-pkg/debversion.h>
#include <iostream>
#include <fstream>

using namespace std;

  static int verrevcmp(const char *val, const char *ref) 
{
   int vc, rc;
   long vl, rl;
   const char *vp, *rp;
   
   if (!val) 
      val = "";
   if (!ref) 
      ref = "";
   for (;;) 
   {
      vp = val;  
      while (*vp && !isdigit(*vp)) 
	 vp++;
      rp = ref;  
      while (*rp && !isdigit(*rp)) 
	 rp++;
      for (;;) 
      {
	 vc= val == vp ? 0 : *val++;
	 rc= ref == rp ? 0 : *ref++;
	 if (!rc && !vc)
	    break;
	 if (vc && !isalpha(vc)) 
	    vc += 256; /* assumes ASCII character set */
	 if (rc && !isalpha(rc)) 
	    rc += 256;
	 if (vc != rc) 
	    return vc - rc;
      }
      val = vp;
      ref = rp;
      vl = 0;
      if (isdigit(*vp)) 
	 vl = strtol(val,(char**)&val,10);
      rl = 0;
      if (isdigit(*rp)) 
	 rl = strtol(ref,(char**)&ref,10);
      if (vl != rl) 
	 return vl - rl;
      if (!*val && !*ref) 
	 return 0;
      if (!*val) 
	 return -1;
      if (!*ref) 
	 return +1;
   }
}

#if 0
static int verrevcmp(const char *val, const char *ref) 
{   
   int vc, rc;
   long vl, rl;
   const char *vp, *rp;
   
   if (!val) val= "";
   if (!ref) ref= "";
   for (;;) 
   {
      vp= val;  while (*vp && !isdigit(*vp) && *vp != '~') vp++;
      rp= ref;  while (*rp && !isdigit(*rp) && *rp != '~') rp++;
      for (;;)
      {	 
	 vc= val == vp ? 0 : *val++;
	 rc= ref == rp ? 0 : *ref++;
	 if (!rc && !vc) break;
	 if (vc && !isalpha(vc)) vc += 256; /* assumes ASCII character set */
	 if (rc && !isalpha(rc)) rc += 256;
	 if (vc != rc) return vc - rc;
      }
      
      val= vp;
      ref= rp;
      if (*vp == '~') val++;
      if (*rp == '~') ref++;
      vl=0;  if (isdigit(*val)) vl= strtol(val,(char**)&val,10);
      rl=0;  if (isdigit(*ref)) rl= strtol(ref,(char**)&ref,10);
      if (vl == 0 && rl == 0)
      {
	 if (*vp == '~' && *rp != '~') return -1;
	 if (*vp != '~' && *rp == '~') return +1;
      }
      if (*vp == '~')
	 vl *= -1;
      if (*rp == '~')
	 rl *= -1;
      if (vl != rl) return vl - rl;
      if (!*val && !*ref) return 0;
      if (!*val)
      {
	 if (*ref == '~')
	    return +1;
         else
	    return -1;
      }
      
      if (!*ref)
      {
	 if (*val == '~')
	    return -1;
	 else
	    return +1;
      }      
   }   
}
#endif
    
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
      int Expected = atoi(I);
      int Res = debVS.CmpVersion(A.c_str(), B.c_str());
      int Res2 = verrevcmp(A.c_str(),B.c_str());
      cout << "'" << A << "' ? '" << B << "' = " << Res << " (= " << Expected << ") " << Res2 << endl;

      if (Res < 0)
           Res = -1;
      else if (Res > 0)
           Res = 1;

      if (Res != Expected)
	 _error->Error("Comparison failed on line %u. '%s' ? '%s' %i != %i",CurLine,A.c_str(),B.c_str(),Res,Expected);

      // Check the reverse as well
      Expected = -1*Expected;
      Res = debVS.CmpVersion(B.c_str(), A.c_str());
      Res2 = verrevcmp(B.c_str(),A.c_str());

      cout << "'" << B << "' ? '" << A << "' = " << Res << " (= " << Expected << ") " << Res2 << endl;

      if (Res < 0)
           Res = -1;
      else if (Res > 0)
           Res = 1;

      if (Res != Expected)
	 _error->Error("Comparison failed on line %u. '%s' ? '%s' %i != %i",CurLine,B.c_str(),A.c_str(),Res,Expected);
   }
}

int main(int argc, char *argv[])
{
   if (argc <= 1)
   {
      cerr << "You must specify a test file" << endl;
      return 0;
   }
   
   RunTest(argv[1]);

   // Print any errors or warnings found
   if (_error->empty() == false)
   {
      string Err;
      while (_error->empty() == false)
      {
	 
	 bool Type = _error->PopMessage(Err);
	 if (Type == true)
	    cout << "E: " << Err << endl;
	 else
	    cout << "W: " << Err << endl;
      }
      
      return 0;
   }
}
