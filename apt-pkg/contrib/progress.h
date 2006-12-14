// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: progress.h,v 1.6 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   OpProgress - Operation Progress
   
   This class allows lengthy operations to communicate their progress 
   to the GUI. The progress model is simple and is not designed to handle
   the complex case of the multi-activity aquire class.
   
   The model is based on the concept of an overall operation consisting
   of a series of small sub operations. Each sub operation has it's own
   completion status and the overall operation has it's completion status.
   The units of the two are not mixed and are completely independent.
   
   The UI is expected to subclass this to provide the visuals to the user.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PROGRESS_H
#define PKGLIB_PROGRESS_H


#include <string>
#include <sys/time.h>

using std::string;

class Configuration;
class OpProgress
{
   unsigned long Current;
   unsigned long Total;
   unsigned long Size;
   unsigned long SubTotal;
   float LastPercent;
   
   // Change reduction code
   struct timeval LastTime;
   string LastOp;
   string LastSubOp;
   
   protected:
   
   string Op;
   string SubOp;
   float Percent;
   
   bool MajorChange;
   
   bool CheckChange(float Interval = 0.7);		    
   virtual void Update() {};
   
   public:
   
   void Progress(unsigned long Current);
   void SubProgress(unsigned long SubTotal);
   void SubProgress(unsigned long SubTotal,const string &Op);
   void OverallProgress(unsigned long Current,unsigned long Total,
			unsigned long Size,const string &Op);
   virtual void Done() {};
   
   OpProgress();
   virtual ~OpProgress() {};
};

class OpTextProgress : public OpProgress
{
   protected:
   
   string OldOp;
   bool NoUpdate;
   bool NoDisplay;
   unsigned long LastLen;
   virtual void Update();
   void Write(const char *S);
   
   public:

   virtual void Done();
   
   OpTextProgress(bool NoUpdate = false) : NoUpdate(NoUpdate), 
                NoDisplay(false), LastLen(0) {};
   OpTextProgress(Configuration &Config);
   virtual ~OpTextProgress() {Done();};
};

#endif
