// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: progress.h,v 1.1 1998/07/21 05:33:21 jgg Exp $
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
// Header section: pkglib
#ifndef PKGLIB_PROGRESS_H
#define PKGLIB_PROGRESS_H

#ifdef __GNUG__
#pragma interface "apt-pkg/progress.h"
#endif 

#include <string>
#include <sys/time.h>

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
   
   protected:
   
   string Op;
   string SubOp;
   float Percent;
   
   bool MajorChange;
   
   bool CheckChange(float Interval = 0.7);		    
   virtual void Update() {};
   
   public:
   
   void Progress(unsigned long Current);
   void SubProgress(unsigned long SubTotal,string Op);
   void OverallProgress(unsigned long Current,unsigned long Total,
			unsigned long Size,string Op);

   OpProgress();
   virtual ~OpProgress() {};
};

class OpTextProgress : public OpProgress
{
   protected:
   
   string OldOp;
   bool NoUpdate;
   unsigned long LastLen;
   virtual void Update();
   void Write(const char *S);
   
   public:

   void Done();
   
   OpTextProgress(bool NoUpdate = false) : NoUpdate(NoUpdate), LastLen(0) {};
   virtual ~OpTextProgress() {Done();};
};

#endif
