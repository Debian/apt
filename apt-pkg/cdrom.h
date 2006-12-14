#ifndef PKGLIB_CDROM_H
#define PKGLIB_CDROM_H

#include<apt-pkg/init.h>
#include<string>
#include<vector>


using namespace std;

class pkgCdromStatus
{
 protected:
   int totalSteps;

 public:
   pkgCdromStatus() {};
   virtual ~pkgCdromStatus() {};

   // total steps
   virtual void SetTotal(int total) { totalSteps = total; };
   // update steps, will be called regularly as a "pulse"
   virtual void Update(string text="", int current=0) = 0;
   
   // ask for cdrom insert
   virtual bool ChangeCdrom() = 0;
   // ask for cdrom name
   virtual bool AskCdromName(string &Name) = 0;
   // Progress indicator for the Index rewriter
   virtual OpProgress* GetOpProgress() {return NULL; };
};

class pkgCdrom 
{
 protected:
   enum {
      STEP_PREPARE = 1,
      STEP_UNMOUNT,
      STEP_WAIT,
      STEP_MOUNT,
      STEP_IDENT,
      STEP_SCAN,
      STEP_COPY,
      STEP_WRITE,
      STEP_UNMOUNT3,
      STEP_LAST
   };


   bool FindPackages(string CD,
		     vector<string> &List,
		     vector<string> &SList, 
		     vector<string> &SigList,
		     vector<string> &TransList,
		     string &InfoDir, pkgCdromStatus *log,
		     unsigned int Depth = 0);
   bool DropBinaryArch(vector<string> &List);
   bool DropRepeats(vector<string> &List,const char *Name);
   void ReduceSourcelist(string CD,vector<string> &List);
   bool WriteDatabase(Configuration &Cnf);
   bool WriteSourceList(string Name,vector<string> &List,bool Source);
   int Score(string Path);

 public:
   bool Ident(string &ident, pkgCdromStatus *log);
   bool Add(pkgCdromStatus *log);
};



#endif
